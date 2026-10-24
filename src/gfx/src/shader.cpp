#include "we/gfx/shader.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace we::gfx {
namespace {

std::expected<GLuint, core::Error> compile_stage (GLenum stage, std::string_view source, std::string_view label) {
    const GLuint shader = glCreateShader (stage);

    if (shader == 0) {
        return core::fail (core::ErrorCode::io, "glCreateShader failed");
    }

    const char* data = source.data ();
    const GLint length = static_cast<GLint> (source.size ());
    glShaderSource (shader, 1, &data, &length);
    glCompileShader (shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv (shader, GL_COMPILE_STATUS, &compiled);

    if (compiled != GL_TRUE) {
        GLint log_length = 0;
        glGetShaderiv (shader, GL_INFO_LOG_LENGTH, &log_length);
        std::string log (static_cast<std::size_t> (std::max (log_length, 1)), '\0');
        glGetShaderInfoLog (shader, log_length, nullptr, log.data ());
        glDeleteShader (shader);
        return core::fail (
            core::ErrorCode::unsupported,
            "shader compilation failed for " + std::string (label) + ": " + std::string (log.c_str ())
        );
    }

    return shader;
}

} // namespace

Shader::~Shader () { this->destroy (); }

Shader::Shader (Shader&& other) noexcept : program_ (other.program_), uniforms_ (std::move (other.uniforms_)) {
    other.program_ = 0;
}

Shader& Shader::operator= (Shader&& other) noexcept {
    if (this != &other) {
        this->destroy ();
        this->program_ = other.program_;
        this->uniforms_ = std::move (other.uniforms_);
        other.program_ = 0;
    }

    return *this;
}

void Shader::destroy () noexcept {
    if (this->program_ != 0) {
        glDeleteProgram (this->program_);
        this->program_ = 0;
    }
    this->uniforms_.clear ();
}

std::expected<Shader, core::Error> Shader::build (std::string_view vertex_source, std::string_view fragment_source,
                                                  std::string_view label) {
    auto vertex = compile_stage (GL_VERTEX_SHADER, vertex_source, label);
    if (!vertex) {
        return std::unexpected (std::move (vertex.error ()));
    }

    auto fragment = compile_stage (GL_FRAGMENT_SHADER, fragment_source, label);
    if (!fragment) {
        glDeleteShader (*vertex);
        return std::unexpected (std::move (fragment.error ()));
    }

    const GLuint program = glCreateProgram ();
    glAttachShader (program, *vertex);
    glAttachShader (program, *fragment);
    glLinkProgram (program);

    glDeleteShader (*vertex);
    glDeleteShader (*fragment);

    GLint linked = GL_FALSE;
    glGetProgramiv (program, GL_LINK_STATUS, &linked);

    if (linked != GL_TRUE) {
        GLint log_length = 0;
        glGetProgramiv (program, GL_INFO_LOG_LENGTH, &log_length);
        std::string log (static_cast<std::size_t> (std::max (log_length, 1)), '\0');
        glGetProgramInfoLog (program, log_length, nullptr, log.data ());
        glDeleteProgram (program);
        return core::fail (
            core::ErrorCode::unsupported,
            "program link failed for " + std::string (label) + ": " + std::string (log.c_str ())
        );
    }

    Shader result;
    result.program_ = program;
    return result;
}

void Shader::use () const { glUseProgram (this->program_); }

GLint Shader::uniform (std::string_view name) {
    const std::string key (name);
    const auto existing = this->uniforms_.find (key);

    if (existing != this->uniforms_.end ()) {
        return existing->second;
    }

    const GLint location = glGetUniformLocation (this->program_, key.c_str ());
    this->uniforms_.emplace (key, location);

    if (location < 0) {
        core::log_debug ("shader uniform not found: ", key);
    }

    return location;
}

void Shader::set_int (std::string_view name, int value) {
    const GLint location = this->uniform (name);

    if (location >= 0) {
        glUniform1i (location, value);
    }
}

void Shader::set_float (std::string_view name, float value) {
    const GLint location = this->uniform (name);

    if (location >= 0) {
        glUniform1f (location, value);
    }
}

void Shader::set_vec2 (std::string_view name, core::Vec2 value) {
    const GLint location = this->uniform (name);

    if (location >= 0) {
        glUniform2f (location, value.x, value.y);
    }
}

void Shader::set_vec3 (std::string_view name, core::Vec3 value) {
    const GLint location = this->uniform (name);

    if (location >= 0) {
        glUniform3f (location, value.x, value.y, value.z);
    }
}

void Shader::set_vec4 (std::string_view name, core::Vec4 value) {
    const GLint location = this->uniform (name);

    if (location >= 0) {
        glUniform4f (location, value.x, value.y, value.z, value.w);
    }
}

void Shader::set_vec4_array (std::string_view name, std::span<const core::Vec4> values) {
    const GLint location = this->uniform (name);

    if (location >= 0 && !values.empty ()) {
        glUniform4fv (location, static_cast<GLsizei> (values.size ()),
                      reinterpret_cast<const GLfloat*> (values.data ()));
    }
}

void Shader::set_mat4 (std::string_view name, const core::Mat4& value) {
    const GLint location = this->uniform (name);

    if (location >= 0) {
        glUniformMatrix4fv (location, 1, GL_FALSE, value.m.data ());
    }
}

} // namespace we::gfx
