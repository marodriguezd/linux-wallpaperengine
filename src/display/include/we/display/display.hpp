#pragma once

#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "we/core/error.hpp"
#include "we/core/math.hpp"
#include "we/gfx/context.hpp"

namespace we::display {

enum class Backend : std::uint8_t {
    automatic,
    x11_desktop,
    x11_root,
    wayland,
    headless,
};

[[nodiscard]] std::string_view to_string (Backend backend) noexcept;
[[nodiscard]] std::optional<Backend> backend_from_string (std::string_view value) noexcept;

struct Output {
    std::string name;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    float refresh_hz = 60.0F;
    bool primary = false;
    int scale = 1;
};

// A drawable presented to the user (or an offscreen buffer for the headless
// backend). One surface per monitor lets the engine pause individual outputs.
class Surface {
public:
    virtual ~Surface () = default;

    Surface (const Surface&) = delete;
    Surface& operator= (const Surface&) = delete;

    [[nodiscard]] virtual bool valid () const noexcept = 0;
    [[nodiscard]] virtual core::Extent size () const noexcept = 0;
    virtual void make_current () = 0;
    virtual void swap () = 0;
    virtual void set_vsync (bool enabled) = 0;

    // Copies the current front/back buffer into `rgba` as top-down RGBA8.
    // Supported by offscreen surfaces; other backends ignore the call.
    virtual void readback (std::span<std::uint8_t> rgba) { (void)rgba; }

protected:
    Surface () = default;
};

class Driver {
public:
    virtual ~Driver () = default;

    Driver (const Driver&) = delete;
    Driver& operator= (const Driver&) = delete;

    [[nodiscard]] virtual Backend backend () const noexcept = 0;
    [[nodiscard]] virtual std::string_view name () const noexcept = 0;
    [[nodiscard]] virtual std::span<const Output> outputs () const noexcept = 0;

    [[nodiscard]] virtual std::expected<std::unique_ptr<gfx::Context>, core::Error> create_context () = 0;

    [[nodiscard]] virtual std::expected<std::unique_ptr<Surface>, core::Error>
    create_surface (std::size_t output_index, gfx::Context& context) = 0;

    virtual void pump () = 0;
    [[nodiscard]] virtual bool should_close () const noexcept = 0;

    // Bounds of the active fullscreen window on `output_index`, if any. Used to
    // pause rendering behind fullscreen games.
    [[nodiscard]] virtual std::optional<core::Rect> fullscreen_window (std::size_t output_index) const {
	(void)output_index;
	return std::nullopt;
    }

    [[nodiscard]] virtual std::optional<core::Vec2> cursor_position () const { return std::nullopt; }

protected:
    Driver () = default;
};

struct DriverOptions {
    Backend backend = Backend::automatic;
    bool vsync = true;

    // Restrict presentation to one output (by name). Empty means all outputs.
    std::string output;

    // Headless backend resolution.
    int headless_width = 1920;
    int headless_height = 1080;
    float headless_refresh = 60.0F;

    // Preferred when the desktop is not X11 and no backend was requested.
    bool allow_wayland = true;
};

// Picks the requested backend, or autodetects X11 / Wayland / headless from the
// environment. `x11_root` is never selected automatically: it is the fallback
// for window managers that do not keep desktop windows below other windows.
[[nodiscard]] std::expected<std::unique_ptr<Driver>, core::Error> create_driver (const DriverOptions& options);

[[nodiscard]] bool x11_available () noexcept;
[[nodiscard]] bool wayland_available () noexcept;

} // namespace we::display
