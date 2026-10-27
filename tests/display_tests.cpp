#include <cstdint>
#include <cstdlib>
#include <vector>

#include "support/harness.hpp"
#include "we/display/display.hpp"
#include "we/gfx/renderer.hpp"

using namespace we;

namespace {

[[nodiscard]] bool x11_tests_enabled () {
    const char* enabled = std::getenv ("WE_TEST_X11");
    return enabled != nullptr && *enabled == '1';
}

void exercise_surface (display::Driver& driver, gfx::Context& context, std::size_t index) {
    auto surface = driver.create_surface (index, context);
    CHECK (surface.has_value ());
    CHECK ((*surface)->valid ());

    const auto extent = (*surface)->size ();
    CHECK (extent.width > 0);
    CHECK (extent.height > 0);

    (*surface)->make_current ();

    auto renderer = gfx::Renderer::create ();
    CHECK (renderer.has_value ());

    renderer->begin (extent.width, extent.height, core::Color { 0.05F, 0.10F, 0.2F, 1.0F });
    renderer->draw_rect (
	core::Rect { 0.0F, 0.0F, static_cast<float> (extent.width), static_cast<float> (extent.height) / 2.0F },
	core::Color { 1.0F, 0.0F, 1.0F, 1.0F }
    );
    renderer->end ();

    std::vector<std::uint8_t> pixels (
	static_cast<std::size_t> (extent.width) * static_cast<std::size_t> (extent.height) * 4U
    );
    (*surface)->readback (pixels);
    (*surface)->swap ();

    const auto index_center = (static_cast<std::size_t> (extent.height / 4) * static_cast<std::size_t> (extent.width)
			       + static_cast<std::size_t> (extent.width / 2))
	* 4U;
    CHECK_EQ (pixels[index_center + 0], std::uint8_t { 255 });
    CHECK_EQ (pixels[index_center + 1], std::uint8_t { 0 });
    CHECK_EQ (pixels[index_center + 2], std::uint8_t { 255 });
}

} // namespace

TEST (display_headless_driver_presents_offscreen) {
    display::DriverOptions options;
    options.backend = display::Backend::headless;
    options.headless_width = 48;
    options.headless_height = 32;

    auto driver = display::create_driver (options);
    CHECK (driver.has_value ());
    CHECK_EQ ((*driver)->backend (), display::Backend::headless);
    CHECK_EQ ((*driver)->outputs ().size (), std::size_t { 1 });
    CHECK_EQ ((*driver)->outputs ()[0].width, 48);
    CHECK_EQ ((*driver)->outputs ()[0].height, 32);

    auto context = (*driver)->create_context ();
    CHECK (context.has_value ());

    exercise_surface (**driver, **context, 0);
    CHECK (!(*driver)->should_close ());
    CHECK (!(*driver)->fullscreen_window (0).has_value ());
    CHECK (!(*driver)->cursor_position ().has_value ());
}

TEST (display_backend_names_round_trip) {
    CHECK_EQ (display::to_string (display::Backend::x11_desktop), std::string_view ("x11-desktop"));
    CHECK_EQ (display::to_string (display::Backend::x11_root), std::string_view ("x11-root"));
    CHECK_EQ (display::to_string (display::Backend::wayland), std::string_view ("wayland"));

    CHECK (display::backend_from_string ("x11").has_value ());
    CHECK_EQ (*display::backend_from_string ("x11"), display::Backend::x11_desktop);
    CHECK_EQ (*display::backend_from_string ("root"), display::Backend::x11_root);
    CHECK_EQ (*display::backend_from_string ("headless"), display::Backend::headless);
    CHECK (!display::backend_from_string ("framebuffer").has_value ());
}

TEST (display_x11_desktop_driver_creates_desktop_windows) {
    if (!x11_tests_enabled ()) {
	return;
    }

    display::DriverOptions options;
    options.backend = display::Backend::x11_desktop;
    options.vsync = false;

    auto driver = display::create_driver (options);
    CHECK (driver.has_value ());
    CHECK_EQ ((*driver)->backend (), display::Backend::x11_desktop);
    CHECK (!(*driver)->outputs ().empty ());

    auto context = (*driver)->create_context ();
    CHECK (context.has_value ());

    exercise_surface (**driver, **context, 0);
    (*driver)->pump ();
}

TEST (display_x11_root_driver_presents_through_root_window) {
    if (!x11_tests_enabled ()) {
	return;
    }

    display::DriverOptions options;
    options.backend = display::Backend::x11_root;
    options.vsync = false;

    auto driver = display::create_driver (options);
    CHECK (driver.has_value ());

    auto context = (*driver)->create_context ();
    CHECK (context.has_value ());

    exercise_surface (**driver, **context, 0);
    (*driver)->pump ();
}

TEST (display_wayland_driver_fails_when_no_compositor) {
    display::DriverOptions options;
    options.backend = display::Backend::wayland;

    auto driver = display::create_driver (options);
    if (!driver) {
	CHECK_EQ (driver.error ().code, core::ErrorCode::io);
    } else {
	CHECK_EQ ((*driver)->backend (), display::Backend::wayland);
	(*driver)->pump ();
    }
}
