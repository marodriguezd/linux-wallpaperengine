#include "we/display/display.hpp"

#include "we/core/log.hpp"

#include <wayland-client.h>
#include <wayland-egl.h>

#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#include <poll.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace we::display {
namespace {

class WaylandOutput {
public:
    WaylandOutput (wl_output* output, std::uint32_t id) : output_ (output), id_ (id) { }

    ~WaylandOutput () {
        if (this->output_ != nullptr) {
            wl_output_destroy (this->output_);
        }
    }

    WaylandOutput (const WaylandOutput&) = delete;
    WaylandOutput& operator= (const WaylandOutput&) = delete;

    [[nodiscard]] wl_output* handle () const noexcept { return this->output_; }
    [[nodiscard]] std::uint32_t id () const noexcept { return this->id_; }

    [[nodiscard]] const Output& description () const noexcept { return this->description_; }
    [[nodiscard]] Output& description () noexcept { return this->description_; }
    [[nodiscard]] bool ready () const noexcept { return this->ready_; }

    static void on_geometry (void* data, wl_output* output, int32_t x, int32_t y, int32_t physical_width,
                             int32_t physical_height, int32_t subpixel, const char* make, const char* model,
                             int32_t transform) {
        auto* self = static_cast<WaylandOutput*> (data);
        (void)output;
        (void)physical_width;
        (void)physical_height;
        (void)subpixel;
        (void)transform;
        self->description_.x = x;
        self->description_.y = y;
        self->description_.name = std::string (make != nullptr ? make : "") + " "
            + std::string (model != nullptr ? model : "");
        self->description_.name = self->description_.name.empty () ? "wayland-" + std::to_string (self->id_)
                                                                   : self->description_.name;
    }

    static void on_mode (void* data, wl_output* output, uint32_t flags, int32_t width, int32_t height,
                         int32_t refresh) {
        auto* self = static_cast<WaylandOutput*> (data);
        (void)output;

        if ((flags & WL_OUTPUT_MODE_CURRENT) != 0) {
            self->description_.width = width;
            self->description_.height = height;
            self->description_.refresh_hz = static_cast<float> (refresh) / 1000.0F;
        }
    }

    static void on_done (void* data, wl_output* output) {
        auto* self = static_cast<WaylandOutput*> (data);
        (void)output;
        self->ready_ = true;
    }

    static void on_scale (void* data, wl_output* output, int32_t factor) {
        auto* self = static_cast<WaylandOutput*> (data);
        (void)output;
        self->description_.scale = factor;
    }

    static void on_name (void* data, wl_output* output, const char* name) {
        auto* self = static_cast<WaylandOutput*> (data);
        (void)output;

        if (name != nullptr && *name != '\0') {
            self->description_.name = name;
        }
    }

