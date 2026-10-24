# Helper that applies the project-wide contract to a target.
#
# Every component is a static library with a local `include/` directory and a
# `src/` directory, so the same defaults apply to all of them.

include_guard(GLOBAL)

function(we_configure_target target)
    target_link_libraries(${target} PRIVATE we_project_options)

    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF)
endfunction()

# Adds a component library with the conventional layout:
#   <component>/include/...  public headers
#   <component>/src/...      implementation
#
# Usage:
#   we_add_component(we_steam SOURCES src/vdf.cpp src/vfs.cpp)
function(we_add_component target)
    cmake_parse_arguments(ARG "" "" "SOURCES" ${ARGN})

    if (NOT ARG_SOURCES)
        message(FATAL_ERROR "we_add_component(${target}) requires SOURCES")
    endif ()

    add_library(${target} STATIC ${ARG_SOURCES})
    target_include_directories(${target}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

    we_configure_target(${target})
endfunction()
