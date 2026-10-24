#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace we::core {

constexpr float pi = 3.14159265358979323846F;
constexpr float radians (float degrees) noexcept { return degrees * (pi / 180.0F); }
constexpr float degrees (float radians_value) noexcept { return radians_value * (180.0F / pi); }

struct Vec2 {
    float x = 0.0F;
    float y = 0.0F;

    [[nodiscard]] Vec2 operator+ (Vec2 other) const noexcept { return { x + other.x, y + other.y }; }
    [[nodiscard]] Vec2 operator- (Vec2 other) const noexcept { return { x - other.x, y - other.y }; }
    [[nodiscard]] Vec2 operator* (float scalar) const noexcept { return { x * scalar, y * scalar }; }
    [[nodiscard]] Vec2 operator/ (float scalar) const noexcept { return { x / scalar, y / scalar }; }
    Vec2& operator+= (Vec2 other) noexcept {
        x += other.x;
        y += other.y;
        return *this;
    }
    Vec2& operator-= (Vec2 other) noexcept {
        x -= other.x;
        y -= other.y;
        return *this;
    }
    Vec2& operator*= (float scalar) noexcept {
        x *= scalar;
        y *= scalar;
        return *this;
    }
};

inline Vec2 operator* (float scalar, Vec2 value) noexcept { return value * scalar; }

struct Vec3 {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;

    [[nodiscard]] Vec3 operator+ (Vec3 other) const noexcept { return { x + other.x, y + other.y, z + other.z }; }
    [[nodiscard]] Vec3 operator- (Vec3 other) const noexcept { return { x - other.x, y - other.y, z - other.z }; }
    [[nodiscard]] Vec3 operator* (float scalar) const noexcept { return { x * scalar, y * scalar, z * scalar }; }
    [[nodiscard]] Vec3 operator* (Vec3 other) const noexcept { return { x * other.x, y * other.y, z * other.z }; }
};

inline Vec3 operator* (float scalar, Vec3 value) noexcept { return value * scalar; }

struct Vec4 {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
    float w = 0.0F;
};

struct Color {
    float r = 1.0F;
    float g = 1.0F;
    float b = 1.0F;
    float a = 1.0F;

    [[nodiscard]] Color with_alpha (float alpha) const noexcept { return { r, g, b, alpha }; }
};

struct Mat4 {
    // Column-major, matching OpenGL's memory layout.
    std::array<float, 16> m { 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F,
                              0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F };

    [[nodiscard]] static Mat4 identity () noexcept { return Mat4 {}; }

    [[nodiscard]] static Mat4 ortho (float left, float right, float bottom, float top, float near_plane = -1.0F,
                                     float far_plane = 1.0F) noexcept {
        Mat4 result {};
        result.m[0] = 2.0F / (right - left);
        result.m[5] = 2.0F / (top - bottom);
        result.m[10] = -2.0F / (far_plane - near_plane);
        result.m[12] = -(right + left) / (right - left);
        result.m[13] = -(top + bottom) / (top - bottom);
        result.m[14] = -(far_plane + near_plane) / (far_plane - near_plane);
        result.m[15] = 1.0F;
        return result;
    }

    [[nodiscard]] static Mat4 translation (Vec3 offset) noexcept {
        Mat4 result {};
        result.m[12] = offset.x;
        result.m[13] = offset.y;
        result.m[14] = offset.z;
        return result;
    }

    [[nodiscard]] static Mat4 scale (Vec3 factor) noexcept {
        Mat4 result {};
        result.m[0] = factor.x;
        result.m[5] = factor.y;
        result.m[10] = factor.z;
        return result;
    }

    [[nodiscard]] static Mat4 rotation_z (float angle) noexcept {
        Mat4 result {};
        const float cos_angle = std::cos (angle);
        const float sin_angle = std::sin (angle);
        result.m[0] = cos_angle;
        result.m[1] = sin_angle;
        result.m[4] = -sin_angle;
        result.m[5] = cos_angle;
        return result;
    }

    [[nodiscard]] static Mat4 look_at (Vec3 eye, Vec3 center, Vec3 up) noexcept {
        const auto normalize = [] (Vec3 value) {
            const float length = std::sqrt (value.x * value.x + value.y * value.y + value.z * value.z);
            if (length <= 1.0e-8F) {
                return value;
            }
            return Vec3 { value.x / length, value.y / length, value.z / length };
        };
        const auto cross = [] (Vec3 a, Vec3 b) {
            return Vec3 { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
        };
        const auto dot = [] (Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; };

        const Vec3 forward = normalize (center - eye);
        const Vec3 side = normalize (cross (forward, up));
        const Vec3 upward = cross (side, forward);

        Mat4 result {};
        result.m[0] = side.x;
        result.m[4] = side.y;
        result.m[8] = side.z;
        result.m[1] = upward.x;
        result.m[5] = upward.y;
        result.m[9] = upward.z;
        result.m[2] = -forward.x;
        result.m[6] = -forward.y;
        result.m[10] = -forward.z;
        result.m[12] = -dot (side, eye);
        result.m[13] = -dot (upward, eye);
        result.m[14] = dot (forward, eye);
        result.m[15] = 1.0F;
        return result;
    }

    [[nodiscard]] Mat4 operator* (const Mat4& other) const noexcept {
        Mat4 result {};
        for (std::size_t column = 0; column < 4; ++column) {
            for (std::size_t row = 0; row < 4; ++row) {
                float sum = 0.0F;
                for (std::size_t k = 0; k < 4; ++k) {
                    sum += m[k * 4 + row] * other.m[column * 4 + k];
                }
                result.m[column * 4 + row] = sum;
            }
        }
        return result;
    }

    [[nodiscard]] Vec4 operator* (Vec4 vector) const noexcept {
        return { m[0] * vector.x + m[4] * vector.y + m[8] * vector.z + m[12] * vector.w,
                 m[1] * vector.x + m[5] * vector.y + m[9] * vector.z + m[13] * vector.w,
                 m[2] * vector.x + m[6] * vector.y + m[10] * vector.z + m[14] * vector.w,
                 m[3] * vector.x + m[7] * vector.y + m[11] * vector.z + m[15] * vector.w };
    }
};

struct Rect {
    float x = 0.0F;
    float y = 0.0F;
    float width = 0.0F;
    float height = 0.0F;

    [[nodiscard]] float right () const noexcept { return x + width; }
    [[nodiscard]] float bottom () const noexcept { return y + height; }
};

struct Extent {
    int width = 0;
    int height = 0;

    [[nodiscard]] float aspect () const noexcept {
        return height == 0 ? 1.0F : static_cast<float> (width) / static_cast<float> (height);
    }
};

inline float lerp (float from, float to, float t) noexcept { return from + (to - from) * t; }
inline float clamp01 (float value) noexcept { return std::clamp (value, 0.0F, 1.0F); }

} // namespace we::core
