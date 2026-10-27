cmake_minimum_required(VERSION 3.24)

project(linux-wallpaperengine
    VERSION 2.0.0
    DESCRIPTION "Native Wallpaper Engine runtime for Linux"
    LANGUAGES C CXX)

include(GNUInstallDirs)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

option(WE_BUILD_CLI "Build the `we` command line tool" ON)
option(WE_BUILD_TESTS "Build the unit tests" ON)
option(WE_ENABLE_WAYLAND "Build the Wayland layer-shell backend" ON)
option(WE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(WE_ENABLE_LTO "Enable link time optimization for release builds" ON)
option(WE_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(WE_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(WE_ENABLE_TSAN "Enable ThreadSanitizer" OFF)

include(CompilerOptions)
include(WeTarget)

if (WE_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT WE_IPO_SUPPORTED OUTPUT WE_IPO_ERROR LANGUAGES CXX)
    if (WE_IPO_SUPPORTED)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    else ()
        message(STATUS "LTO unavailable: ${WE_IPO_ERROR}")
    endif ()
endif ()

# Component order matters: steam depends on core, display depends on gfx,
# and the CLI depends on all of them.
add_subdirectory(src/core)
add_subdirectory(src/presets)
add_subdirectory(src/steam)
add_subdirectory(src/gfx)
add_subdirectory(src/display)
add_subdirectory(src/scene)
add_subdirectory(src/power)
add_subdirectory(src/app)

if (WE_BUILD_CLI)
    add_subdirectory(src/cli)
endif ()

if (WE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()

# ==============================================================================
# Installation
# ==============================================================================
if (WE_BUILD_CLI)
    # Desktop entry
    install(FILES packaging/desktop/linux-wallpaperengine.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
        COMPONENT Application
    )

    # Scalable application icon
    install(FILES packaging/desktop/linux-wallpaperengine.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
        COMPONENT Application
    )

    # Systemd user units
    if (NOT DEFINED SYSTEMD_USER_UNIT_DIR)
        set(SYSTEMD_USER_UNIT_DIR "lib/systemd/user")
    endif ()

    install(FILES
        packaging/systemd/linux-wallpaperengine.service
        packaging/systemd/linux-wallpaperengine@.service
        DESTINATION ${SYSTEMD_USER_UNIT_DIR}
        COMPONENT Systemd
    )

    # Quickshell Gallery integration
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/contrib/quickshell-gallery")
        install(DIRECTORY contrib/quickshell-gallery
            DESTINATION ${CMAKE_INSTALL_DATADIR}/linux-wallpaperengine
            COMPONENT Gallery
        )
    endif ()
endif ()

# ==============================================================================
# Packaging (CPack)
# ==============================================================================
set(CPACK_PACKAGE_NAME "linux-wallpaperengine")
set(CPACK_PACKAGE_VENDOR "linux-wallpaperengine")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_DESCRIPTION "Native, lightweight Wallpaper Engine runtime for Linux built from scratch in modern C++23.")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/Almamu/linux-wallpaperengine")
set(CPACK_PACKAGE_CONTACT "Alexis Maiquez <aur@almamu.com>")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_LICENSE "GPL-3.0-or-later")

# Generators
set(CPACK_GENERATOR "TGZ;DEB;RPM")
set(CPACK_SOURCE_GENERATOR "TGZ")

# Debian packaging
if (NOT CPACK_DEBIAN_PACKAGE_ARCHITECTURE)
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
    elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "arm64")
    else ()
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${CMAKE_SYSTEM_PROCESSOR}")
    endif ()
endif ()
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_CONTACT}")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${CPACK_PACKAGE_HOMEPAGE_URL}")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libstdc++6, libmpv2 | libmpv1, libwayland-client0, libx11-6, libegl1, liblz4-1")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)

# RPM packaging
set(CPACK_RPM_PACKAGE_DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION}")
set(CPACK_RPM_PACKAGE_LICENSE "GPL-3.0-or-later")
set(CPACK_RPM_PACKAGE_GROUP "Amusements/Graphics")
set(CPACK_RPM_PACKAGE_URL "${CPACK_PACKAGE_HOMEPAGE_URL}")
set(CPACK_RPM_PACKAGE_REQUIRES "mpv-libs, wayland, libX11, libglvnd-egl, lz4-libs")
set(CPACK_RPM_PACKAGE_AUTOREQ OFF)

include(CPack)

message(STATUS "")
message(STATUS "linux-wallpaperengine ${PROJECT_VERSION} (rebuild)")
message(STATUS "  build type .......... ${CMAKE_BUILD_TYPE}")
message(STATUS "  compiler ............ ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  CLI ................. ${WE_BUILD_CLI}")
message(STATUS "  tests ............... ${WE_BUILD_TESTS}")
message(STATUS "  warnings as errors .. ${WE_WARNINGS_AS_ERRORS}")
message(STATUS "")
