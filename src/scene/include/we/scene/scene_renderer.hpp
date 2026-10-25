#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <expected>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "we/core/error.hpp"
#include "we/core/math.hpp"
#include "we/gfx/framebuffer.hpp"
#include "we/gfx/gl.hpp"
#include "we/gfx/renderer.hpp"
#include "we/gfx/shader.hpp"
#include "we/scene/asset_container.hpp"
#include "we/scene/scene.hpp"
#include "we/scene/texture.hpp"
#include "we/scene/video.hpp"

namespace we::scene {

struct CompiledPass {
    gfx::Shader shader;
    std::string target;
    std::vector<EffectPassBind> binds;
    std::unordered_map<std::string, float> float_uniforms;
    std::unordered_map<std::string, core::Vec2> vec2_uniforms;
    std::unordered_map<std::string, core::Vec3> vec3_uniforms;
    std::unordered_map<std::string, core::Vec4> vec4_uniforms;
    std::unordered_map<std::string, int> int_uniforms;
    std::vector<std::shared_ptr<gfx::Texture>> textures;
    gfx::BlendMode blend = gfx::BlendMode::normal;
    bool compose = false;
};

struct EffectInstance {
    std::vector<CompiledPass> passes;
};

struct ParticleInstance {
    core::Vec3 position {};
    core::Vec3 velocity {};
    core::Vec3 rotation {};
    core::Vec3 angular_velocity {};
    core::Vec3 color { 1.0F, 1.0F, 1.0F };
    float alpha = 1.0F;
    float size = 20.0F;
    float lifetime = 1.0F;
    float age = 0.0F;
    bool alive = false;

    struct {
	core::Vec3 color { 1.0F, 1.0F, 1.0F };
	float alpha = 1.0F;
	float size = 20.0F;
	float lifetime = 1.0F;
    } initial {};

    struct {
	bool initialized = false;
	float frequency = 1.0F;
	float scale_min = 0.0F;
	float scale_max = 1.0F;
	float phase = 0.0F;
	float base = 1.0F;
    } oscillate_alpha {};
};

enum class EmitterType : std::uint8_t {
    box_random,
    sphere_random,
};

enum class InitializerType : std::uint8_t {
    unknown,
    lifetime_random,
    size_random,
    alpha_random,
    color_random,
    velocity_random,
    rotation_random,
    angular_velocity_random,
};

enum class OperatorType : std::uint8_t {
    unknown,
    movement,
    alpha_fade,
    oscillate_alpha,
    size_change,
    oscillate_position,
};

struct ParticleEmitterDef {
    std::string name;
    EmitterType type = EmitterType::box_random;
    core::Vec3 origin {};
    core::Vec3 distance_min {};
    core::Vec3 distance_max {};
    core::Vec3 directions { 1.0F, 1.0F, 1.0F };
    core::Vec3 sign {};
    float rate = 0.0F;
    int instantaneous = 0;
    float speed_min = 0.0F;
    float speed_max = 0.0F;
};

struct ParticleInitializerDef {
    std::string name;
    InitializerType type = InitializerType::unknown;
    float min_val = 0.0F;
    float max_val = 0.0F;
    float exponent = 1.0F;
    core::Vec3 min_vec {};
    core::Vec3 max_vec {};
};

struct ParticleOperatorDef {
    std::string name;
    OperatorType type = OperatorType::unknown;
    core::Vec3 gravity {};
    float drag = 0.0F;
    float fade_in_time = 0.0F;
    float fade_out_time = 1.0F;
    float frequency_min = 0.0F;
    float frequency_max = 0.0F;
    float scale_min = 0.0F;
    float scale_max = 1.0F;
    float phase_min = 0.0F;
    float phase_max = 0.0F;
    float start_time = 0.0F;
    float end_time = 1.0F;
    float start_val = 0.0F;
    float end_val = 1.0F;
    core::Vec3 mask { 1.0F, 1.0F, 1.0F };
};

struct ParticleRendererDef {
    std::string name = "sprite";
    float length = 1.0F;
    float max_length = 10.0F;
    float min_length = 0.0F;
};

struct ParticleSystemState {
    std::string name;
    core::Vec3 origin {};
    core::Vec3 scale { 1.0F, 1.0F, 1.0F };
    ParticleInstanceOverride instance_override;
    std::shared_ptr<gfx::Texture> texture;
    gfx::BlendMode blend = gfx::BlendMode::normal;
    std::uint32_t max_count = 1000;
    float start_time = 0.0F;
    std::vector<ParticleEmitterDef> emitters;
    std::vector<ParticleInitializerDef> initializers;
    std::vector<ParticleOperatorDef> operators;
    ParticleRendererDef renderer_def;
    std::vector<float> emitter_timers;
    std::vector<std::uint8_t> emitter_instantaneous_done;
    bool is_refract = false;

    std::vector<ParticleInstance> particles;
    std::mt19937 rng { 1337 };

    void update (float dt, float current_time);
    void prewarm (float seconds);
    void spawn_particle (ParticleInstance& p, const ParticleEmitterDef& emitter);
};

// Turns scene.json image objects into textured quads and post-processing layers.
class SceneRenderer {
public:
    SceneRenderer () = default;
    ~SceneRenderer () = default;
    SceneRenderer (const SceneRenderer&) = delete;
    SceneRenderer& operator= (const SceneRenderer&) = delete;
    SceneRenderer (SceneRenderer&& other) noexcept = default;
    SceneRenderer& operator= (SceneRenderer&& other) noexcept = default;

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
    [[nodiscard]] bool uses_offscreen () const noexcept { return this->uses_offscreen_; }

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
	std::vector<TextureFrame> frames;
	float duration = 0.0F;
	int texture_width = 0;
	int texture_height = 0;
	std::unique_ptr<VideoPlayer> video;

	bool is_effect = false;
	bool fullscreen = false;
	std::vector<EffectInstance> effects;
	bool is_particle = false;
	std::size_t particle_system_index = 0;
	int order_index = 0;
    };

    void render_effect_pass (
	const CompiledPass& pass, gfx::Framebuffer*& current_composite, gfx::Framebuffer*& next_composite,
	float current_time = 0.0F
    ) const;

    void render_particles (gfx::Renderer& renderer, ParticleSystemState& ps, float dt, float current_time) const;

    std::vector<Layer> layers_;
    std::vector<gfx::PointLight> lights_;
    core::Vec3 ambient_ {};
    std::unordered_map<std::string, std::shared_ptr<gfx::Texture>> textures_;
    std::unordered_map<std::string, TextureData> texture_data_;
    core::Mat4 view_projection_ {};
    core::Color clear_color_ {};
    int scene_width_ = 1920;
    int scene_height_ = 1080;
    std::chrono::steady_clock::time_point start_time_ { std::chrono::steady_clock::now () };

    bool uses_offscreen_ = false;
    bool bloom_enabled_ = false;
    BloomSettings bloom_;
    mutable gfx::Framebuffer fbo_scene_a_;
    mutable gfx::Framebuffer fbo_scene_b_;
    mutable std::unordered_map<std::string, gfx::Framebuffer> fbos_;
    std::vector<CompiledPass> bloom_passes_;
    gfx::Shader blit_shader_;
    gfx::GlVertexArray quad_vao_;
    gfx::GlBuffer quad_vbo_;
    mutable std::chrono::steady_clock::time_point last_render_time_ { std::chrono::steady_clock::now () };
    mutable std::vector<ParticleSystemState> particle_systems_;
};

} // namespace we::scene