    static void on_description (void* data, wl_output* output, const char* description) {
        auto* self = static_cast<WaylandOutput*> (data);
        (void)output;
        (void)description;
        (void)self;
    }

private:
    wl_output* output_ = nullptr;
    std::uint32_t id_ = 0;
    Output description_;
    bool ready_ = false;
};

constexpr wl_output_listener output_listener = {
    .geometry = WaylandOutput::on_geometry,
    .mode = WaylandOutput::on_mode,
    .done = WaylandOutput::on_done,
    .scale = WaylandOutput::on_scale,
    .name = WaylandOutput::on_name,
    .description = WaylandOutput::on_description,
};

class WaylandLayerSurface final : public Surface {
public:
    WaylandLayerSurface (gfx::Context& context, wl_display* display, wl_compositor* compositor,
                         zwlr_layer_shell_v1* shell, const Output& output, bool vsync) :
        context_ (context), display_ (display), output_ (output), vsync_ (vsync) {
        this->surface_ = wl_compositor_create_surface (compositor);

        if (this->surface_ == nullptr) {
            core::log_error ("wl_compositor.create_surface failed");
            return;
        }

        this->layer_ = zwlr_layer_shell_v1_get_layer_surface (shell, this->surface_, nullptr,
                                                              ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND,
                                                              "linux-wallpaperengine");

        if (this->layer_ == nullptr) {
            core::log_error ("zwlr_layer_shell_v1.get_layer_surface failed");
            return;
        }

        zwlr_layer_surface_v1_add_listener (this->layer_, &this->layer_listener_, this);

        zwlr_layer_surface_v1_set_anchor (this->layer_, ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP
                                                           | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM
                                                           | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT
                                                           | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
        zwlr_layer_surface_v1_set_exclusive_zone (this->layer_, -1);
        // Protocol v1 exposes keyboard interactivity as a uint (0 = none).
        zwlr_layer_surface_v1_set_keyboard_interactivity (this->layer_, 0);
        zwlr_layer_surface_v1_set_size (this->layer_, 0, 0);

        wl_surface_commit (this->surface_);

        if (!this->wait_for_configure ()) {
            core::log_error ("Wayland layer surface was never configured");
            return;
        }

        const int width = this->configured_width_ > 0 ? this->configured_width_ : this->output_.width;
        const int height = this->configured_height_ > 0 ? this->configured_height_ : this->output_.height;
        const int scale = std::max (this->output_.scale, 1);

        wl_surface_set_buffer_scale (this->surface_, scale);
        this->egl_window_ = wl_egl_window_create (this->surface_, width * scale, height * scale);

        if (this->egl_window_ == nullptr) {
            core::log_error ("wl_egl_window_create failed");
            return;
        }

        auto egl_surface = context.create_window_surface (this->egl_window_, width * scale, height * scale);

        if (!egl_surface) {
            core::log_error ("cannot create the Wayland EGL surface: ", egl_surface.error ().message);
            return;
        }

        this->egl_surface_ = std::move (*egl_surface);
        this->valid_ = true;
    }

    ~WaylandLayerSurface () override {
        if (this->egl_window_ != nullptr) {
            wl_egl_window_destroy (this->egl_window_);
        }

        if (this->layer_ != nullptr) {
            zwlr_layer_surface_v1_destroy (this->layer_);
        }

        if (this->surface_ != nullptr) {
            wl_surface_destroy (this->surface_);
        }
    }

    [[nodiscard]] bool valid () const noexcept override { return this->valid_; }

    [[nodiscard]] core::Extent size () const noexcept override {
        return { this->egl_surface_.width (), this->egl_surface_.height () };
    }

    void make_current () override { this->context_.make_current (&this->egl_surface_); }

    void swap () override {
        this->make_current ();
        this->egl_surface_.swap ();
        wl_surface_damage_buffer (this->surface_, 0, 0, this->egl_surface_.width (), this->egl_surface_.height ());
        wl_surface_commit (this->surface_);
        wl_display_flush (this->display_);
    }

    void set_vsync (bool enabled) override {
        this->vsync_ = enabled;
        this->context_.set_swap_interval (enabled ? 1 : 0);
    }

    void resize_from_configure (std::uint32_t width, std::uint32_t height) {
        if (this->egl_window_ == nullptr) {
            this->configured_width_ = static_cast<int> (width);
            this->configured_height_ = static_cast<int> (height);
            return;
        }

        const int scale = std::max (this->output_.scale, 1);
        const int pixel_width = static_cast<int> (width > 0 ? width : this->configured_width_) * scale;
        const int pixel_height = static_cast<int> (height > 0 ? height : this->configured_height_) * scale;
        wl_egl_window_resize (this->egl_window_, pixel_width, pixel_height, 0, 0);
        this->egl_surface_.resize (pixel_width, pixel_height);
    }

private:
    [[nodiscard]] bool wait_for_configure () {
        wl_display_flush (this->display_);

        for (int attempt = 0; attempt < 200 && !this->configured_; ++attempt) {
            pollfd descriptor { .fd = wl_display_get_fd (this->display_), .events = POLLIN, .revents = 0 };

            if (poll (&descriptor, 1, 10) <= 0) {
                continue;
            }

            if (wl_display_dispatch (this->display_) < 0) {
                return false;
            }
        }

        return this->configured_;
    }

    static void on_configure (void* data, zwlr_layer_surface_v1* layer, std::uint32_t serial,
                              std::uint32_t width, std::uint32_t height) {
        auto* self = static_cast<WaylandLayerSurface*> (data);
        zwlr_layer_surface_v1_ack_configure (layer, serial);
        self->configured_ = true;
        self->resize_from_configure (width, height);
    }

    static void on_closed (void* data, zwlr_layer_surface_v1* layer) {
        (void)layer;
        static_cast<WaylandLayerSurface*> (data)->closed_ = true;
    }

    static constexpr zwlr_layer_surface_v1_listener layer_listener_ = {
        .configure = on_configure,
        .closed = on_closed,
    };

    gfx::Context& context_;
    wl_display* display_ = nullptr;
    Output output_;
    wl_surface* surface_ = nullptr;
    zwlr_layer_surface_v1* layer_ = nullptr;
    wl_egl_window* egl_window_ = nullptr;
    gfx::Surface egl_surface_;
    int configured_width_ = 0;
    int configured_height_ = 0;
    bool configured_ = false;
    bool valid_ = false;
    bool vsync_ = true;
    bool closed_ = false;
};

class WaylandDriver final : public Driver {
public:
    explicit WaylandDriver (bool vsync) : vsync_ (vsync) {
        this->display_ = wl_display_connect (nullptr);

        if (this->display_ == nullptr) {
            return;
        }

        this->registry_ = wl_display_get_registry (this->display_);
        wl_registry_add_listener (this->registry_, &this->registry_listener_, this);

        // First roundtrip binds the globals, the second delivers output
        // geometry/mode events for every wl_output.
        wl_display_roundtrip (this->display_);
        wl_display_roundtrip (this->display_);
    }

    ~WaylandDriver () override {
        this->outputs_.clear ();

        if (this->layer_shell_ != nullptr) {
            zwlr_layer_shell_v1_destroy (this->layer_shell_);
        }

        if (this->compositor_ != nullptr) {
            wl_compositor_destroy (this->compositor_);
        }

        if (this->registry_ != nullptr) {
            wl_registry_destroy (this->registry_);
        }

        if (this->display_ != nullptr) {
            wl_display_disconnect (this->display_);
        }
    }

