#pragma once

#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

#include "we/core/error.hpp"
#include "we/core/math.hpp"
#include "we/gfx/gl.hpp"

namespace we::gfx {

class Shader {
public:
    Shader () noexcept = default;
    ~Shader ();

    Shader (const Shader&) = delete;
    Shader& operator= (const Shader&) = delete;
    Shader (Shader&& other) noexcept;
    Shader& operator= (Shader&& other) noexcept;

    [[nodiscard]] static std::expected<Shader, core::Error>
    build (std::string_view vertex_source, std::string_view fragment_source, std::string_view label);

    [[nodiscard]] bool valid () const noexcept { return this->program_ != 0; }
    [[nodiscard]] GLuint id () const noexcept { return this->program_; }

    void use () const;

    void set_int (std::string_view name, int value);
    void set_float (std::string_view name, float value);
    void set_vec2 (std::string_view name, core::Vec2 value);
    void set_vec3 (std::string_view name, core::Vec3 value);
    void set_vec4 (std::string_view name, core::Vec4 value);
    void set_vec4_array (std::string_view name, std::span<const core::Vec4> values);
    void set_mat4 (std::string_view name, const core::Mat4& value);

private:
    [[nodiscard]] GLint uniform (std::string_view name);

    void destroy () noexcept;

    GLuint program_ = 0;
    mutable std::unordered_map<std::string, GLint> uniforms_;
};

} // namespace we::gfx
