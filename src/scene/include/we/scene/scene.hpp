#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <unordered_map>
#include <vector>

#include "we/core/error.hpp"
#include "we/core/math.hpp"
#include "we/scene/asset_container.hpp"

namespace we::scene {

struct SceneCamera {
    core::Vec3 center { 0.0F, 0.0F, 0.0F };
    core::Vec3 eye { 0.0F, 0.0F, 1.0F };
    core::Vec3 up { 0.0F, 1.0F, 0.0F };
    float fov = 50.0F;
    float near_plane = 0.0F;
    float far_plane = 1000.0F;
    int width = 1920;
    int height = 1080;
    bool automatic_projection = false;
    bool parallax_enabled = false;
    float parallax_amount = 0.5F;
    float parallax_delay = 0.9F;
    float parallax_mouse_influence = 1.0F;
};

struct SceneSound {
    std::string name;
    std::vector<std::string> files;
    std::string playback_mode = "loop";
    float volume = 1.0F;
};

struct EffectPassBind {
    std::string name;
    int index = 0;
};

struct EffectPassFbo {
    std::string name;
    int scale = 1;
    std::string format = "rgba_backbuffer";
};

struct EffectPass {
    std::string material;
    std::string shader;
    std::string target;
    std::vector<EffectPassBind> binds;
    std::unordered_map<std::string, int> combos;
    std::unordered_map<std::string, std::string> constant_values;
    std::vector<std::string> textures;
    std::string blending = "normal";
    bool compose = false;
};

struct SceneEffect {
    int id = 0;
    std::string file;
    bool visible = true;
    std::vector<EffectPassFbo> fbos;
    std::vector<EffectPass> passes;
};

struct BloomSettings {
    bool enabled = false;
    float strength = 1.0F;
    float threshold = 0.65F;
    core::Vec3 tint { 1.0F, 1.0F, 1.0F };
};

struct SceneImage {
    int id = 0;
    int order_index = 0;
    std::string name;
    core::Vec3 origin { 0.0F, 0.0F, 0.0F };
    core::Vec3 scale { 1.0F, 1.0F, 1.0F };
    core::Vec3 angles { 0.0F, 0.0F, 0.0F };
    float alpha = 1.0F;
    core::Color color {};
    core::Vec2 size { 0.0F, 0.0F };
    core::Vec2 parallax_depth { 0.0F, 0.0F };
    std::string alignment = "center";
    std::string texture_path;
    std::string shader;
    std::string blending = "normal";
    // LIGHTING/REFLECTION combos. Wallpaper Engine multiplies their lighting by
    // the object colour (genericimage4.frag), so the colour remains a tint.
    bool lit_material = false;
    bool lighting = false;
    bool reflective = false;
    float reflectivity = 1.0F;
    float roughness = 0.7F;
    float metallic = 0.0F;
    float reflectivity_distance = 4.0F;
    std::string normal_texture_path;
    // PBRMASKS: second uv channel, metallic in R, roughness in G, reflection in
    // B and emissive in A. Each map only overrides its constant when enabled.
    std::string pbr_mask_texture_path;
    bool metallic_map = false;
    bool roughness_map = false;
    bool reflection_map = false;
    bool emissive_map = false;
    core::Vec3 specular_tint { 1.0F, 1.0F, 1.0F };
    core::Vec3 emissive_color { 1.0F, 1.0F, 1.0F };
    float emissive_brightness = 1.0F;
    bool visible = true;
    bool fullscreen = false;
    bool solid_layer = false;
    int effect_count = 0;
    std::vector<SceneEffect> effects;
    bool copy_background = false;
    bool passthrough = false;
};

struct SceneLight {
    int id = 0;
    core::Vec3 origin { 0.0F, 0.0F, 0.0F };
    core::Color color { 1.0F, 1.0F, 1.0F, 1.0F };
    float intensity = 1.0F;
    // Falloff is evaluated as pow(1 - distance / radius, exponent), matching
    // ComputePBRLightShadow in Wallpaper Engine's common_pbr_2.h.
    float radius = 1.0F;
    float exponent = 1.0F;
    float density = 1.0F;
};

struct ParticleInstanceOverride {
    float rate = 1.0F;
    float lifetime = 1.0F;
    float size = 1.0F;
    float speed = 1.0F;
    float count = 1.0F;
    float alpha = 1.0F;
    core::Vec3 colorn { 1.0F, 1.0F, 1.0F };
    core::Vec3 color { 1.0F, 1.0F, 1.0F };
};

struct SceneParticle {
    int id = 0;
    int order_index = 0;
    std::string name;
    std::string file;
    core::Vec3 origin {};
    core::Vec3 scale { 1.0F, 1.0F, 1.0F };
    core::Vec3 angles {};
    bool visible = true;
    ParticleInstanceOverride instance_override;
};

struct Scene {
    std::string source;
    SceneCamera camera;
    core::Color clear_color { 0.0F, 0.0F, 0.0F, 1.0F };
    core::Vec3 ambient { 0.0F, 0.0F, 0.0F };
    BloomSettings bloom;
    std::vector<SceneImage> images;
    std::vector<SceneLight> lights;
    std::vector<SceneParticle> particles;
    std::vector<SceneSound> sounds;
    int skipped_objects = 0;
};

[[nodiscard]] std::expected<Scene, core::Error> load_scene (const AssetContainer& assets, const std::string& file);

} // namespace we::scene
