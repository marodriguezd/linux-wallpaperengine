# Prefer the real system EGL. Some launcher environments inject their own
# bundled libEGL through the search paths, which is not usable for the host
# display server.
find_library(WE_SYSTEM_EGL_LIBRARY
    NAMES EGL
    PATHS /usr/lib64 /usr/lib /usr/lib/x86_64-linux-gnu /usr/lib/aarch64-linux-gnu
    NO_DEFAULT_PATH)

if (WE_SYSTEM_EGL_LIBRARY)
    set(OpenGL_egl_LIBRARY "${WE_SYSTEM_EGL_LIBRARY}" CACHE FILEPATH "EGL library" FORCE)
    set(OPENGL_egl_LIBRARY "${WE_SYSTEM_EGL_LIBRARY}" CACHE FILEPATH "EGL library" FORCE)
endif ()

find_package(OpenGL REQUIRED COMPONENTS OpenGL EGL)

we_add_component(we_gfx
    SOURCES
        src/context.cpp
        src/framebuffer.cpp
        src/renderer.cpp
        src/shader.cpp
        src/texture.cpp)

target_link_libraries(we_gfx
    PUBLIC
        we_core
        OpenGL::GL
        OpenGL::EGL)
