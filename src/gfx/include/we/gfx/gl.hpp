#pragma once

#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif

#include <GL/gl.h>
#include <GL/glext.h>

#include <string_view>
#include <utility>

#include "we/core/log.hpp"

namespace we::gfx {

class GlVertexArray {
public:
    GlVertexArray () noexcept = default;
    ~GlVertexArray () noexcept { reset (); }

    GlVertexArray (const GlVertexArray&) = delete;
    GlVertexArray& operator= (const GlVertexArray&) = delete;

    GlVertexArray (GlVertexArray&& other) noexcept : id_ (std::exchange (other.id_, 0)) { }
    GlVertexArray& operator= (GlVertexArray&& other) noexcept {
	if (this != &other) {
	    reset ();
	    id_ = std::exchange (other.id_, 0);
	}
	return *this;
    }

    [[nodiscard]] GLuint id () const noexcept { return id_; }
    [[nodiscard]] bool valid () const noexcept { return id_ != 0; }

    static GlVertexArray generate () {
	GlVertexArray vao;
	glGenVertexArrays (1, &vao.id_);
	return vao;
    }

    void reset () noexcept {
	if (id_ != 0) {
	    glDeleteVertexArrays (1, &id_);
	    id_ = 0;
	}
    }

    void bind () const noexcept { glBindVertexArray (id_); }

    static void unbind () noexcept { glBindVertexArray (0); }

private:
    GLuint id_ = 0;
};

class GlBuffer {
public:
    GlBuffer () noexcept = default;
    ~GlBuffer () noexcept { reset (); }

    GlBuffer (const GlBuffer&) = delete;
    GlBuffer& operator= (const GlBuffer&) = delete;

    GlBuffer (GlBuffer&& other) noexcept : id_ (std::exchange (other.id_, 0)) { }
    GlBuffer& operator= (GlBuffer&& other) noexcept {
	if (this != &other) {
	    reset ();
	    id_ = std::exchange (other.id_, 0);
	}
	return *this;
    }

    [[nodiscard]] GLuint id () const noexcept { return id_; }
    [[nodiscard]] bool valid () const noexcept { return id_ != 0; }

    static GlBuffer generate () {
	GlBuffer buf;
	glGenBuffers (1, &buf.id_);
	return buf;
    }

    void reset () noexcept {
	if (id_ != 0) {
	    glDeleteBuffers (1, &id_);
	    id_ = 0;
	}
    }

    void bind (GLenum target = GL_ARRAY_BUFFER) const noexcept { glBindBuffer (target, id_); }

    static void unbind (GLenum target = GL_ARRAY_BUFFER) noexcept { glBindBuffer (target, 0); }

private:
    GLuint id_ = 0;
};

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