    [[nodiscard]] bool valid () const noexcept { return this->display_ != nullptr && this->compositor_ != nullptr; }
    [[nodiscard]] bool has_layer_shell () const noexcept { return this->layer_shell_ != nullptr; }

    [[nodiscard]] Backend backend () const noexcept override { return Backend::wayland; }
    [[nodiscard]] std::string_view name () const noexcept override { return "wayland-layer"; }

    [[nodiscard]] std::span<const Output> outputs () const noexcept override {
        this->output_descriptions_.clear ();

        for (const auto& output : this->outputs_) {
            this->output_descriptions_.push_back (output->description ());
        }

        return this->output_descriptions_;
    }

    std::expected<std::unique_ptr<gfx::Context>, core::Error> create_context () override {
        auto context = gfx::Context::create_wayland (this->display_);

        if (!context) {
            return std::unexpected (std::move (context.error ()));
        }

        context->set_swap_interval (this->vsync_ ? 1 : 0);
        return std::make_unique<gfx::Context> (std::move (*context));
    }

    std::expected<std::unique_ptr<Surface>, core::Error>
    create_surface (std::size_t output_index, gfx::Context& context) override {
        if (!this->has_layer_shell ()) {
            return core::fail (core::ErrorCode::unsupported,
                               "the compositor does not expose zwlr_layer_shell_v1");
        }

        if (output_index >= this->outputs_.size ()) {
            return core::fail (core::ErrorCode::out_of_range, "Wayland output index out of range");
        }

        auto surface = std::make_unique<WaylandLayerSurface> (
            context, this->display_, this->compositor_, this->layer_shell_,
            this->outputs_[output_index]->description (), this->vsync_
        );

        if (!surface->valid ()) {
            return core::fail (core::ErrorCode::io, "cannot create the Wayland layer surface");
        }

        return surface;
    }

    void pump () override {
        if (this->display_ == nullptr) {
            return;
        }

        while (wl_display_prepare_read (this->display_) == 0) {
            wl_display_flush (this->display_);
            pollfd descriptor { .fd = wl_display_get_fd (this->display_), .events = POLLIN, .revents = 0 };

            if (poll (&descriptor, 1, 0) <= 0 || (descriptor.revents & POLLIN) == 0) {
                wl_display_cancel_read (this->display_);
                break;
            }

            if (wl_display_read_events (this->display_) < 0) {
                break;
            }
        }

        wl_display_dispatch_pending (this->display_);
        wl_display_flush (this->display_);
    }

    [[nodiscard]] bool should_close () const noexcept override { return this->closed_; }

private:
    static void on_registry_global (void* data, wl_registry* registry, std::uint32_t name,
                                    const char* interface, std::uint32_t version) {
        auto* self = static_cast<WaylandDriver*> (data);

        if (std::strcmp (interface, wl_compositor_interface.name) == 0) {
            self->compositor_ = static_cast<wl_compositor*> (
                wl_registry_bind (registry, name, &wl_compositor_interface, std::min (version, 4U))
            );
        } else if (std::strcmp (interface, zwlr_layer_shell_v1_interface.name) == 0) {
            self->layer_shell_ = static_cast<zwlr_layer_shell_v1*> (
                wl_registry_bind (registry, name, &zwlr_layer_shell_v1_interface, std::min (version, 4U))
            );
        } else if (std::strcmp (interface, wl_output_interface.name) == 0) {
            auto* output = static_cast<wl_output*> (
                wl_registry_bind (registry, name, &wl_output_interface, std::min (version, 4U))
            );
            auto owned = std::make_unique<WaylandOutput> (output, name);
            wl_output_add_listener (output, &output_listener, owned.get ());
            self->outputs_.push_back (std::move (owned));
        }
    }

    static void on_registry_remove (void* data, wl_registry* registry, std::uint32_t name) {
        auto* self = static_cast<WaylandDriver*> (data);
        (void)registry;
        std::erase_if (self->outputs_, [name] (const auto& output) { return output->id () == name; });
    }

    static constexpr wl_registry_listener registry_listener_ = {
        .global = on_registry_global,
        .global_remove = on_registry_remove,
    };

    wl_display* display_ = nullptr;
    wl_registry* registry_ = nullptr;
    wl_compositor* compositor_ = nullptr;
    zwlr_layer_shell_v1* layer_shell_ = nullptr;
    std::vector<std::unique_ptr<WaylandOutput>> outputs_;
    mutable std::vector<Output> output_descriptions_;
    bool vsync_ = true;
    bool closed_ = false;
};

} // namespace

std::expected<std::unique_ptr<Driver>, core::Error> create_wayland_driver (const DriverOptions& options) {
    auto driver = std::make_unique<WaylandDriver> (options.vsync);

    if (!driver->valid ()) {
        return core::fail (core::ErrorCode::io, "cannot connect to the Wayland compositor");
    }

    if (driver->outputs ().empty ()) {
        return core::fail (core::ErrorCode::io, "the Wayland compositor reported no outputs");
    }

    return driver;
}

} // namespace we::display
