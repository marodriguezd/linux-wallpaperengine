#include "we/display/display.hpp"

#include <cstdlib>
#include <string>

namespace we::display {

std::expected<std::unique_ptr<Driver>, core::Error> create_headless_driver (const DriverOptions& options);
std::expected<std::unique_ptr<Driver>, core::Error> create_x11_driver (const DriverOptions& options, Backend backend);
#ifdef WE_HAVE_WAYLAND
std::expected<std::unique_ptr<Driver>, core::Error> create_wayland_driver (const DriverOptions& options);
#endif

std::string_view to_string (Backend backend) noexcept {
    switch (backend) {
        case Backend::automatic:
            return "auto";
        case Backend::x11_desktop:
            return "x11-desktop";
        case Backend::x11_root:
            return "x11-root";
        case Backend::wayland:
            return "wayland";
        case Backend::headless:
            return "headless";
    }

    return "unknown";
}

std::optional<Backend> backend_from_string (std::string_view value) noexcept {
    if (value == "auto" || value == "automatic") {
        return Backend::automatic;
    }

    if (value == "x11" || value == "x11-desktop" || value == "desktop") {
        return Backend::x11_desktop;
    }

    if (value == "x11-root" || value == "root" || value == "root-pixmap") {
        return Backend::x11_root;
    }

    if (value == "wayland" || value == "wlr") {
        return Backend::wayland;
    }

    if (value == "headless" || value == "offscreen") {
        return Backend::headless;
    }

    return std::nullopt;
}

bool wayland_available () noexcept {
    const char* display = std::getenv ("WAYLAND_DISPLAY");
    return display != nullptr && *display != '\0';
}

std::expected<std::unique_ptr<Driver>, core::Error> create_driver (const DriverOptions& options) {
    Backend backend = options.backend;

    if (backend == Backend::automatic) {
        if (x11_available ()) {
            backend = Backend::x11_desktop;
        } else if (options.allow_wayland && wayland_available ()) {
            backend = Backend::wayland;
        } else {
            backend = Backend::headless;
        }
    }

    switch (backend) {
        case Backend::x11_desktop:
        case Backend::x11_root:
            return create_x11_driver (options, backend);
        case Backend::wayland: {
#ifdef WE_HAVE_WAYLAND
            return create_wayland_driver (options);
#else
            return core::fail (core::ErrorCode::unsupported,
                               "this build has no Wayland support (rebuild with WE_ENABLE_WAYLAND=ON)");
#endif
        }
        case Backend::headless:
            return create_headless_driver (options);
        case Backend::automatic:
            break;
    }

    return core::fail (core::ErrorCode::invalid_argument, "unresolved display backend");
}

} // namespace we::display
