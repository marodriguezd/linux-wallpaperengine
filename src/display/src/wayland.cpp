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
#include <optional>
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

	static void on_geometry (
	    void* data, wl_output* output, int32_t x, int32_t y, int32_t physical_width, int32_t physical_height,
	    int32_t subpixel, const char* make, const char* model, int32_t transform
	) {
	    auto* self = static_cast<WaylandOutput*> (data);
	    (void)output;
	    (void)physical_width;
	    (void)physical_height;
	    (void)subpixel;
	    (void)transform;
	    self->description_.x = x;
	    self->description_.y = y;
	    self->description_.name
		= std::string (make != nullptr ? make : "") + " " + std::string (model != nullptr ? model : "");
	    self->description_.name
		= self->description_.name.empty () ? "wayland-" + std::to_string (self->id_) : self->description_.name;
	}

	static void
	on_mode (void* data, wl_output* output, uint32_t flags, int32_t width, int32_t height, int32_t refresh) {
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
	    self->description_.scale = std::max (factor, 1);
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

    class WaylandLayerSurface;

    class WaylandDriver final : public Driver {
    public:
	explicit WaylandDriver (bool vsync);
	~WaylandDriver () override;

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
	create_surface (std::size_t output_index, gfx::Context& context) override;

	void pump () override;

	[[nodiscard]] bool should_close () const noexcept override { return this->closed_; }

	[[nodiscard]] std::optional<core::Vec2> cursor_position () const override { return this->cursor_position_; }

	void register_surface (wl_surface* surface, const Output* output) {
	    if (surface == nullptr) {
		return;
	    }
	    this->surface_bindings_.push_back (SurfaceBinding { .surface = surface, .output = output });
	}

	void unregister_surface (wl_surface* surface) {
	    if (surface == nullptr) {
		return;
	    }
	    std::erase_if (this->surface_bindings_, [surface] (const auto& binding) {
		return binding.surface == surface;
	    });
	    if (this->active_pointer_surface_ == surface) {
		this->active_pointer_surface_ = nullptr;
		this->active_pointer_output_ = nullptr;
		this->cursor_position_ = std::nullopt;
	    }
	}

	void notify_surface_closed () noexcept { this->closed_ = true; }

    private:
	struct SurfaceBinding {
	    wl_surface* surface = nullptr;
	    const Output* output = nullptr;
	};

	void destroy_pointer () noexcept {
	    if (this->pointer_ != nullptr) {
		if (wl_pointer_get_version (this->pointer_) >= WL_POINTER_RELEASE_SINCE_VERSION) {
		    wl_pointer_release (this->pointer_);
		} else {
		    wl_pointer_destroy (this->pointer_);
		}
		this->pointer_ = nullptr;
	    }
	    this->active_pointer_surface_ = nullptr;
	    this->active_pointer_output_ = nullptr;
	    this->cursor_position_ = std::nullopt;
	}

	void destroy_seat () noexcept {
	    this->destroy_pointer ();
	    if (this->seat_ != nullptr) {
		if (wl_seat_get_version (this->seat_) >= WL_SEAT_RELEASE_SINCE_VERSION) {
		    wl_seat_release (this->seat_);
		} else {
		    wl_seat_destroy (this->seat_);
		}
		this->seat_ = nullptr;
		this->seat_id_ = 0;
	    }
	}

	void update_cursor_position (wl_fixed_t surface_x, wl_fixed_t surface_y) noexcept {
	    const double sx = wl_fixed_to_double (surface_x);
	    const double sy = wl_fixed_to_double (surface_y);
	    const float ox
		= this->active_pointer_output_ != nullptr ? static_cast<float> (this->active_pointer_output_->x) : 0.0F;
	    const float oy
		= this->active_pointer_output_ != nullptr ? static_cast<float> (this->active_pointer_output_->y) : 0.0F;

	    this->cursor_position_ = core::Vec2 {
		ox + static_cast<float> (sx),
		oy + static_cast<float> (sy),
	    };
	}

	void handle_pointer_enter (wl_surface* surface, wl_fixed_t surface_x, wl_fixed_t surface_y) noexcept {
	    this->active_pointer_surface_ = surface;
	    this->active_pointer_output_ = nullptr;
	    for (const auto& binding : this->surface_bindings_) {
		if (binding.surface == surface) {
		    this->active_pointer_output_ = binding.output;
		    break;
		}
	    }
	    this->update_cursor_position (surface_x, surface_y);
	}

	void handle_pointer_leave (wl_surface* surface) noexcept {
	    if (this->active_pointer_surface_ == surface) {
		this->active_pointer_surface_ = nullptr;
		this->active_pointer_output_ = nullptr;
		this->cursor_position_ = std::nullopt;
	    }
	}

	void handle_pointer_motion (wl_fixed_t surface_x, wl_fixed_t surface_y) noexcept {
	    this->update_cursor_position (surface_x, surface_y);
	}

	static void on_registry_global (
	    void* data, wl_registry* registry, std::uint32_t name, const char* interface, std::uint32_t version
	) {
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
	    } else if (std::strcmp (interface, wl_seat_interface.name) == 0) {
		if (self->seat_ == nullptr) {
		    self->seat_id_ = name;
		    self->seat_ = static_cast<wl_seat*> (
			wl_registry_bind (registry, name, &wl_seat_interface, std::min (version, 7U))
		    );
		    wl_seat_add_listener (self->seat_, &seat_listener_, self);
		}
	    }
	}

	static void on_registry_remove (void* data, wl_registry* registry, std::uint32_t name) {
	    auto* self = static_cast<WaylandDriver*> (data);
	    (void)registry;
	    if (self->seat_id_ == name) {
		self->destroy_seat ();
	    }
	    std::erase_if (self->outputs_, [name] (const auto& output) { return output->id () == name; });
	}

	static constexpr wl_registry_listener registry_listener_ = {
	    .global = on_registry_global,
	    .global_remove = on_registry_remove,
	};

	static void on_seat_capabilities (void* data, wl_seat* seat, std::uint32_t capabilities) {
	    auto* self = static_cast<WaylandDriver*> (data);
	    const bool has_pointer = (capabilities & WL_SEAT_CAPABILITY_POINTER) != 0;

	    if (has_pointer && self->pointer_ == nullptr) {
		self->pointer_ = wl_seat_get_pointer (seat);
		wl_pointer_add_listener (self->pointer_, &pointer_listener_, self);
	    } else if (!has_pointer && self->pointer_ != nullptr) {
		self->destroy_pointer ();
	    }
	}

	static void on_seat_name (void* data, wl_seat* seat, const char* name) {
	    (void)data;
	    (void)seat;
	    (void)name;
	}

	static constexpr wl_seat_listener seat_listener_ = {
	    .capabilities = on_seat_capabilities,
	    .name = on_seat_name,
	};

	static void on_pointer_enter (
	    void* data, wl_pointer* pointer, uint32_t serial, wl_surface* surface, wl_fixed_t surface_x,
	    wl_fixed_t surface_y
	) {
	    (void)pointer;
	    (void)serial;
	    auto* self = static_cast<WaylandDriver*> (data);
	    self->handle_pointer_enter (surface, surface_x, surface_y);
	}

	static void on_pointer_leave (void* data, wl_pointer* pointer, uint32_t serial, wl_surface* surface) {
	    (void)pointer;
	    (void)serial;
	    auto* self = static_cast<WaylandDriver*> (data);
	    self->handle_pointer_leave (surface);
	}

	static void
	on_pointer_motion (void* data, wl_pointer* pointer, uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y) {
	    (void)pointer;
	    (void)time;
	    auto* self = static_cast<WaylandDriver*> (data);
	    self->handle_pointer_motion (surface_x, surface_y);
	}

	static void on_pointer_button (void*, wl_pointer*, uint32_t, uint32_t, uint32_t, uint32_t) { }
	static void on_pointer_axis (void*, wl_pointer*, uint32_t, uint32_t, wl_fixed_t) { }
	static void on_pointer_frame (void*, wl_pointer*) { }
	static void on_pointer_axis_source (void*, wl_pointer*, uint32_t) { }
	static void on_pointer_axis_stop (void*, wl_pointer*, uint32_t, uint32_t) { }
	static void on_pointer_axis_discrete (void*, wl_pointer*, uint32_t, int32_t) { }
#ifdef WL_POINTER_AXIS_VALUE120_SINCE_VERSION
	static void on_pointer_axis_value120 (void*, wl_pointer*, uint32_t, int32_t) { }
#endif
#ifdef WL_POINTER_AXIS_RELATIVE_DIRECTION_SINCE_VERSION
	static void on_pointer_axis_relative_direction (void*, wl_pointer*, uint32_t, uint32_t) { }
#endif
#ifdef WL_POINTER_WARP_SINCE_VERSION
	static void on_pointer_warp (void*, wl_pointer*, wl_fixed_t, wl_fixed_t) { }
#endif

	static constexpr wl_pointer_listener pointer_listener_ = {
	    .enter = on_pointer_enter,
	    .leave = on_pointer_leave,
	    .motion = on_pointer_motion,
	    .button = on_pointer_button,
	    .axis = on_pointer_axis,
	    .frame = on_pointer_frame,
	    .axis_source = on_pointer_axis_source,
	    .axis_stop = on_pointer_axis_stop,
	    .axis_discrete = on_pointer_axis_discrete,
#ifdef WL_POINTER_AXIS_VALUE120_SINCE_VERSION
	    .axis_value120 = on_pointer_axis_value120,
#endif
#ifdef WL_POINTER_AXIS_RELATIVE_DIRECTION_SINCE_VERSION
	    .axis_relative_direction = on_pointer_axis_relative_direction,
#endif
#ifdef WL_POINTER_WARP_SINCE_VERSION
	    .warp = on_pointer_warp,
#endif
	};

	wl_display* display_ = nullptr;
	wl_registry* registry_ = nullptr;
	wl_compositor* compositor_ = nullptr;
	zwlr_layer_shell_v1* layer_shell_ = nullptr;
	wl_seat* seat_ = nullptr;
	std::uint32_t seat_id_ = 0;
	wl_pointer* pointer_ = nullptr;
	wl_surface* active_pointer_surface_ = nullptr;
	const Output* active_pointer_output_ = nullptr;
	std::optional<core::Vec2> cursor_position_;

	std::vector<SurfaceBinding> surface_bindings_;
	std::vector<std::unique_ptr<WaylandOutput>> outputs_;
	mutable std::vector<Output> output_descriptions_;
	bool vsync_ = true;
	bool closed_ = false;
    };

    class WaylandLayerSurface final : public Surface {
    public:
	WaylandLayerSurface (
	    WaylandDriver& driver, gfx::Context& context, wl_display* display, wl_compositor* compositor,
	    zwlr_layer_shell_v1* shell, wl_output* output_handle, const Output& output, bool vsync
	) : driver_ (driver), context_ (context), display_ (display), output_ (output), vsync_ (vsync) {
	    this->surface_ = wl_compositor_create_surface (compositor);

	    if (this->surface_ == nullptr) {
		core::log_error ("wl_compositor.create_surface failed");
		return;
	    }

	    this->layer_ = zwlr_layer_shell_v1_get_layer_surface (
		shell, this->surface_, output_handle, ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, "linux-wallpaperengine"
	    );

	    if (this->layer_ == nullptr) {
		core::log_error ("zwlr_layer_shell_v1.get_layer_surface failed");
		return;
	    }

	    zwlr_layer_surface_v1_add_listener (this->layer_, &this->layer_listener_, this);

	    zwlr_layer_surface_v1_set_anchor (
		this->layer_,
		ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM
		    | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT
	    );
	    zwlr_layer_surface_v1_set_exclusive_zone (this->layer_, -1);
	    // Protocol v1 exposes keyboard interactivity as a uint (0 = none).
	    zwlr_layer_surface_v1_set_keyboard_interactivity (this->layer_, 0);
	    zwlr_layer_surface_v1_set_size (this->layer_, 0, 0);

	    const int initial_scale = std::max (this->output_.scale, 1);
	    wl_surface_set_buffer_scale (this->surface_, initial_scale);

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

	    this->driver_.register_surface (this->surface_, &this->output_);
	}

	~WaylandLayerSurface () override {
	    this->driver_.unregister_surface (this->surface_);
	    this->egl_surface_ = gfx::Surface {};

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

	[[nodiscard]] bool valid () const noexcept override { return this->valid_ && !this->closed_; }

	[[nodiscard]] core::Extent size () const noexcept override {
	    return { this->egl_surface_.width (), this->egl_surface_.height () };
	}

	void make_current () override {
	    if (this->valid ()) {
		this->context_.make_current (&this->egl_surface_);
	    }
	}

	void swap () override {
	    if (!this->valid ()) {
		return;
	    }
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
	    if (width > 0) {
		this->configured_width_ = static_cast<int> (width);
	    }
	    if (height > 0) {
		this->configured_height_ = static_cast<int> (height);
	    }

	    if (this->egl_window_ == nullptr) {
		return;
	    }

	    const int scale = std::max (this->output_.scale, 1);
	    wl_surface_set_buffer_scale (this->surface_, scale);

	    const int logical_w = this->configured_width_ > 0 ? this->configured_width_ : this->output_.width;
	    const int logical_h = this->configured_height_ > 0 ? this->configured_height_ : this->output_.height;
	    const int pixel_width = logical_w * scale;
	    const int pixel_height = logical_h * scale;

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

	static void on_configure (
	    void* data, zwlr_layer_surface_v1* layer, std::uint32_t serial, std::uint32_t width, std::uint32_t height
	) {
	    auto* self = static_cast<WaylandLayerSurface*> (data);
	    zwlr_layer_surface_v1_ack_configure (layer, serial);
	    self->configured_ = true;
	    self->resize_from_configure (width, height);
	}

	static void on_closed (void* data, zwlr_layer_surface_v1* layer) {
	    (void)layer;
	    auto* self = static_cast<WaylandLayerSurface*> (data);
	    self->closed_ = true;
	    self->valid_ = false;
	    self->driver_.notify_surface_closed ();
	}

	static constexpr zwlr_layer_surface_v1_listener layer_listener_ = {
	    .configure = on_configure,
	    .closed = on_closed,
	};

	WaylandDriver& driver_;
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

    WaylandDriver::WaylandDriver (bool vsync) : vsync_ (vsync) {
	this->display_ = wl_display_connect (nullptr);

	if (this->display_ == nullptr) {
	    return;
	}

	this->registry_ = wl_display_get_registry (this->display_);
	wl_registry_add_listener (this->registry_, &this->registry_listener_, this);

	// First roundtrip binds the globals, the second delivers output
	// geometry/mode events and seat capabilities for every wl_output / wl_seat.
	wl_display_roundtrip (this->display_);
	wl_display_roundtrip (this->display_);
    }

    WaylandDriver::~WaylandDriver () {
	this->surface_bindings_.clear ();
	this->destroy_seat ();
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

    std::expected<std::unique_ptr<Surface>, core::Error>
    WaylandDriver::create_surface (std::size_t output_index, gfx::Context& context) {
	if (!this->has_layer_shell ()) {
	    return core::fail (core::ErrorCode::unsupported, "the compositor does not expose zwlr_layer_shell_v1");
	}

	if (output_index >= this->outputs_.size ()) {
	    return core::fail (core::ErrorCode::out_of_range, "Wayland output index out of range");
	}

	auto surface = std::make_unique<WaylandLayerSurface> (
	    *this, context, this->display_, this->compositor_, this->layer_shell_,
	    this->outputs_[output_index]->handle (), this->outputs_[output_index]->description (), this->vsync_
	);

	if (!surface->valid ()) {
	    return core::fail (core::ErrorCode::io, "cannot create the Wayland layer surface");
	}

	return surface;
    }

    void WaylandDriver::pump () {
	if (this->display_ == nullptr || this->closed_) {
	    return;
	}

	if (wl_display_get_error (this->display_) != 0) {
	    this->closed_ = true;
	    return;
	}

	while (wl_display_prepare_read (this->display_) != 0) {
	    if (wl_display_dispatch_pending (this->display_) < 0) {
		this->closed_ = true;
		return;
	    }
	}

	if (wl_display_flush (this->display_) < 0 && errno != EAGAIN) {
	    wl_display_cancel_read (this->display_);
	    this->closed_ = true;
	    return;
	}

	pollfd descriptor {
	    .fd = wl_display_get_fd (this->display_),
	    .events = POLLIN | POLLERR | POLLHUP,
	    .revents = 0,
	};

	const int poll_ret = poll (&descriptor, 1, 0);
	if (poll_ret < 0 && errno != EINTR) {
	    wl_display_cancel_read (this->display_);
	    this->closed_ = true;
	    return;
	}

	if (poll_ret > 0 && (descriptor.revents & (POLLERR | POLLHUP)) != 0) {
	    wl_display_cancel_read (this->display_);
	    this->closed_ = true;
	    return;
	}

	if (poll_ret > 0 && (descriptor.revents & POLLIN) != 0) {
	    if (wl_display_read_events (this->display_) < 0) {
		this->closed_ = true;
		return;
	    }
	} else {
	    wl_display_cancel_read (this->display_);
	}

	if (wl_display_dispatch_pending (this->display_) < 0) {
	    this->closed_ = true;
	    return;
	}

	if (wl_display_flush (this->display_) < 0 && errno != EAGAIN) {
	    this->closed_ = true;
	    return;
	}

	if (wl_display_get_error (this->display_) != 0) {
	    this->closed_ = true;
	}
    }

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
