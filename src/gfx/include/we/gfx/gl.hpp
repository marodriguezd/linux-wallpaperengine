#pragma once

#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif

#include <GL/gl.h>
#include <GL/glext.h>

#include <string_view>

#include "we/core/log.hpp"

namespace we::gfx {

// Drains the GL error queue and logs the first error found. Returns true when
// the queue was clean.
inline bool check_gl (std::string_view where) noexcept {
    bool clean = true;

    for (int guard = 0; guard < 32; ++guard) {
        const GLenum error = glGetError ();

        if (error == GL_NO_ERROR) {
            return clean;
        }

        clean = false;

        const char* name = "GL_UNKNOWN";
        switch (error) {
            case GL_INVALID_ENUM:
                name = "GL_INVALID_ENUM";
                break;
            case GL_INVALID_VALUE:
                name = "GL_INVALID_VALUE";
                break;
            case GL_INVALID_OPERATION:
                name = "GL_INVALID_OPERATION";
                break;
            case GL_OUT_OF_MEMORY:
                name = "GL_OUT_OF_MEMORY";
                break;
            default:
                break;
        }

        core::log_joined (core::LogLevel::error, "OpenGL ", name, " at ", where);
    }

    return clean;
}

} // namespace we::gfx
