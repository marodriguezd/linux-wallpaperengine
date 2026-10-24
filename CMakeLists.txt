cmake_minimum_required(VERSION 3.24)

project(linux-wallpaperengine
    VERSION 2.0.0
    DESCRIPTION "Native Wallpaper Engine runtime for Linux"
    LANGUAGES C CXX)

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

message(STATUS "")
message(STATUS "linux-wallpaperengine ${PROJECT_VERSION} (rebuild)")
message(STATUS "  build type .......... ${CMAKE_BUILD_TYPE}")
message(STATUS "  compiler ............ ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  CLI ................. ${WE_BUILD_CLI}")
message(STATUS "  tests ............... ${WE_BUILD_TESTS}")
message(STATUS "  warnings as errors .. ${WE_WARNINGS_AS_ERRORS}")
message(STATUS "")
