#pragma once

#include <array>
#include <cstddef>
#include <expected>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "we/core/error.hpp"
#include "we/core/math.hpp"
#include "we/gfx/renderer.hpp"
#include "we/scene/asset_container.hpp"
#include "we/scene/scene.hpp"
#include "we/scene/texture.hpp"
#include "we/scene/video.hpp"

namespace we::scene {

// Turns scene.json image objects into textured quads. Custom material shaders
// are recorded per layer but the renderer currently uses the shared sprite
// shader, so effects that depend on custom shaders are not applied yet.
class SceneRenderer {
public:
    SceneRenderer () = default;
    SceneRenderer (const SceneRenderer&) = delete;
    SceneRenderer& operator= (const SceneRenderer&) = delete;
    SceneRenderer (SceneRenderer&&) = default;
    SceneRenderer& operator= (SceneRenderer&&) = default;

    // Requires a current OpenGL context: textures are uploaded here.
    [[nodiscard]] static std::expected<SceneRenderer, core::Error>
    build (const Scene& scene, const AssetContainer& assets);

    void render (gfx::Renderer& renderer) const;

    // Advances every video layer. Returns true when any frame changed.
    bool update_videos ();
    // Must be called after presenting frames so mpv can pace decoding.
    void report_video_swap ();

    [[nodiscard]] std::size_t layer_count () const noexcept { return this->layers_.size (); }
    [[nodiscard]] const core::Mat4& view_projection () const noexcept { return this->view_projection_; }
    [[nodiscard]] core::Color clear_color () const noexcept { return this->clear_color_; }
    [[nodiscard]] int scene_width () const noexcept { return this->scene_width_; }
    [[nodiscard]] int scene_height () const noexcept { return this->scene_height_; }

private:
    struct Layer {
        std::shared_ptr<gfx::Texture> texture;
        std::array<core::Vec2, 4> corners;
        core::Rect uv;
        core::Color tint;
        gfx::BlendMode blend = gfx::BlendMode::normal;
        float z = 0.0F;
        bool lighting = false;
        bool reflective = false;
        float reflectivity = 1.0F;
        float roughness = 0.7F;
        float metallic = 0.0F;
        float reflectivity_distance = 4.0F;
        core::Vec2 tangent { 1.0F, 0.0F };
        core::Vec2 bitangent { 0.0F, 1.0F };
        std::shared_ptr<gfx::Texture> normal_texture;
        std::shared_ptr<gfx::Texture> pbr_mask_texture;
        bool metallic_map = false;
        bool roughness_map = false;
        bool reflection_map = false;
        bool emissive_map = false;
        core::Vec3 specular_tint { 1.0F, 1.0F, 1.0F };
        core::Vec3 emissive_color { 1.0F, 1.0F, 1.0F };
        float emissive_brightness = 1.0F;
        std::unique_ptr<VideoPlayer> video;
    };

    std::vector<Layer> layers_;
    std::vector<gfx::PointLight> lights_;
    core::Vec3 ambient_ {};
    std::unordered_map<std::string, std::shared_ptr<gfx::Texture>> textures_;
    std::unordered_map<std::string, TextureData> texture_data_;
    core::Mat4 view_projection_ {};
    core::Color clear_color_ {};
    int scene_width_ = 1920;
    int scene_height_ = 1080;
};

} // namespace we::scene
