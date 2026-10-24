#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

#include "we/core/math.hpp"
#include "we/gfx/shader.hpp"
#include "we/gfx/texture.hpp"

namespace we::gfx {

enum class BlendMode : std::uint8_t { normal, additive, translucent };

// Wallpaper Engine caps scenes at four lights for performance reasons.
inline constexpr int max_point_lights = 4;

struct PointLight {
    core::Vec3 position;
    core::Vec3 color { 1.0F, 1.0F, 1.0F };
    float intensity = 1.0F;
    float radius = 1.0F;
    float exponent = 1.0F;
};

// Parameters of a genericimage4 REFLECTION pass.
struct ReflectionOptions {
    // Tangent-space normal map; without one the material has no NORMALMAP
    // combo and the reflection is skipped, as in Wallpaper Engine.
    const Texture* normal_map = nullptr;
    // Materials can enable REFLECTION without LIGHTING, but still use PBRMASKS
    // for roughness, metallic and the reflection map.
    const Texture* pbr_mask = nullptr;
    bool metallic_map = false;
    bool roughness_map = false;
    bool reflection_map = false;
    float reflectivity = 1.0F;
    float roughness = 0.7F;
    float metallic = 0.0F;
    float reflectivity_distance = 4.0F;
    // Quad frame in scene space: tangent points along +u, bitangent along +v.
    core::Vec2 tangent { 1.0F, 0.0F };
    core::Vec2 bitangent { 0.0F, 1.0F };
};

// Parameters of a genericimage4 LIGHTING pass. The material colour is the
// albedo tint, as in Wallpaper Engine; the ambient term multiplies it too.
struct LightingOptions {
    // Tangent-space normal map. Without one the surface normal is +Z.
    const Texture* normal_map = nullptr;
    // PBRMASKS texture: metallic R, roughness G, reflection B, emissive A.
    const Texture* pbr_mask = nullptr;
    float roughness = 0.7F;
    float metallic = 0.0F;
    core::Vec3 specular_tint { 1.0F, 1.0F, 1.0F };
    core::Vec3 emissive_color { 1.0F, 1.0F, 1.0F };
    float emissive_brightness = 1.0F;
    bool metallic_map = false;
    bool roughness_map = false;
    bool reflection_map = false;
    bool emissive_map = false;
    // Quad frame in scene space: tangent points along +u, bitangent along +v.
    core::Vec2 tangent { 1.0F, 0.0F };
    core::Vec2 bitangent { 0.0F, 1.0F };
};

// Batched textured-quad renderer.
//
// Coordinates are in pixels with the origin at the top-left corner unless a
// custom projection is installed. Draws accumulate into one GPU buffer and are
// flushed when the texture or blend mode changes.
class Renderer {
public:
    Renderer () noexcept = default;
    ~Renderer ();

    Renderer (const Renderer&) = delete;
    Renderer& operator= (const Renderer&) = delete;
    Renderer (Renderer&& other) noexcept;
    Renderer& operator= (Renderer&& other) noexcept;

    [[nodiscard]] static std::expected<Renderer, core::Error> create ();

    void begin (int framebuffer_width, int framebuffer_height, core::Color clear_color);
    void set_projection (const core::Mat4& projection);

    // `uv` uses x/y as the top-left texture coordinate and width/height as the
    // coordinate span, so {0,0,1,1} samples the whole texture.
    void draw (const Texture& texture, const core::Rect& destination, const core::Rect& uv,
               core::Color tint = {}, BlendMode blend = BlendMode::normal);

    // Arbitrary quad, corners in clockwise order starting at the top-left.
    // Used for rotated scene layers. `z` is the scene-space height of the quad,
    // which only matters for lit materials.
    void draw_quad (const Texture& texture, const std::array<core::Vec2, 4>& corners, const core::Rect& uv,
                    core::Color tint = {}, BlendMode blend = BlendMode::normal, float z = 0.0F);

    // Copies what has been drawn so far into an internal mipmapped texture, so
    // the next reflective quads sample the frame behind them.
    void capture_frame ();

    // Draws a quad with the REFLECTION term of genericimage4: the normal map
    // perturbs a screen-space lookup into the captured frame.
    void draw_quad_reflective (const Texture& texture, const std::array<core::Vec2, 4>& corners,
                               const core::Rect& uv, core::Color tint, BlendMode blend,
                               const ReflectionOptions& options, float z = 0.0F);

    // Uploads the scene lights for the LIGHTING term. Call after begin().
    void set_lighting (std::span<const PointLight> lights, core::Vec3 ambient);

    // Draws a genericimage4 material quad: LIGHTING when `lighting` is set,
    // REFLECTION when `reflection` is set (and a frame was captured). Both
    // terms share the normal map.
    void draw_quad_material (const Texture& texture, const std::array<core::Vec2, 4>& corners,
                             const core::Rect& uv, core::Color tint, BlendMode blend, float z,
                             const LightingOptions* lighting, const ReflectionOptions* reflection);

    void draw_rect (const core::Rect& destination, core::Color color, BlendMode blend = BlendMode::normal);

    void end ();

    [[nodiscard]] int draw_calls () const noexcept { return this->draw_calls_; }
    [[nodiscard]] int vertex_count () const noexcept { return static_cast<int> (this->vertices_.size ()); }

private:
    struct Vertex {
        float x;
        float y;
        float z;
        float u;
        float v;
        float r;
        float g;
        float b;
        float a;
    };

    void push_quad (const core::Rect& destination, const core::Rect& uv, core::Color tint, float z = 0.0F);
    void push_quad (const std::array<core::Vec2, 4>& corners, const core::Rect& uv, core::Color tint,
                    float z = 0.0F);
    void prepare_draw (const Texture& texture, BlendMode blend);
    void flush ();
    void destroy () noexcept;

    Shader shader_;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    Texture white_;
    Texture neutral_normal_;
    Texture reflection_;
    int reflection_width_ = 0;
    int reflection_height_ = 0;
    std::vector<Vertex> vertices_;
    const Texture* current_texture_ = nullptr;
    BlendMode current_blend_ = BlendMode::normal;
    std::array<core::Vec4, max_point_lights> light_positions_ {};
    std::array<core::Vec4, max_point_lights> light_colors_ {};
    int light_count_ = 0;
    core::Vec3 ambient_ {};
    core::Mat4 projection_ {};
    int framebuffer_width_ = 0;
    int framebuffer_height_ = 0;
    int draw_calls_ = 0;
    bool in_frame_ = false;
};

} // namespace we::gfx
