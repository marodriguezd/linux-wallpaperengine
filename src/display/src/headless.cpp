#include "we/display/display.hpp"

#include "we/core/log.hpp"

#include <memory>

namespace we::display {
namespace {

class HeadlessSurface final : public Surface {
public:
    HeadlessSurface (gfx::Context& context, int width, int height) : context_ (context) {
        auto surface = context.create_pbuffer (width, height);

        if (surface) {
            this->surface_ = std::move (*surface);
            this->valid_ = true;
        } else {
            core::log_error ("cannot create headless pbuffer: ", surface.error ().message);
        }
    }

    [[nodiscard]] bool valid () const noexcept override { return this->valid_; }

    [[nodiscard]] core::Extent size () const noexcept override {
        return { this->surface_.width (), this->surface_.height () };
    }

    void make_current () override { this->context_.make_current (&this->surface_); }

    void swap () override { this->make_current (); }

    void set_vsync (bool enabled) override { this->context_.set_swap_interval (enabled ? 1 : 0); }

    void readback (std::span<std::uint8_t> rgba) override {
        if (rgba.size () >= static_cast<std::size_t> (this->surface_.width ())
                * static_cast<std::size_t> (this->surface_.height ()) * 4U) {
            this->surface_.read_rgba (rgba, this->surface_.width (), this->surface_.height ());
        }
    }

private:
    gfx::Context& context_;
    gfx::Surface surface_;
    bool valid_ = false;
};

class HeadlessDriver final : public Driver {
public:
    explicit HeadlessDriver (const DriverOptions& options) :
        output_ { .name = "headless",
                  .x = 0,
                  .y = 0,
                  .width = options.headless_width,
                  .height = options.headless_height,
                  .refresh_hz = options.headless_refresh,
                  .primary = true,
                  .scale = 1 },
        vsync_ (options.vsync) { }

    [[nodiscard]] Backend backend () const noexcept override { return Backend::headless; }
    [[nodiscard]] std::string_view name () const noexcept override { return "headless"; }
    [[nodiscard]] std::span<const Output> outputs () const noexcept override { return { &this->output_, 1 }; }

    std::expected<std::unique_ptr<gfx::Context>, core::Error> create_context () override {
        auto context = gfx::Context::create_headless ();

        if (!context) {
            return std::unexpected (std::move (context.error ()));
        }

        context->set_swap_interval (0);
        return std::make_unique<gfx::Context> (std::move (*context));
    }

    std::expected<std::unique_ptr<Surface>, core::Error>
    create_surface (std::size_t output_index, gfx::Context& context) override {
        if (output_index != 0) {
            return core::fail (core::ErrorCode::out_of_range, "headless backend has a single output");
        }

        return std::make_unique<HeadlessSurface> (context, this->output_.width, this->output_.height);
    }

    void pump () override { }
    [[nodiscard]] bool should_close () const noexcept override { return this->closed_; }

    void close () noexcept { this->closed_ = true; }

private:
    Output output_;
    bool vsync_ = true;
    bool closed_ = false;
};

} // namespace

std::expected<std::unique_ptr<Driver>, core::Error> create_headless_driver (const DriverOptions& options) {
    return std::make_unique<HeadlessDriver> (options);
}

} // namespace we::display
