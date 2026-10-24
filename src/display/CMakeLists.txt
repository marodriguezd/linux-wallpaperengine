find_package(X11 REQUIRED)

set(WE_DISPLAY_SOURCES
    src/factory.cpp
    src/headless.cpp
    src/x11.cpp)

if (WE_ENABLE_WAYLAND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(WAYLAND_CLIENT IMPORTED_TARGET wayland-client)
    pkg_check_modules(WAYLAND_EGL IMPORTED_TARGET wayland-egl)
    pkg_check_modules(WAYLAND_PROTOCOLS IMPORTED_TARGET wayland-protocols)
    pkg_get_variable(WE_WAYLAND_PROTOCOLS_DATADIR wayland-protocols pkgdatadir)

    find_program(WAYLAND_SCANNER wayland-scanner)

    if (WAYLAND_CLIENT_FOUND AND WAYLAND_EGL_FOUND AND WAYLAND_SCANNER)
        set(WE_WAYLAND_PROTOCOLS_DIR "${CMAKE_CURRENT_BINARY_DIR}/protocols")
        file(MAKE_DIRECTORY "${WE_WAYLAND_PROTOCOLS_DIR}")

        set(WE_LAYER_SHELL_XML "${PROJECT_SOURCE_DIR}/protocols/wlr-layer-shell-unstable-v1.xml")
        set(WE_LAYER_SHELL_HEADER "${WE_WAYLAND_PROTOCOLS_DIR}/wlr-layer-shell-unstable-v1-client-protocol.h")
        set(WE_LAYER_SHELL_SOURCE "${WE_WAYLAND_PROTOCOLS_DIR}/wlr-layer-shell-unstable-v1-protocol.c")

        add_custom_command(
            OUTPUT "${WE_LAYER_SHELL_HEADER}"
            COMMAND "${WAYLAND_SCANNER}" client-header "${WE_LAYER_SHELL_XML}" "${WE_LAYER_SHELL_HEADER}"
            DEPENDS "${WE_LAYER_SHELL_XML}"
            VERBATIM)

        add_custom_command(
            OUTPUT "${WE_LAYER_SHELL_SOURCE}"
            COMMAND "${WAYLAND_SCANNER}" private-code "${WE_LAYER_SHELL_XML}" "${WE_LAYER_SHELL_SOURCE}"
            DEPENDS "${WE_LAYER_SHELL_XML}"
            VERBATIM)

        set(WE_XDG_SHELL_XML "${WE_WAYLAND_PROTOCOLS_DATADIR}/stable/xdg-shell/xdg-shell.xml")
        set(WE_XDG_SHELL_HEADER "${WE_WAYLAND_PROTOCOLS_DIR}/xdg-shell-client-protocol.h")
        set(WE_XDG_SHELL_SOURCE "${WE_WAYLAND_PROTOCOLS_DIR}/xdg-shell-protocol.c")

        add_custom_command(
            OUTPUT "${WE_XDG_SHELL_HEADER}"
            COMMAND "${WAYLAND_SCANNER}" client-header "${WE_XDG_SHELL_XML}" "${WE_XDG_SHELL_HEADER}"
            DEPENDS "${WE_XDG_SHELL_XML}"
            VERBATIM)

        add_custom_command(
            OUTPUT "${WE_XDG_SHELL_SOURCE}"
            COMMAND "${WAYLAND_SCANNER}" private-code "${WE_XDG_SHELL_XML}" "${WE_XDG_SHELL_SOURCE}"
            DEPENDS "${WE_XDG_SHELL_XML}"
            VERBATIM)

        list(APPEND WE_DISPLAY_SOURCES
            src/wayland.cpp
            "${WE_LAYER_SHELL_SOURCE}"
            "${WE_LAYER_SHELL_HEADER}"
            "${WE_XDG_SHELL_SOURCE}"
            "${WE_XDG_SHELL_HEADER}")
        set(WE_HAVE_WAYLAND ON)
    else ()
        message(STATUS "Wayland development files not found; Wayland backend disabled")
    endif ()
else ()
    message(STATUS "Wayland backend disabled by WE_ENABLE_WAYLAND=OFF")
endif ()

we_add_component(we_display SOURCES ${WE_DISPLAY_SOURCES})

target_link_libraries(we_display
    PUBLIC
        we_gfx
        we_core
        X11::X11
        ${X11_Xext_LIB}
        ${X11_Xrandr_LIB}
        ${X11_Xfixes_LIB})

if (WE_HAVE_WAYLAND)
    target_link_libraries(we_display PUBLIC PkgConfig::WAYLAND_CLIENT PkgConfig::WAYLAND_EGL)
    target_include_directories(we_display PRIVATE "${WE_WAYLAND_PROTOCOLS_DIR}")
    target_compile_definitions(we_display PRIVATE WE_HAVE_WAYLAND=1)
endif ()
