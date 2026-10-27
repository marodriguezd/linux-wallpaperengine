#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
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
    std::vector<core::Vec4> texture_resolutions;
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
    int random_frame = -1;

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

    struct {
	bool initialized = false;
	core::Vec3 frequency {};
	core::Vec3 scale {};
	core::Vec3 phase {};
	core::Vec3 current_offset {};
    } oscillate_position {};
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
    float frequency_min = 1.0F;
    float frequency_max = 2.0F;
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
    std::string animation_mode = "sequence";
    float sequence_multiplier = 1.0F;
    std::vector<TextureFrame> frames;
    float duration = 0.0F;
    int texture_width = 0;
    int texture_height = 0;
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

    [[nodiscard]] bool has_animation () const noexcept { return !this->frames.empty (); }
    [[nodiscard]] core::Rect get_particle_uv (const ParticleInstance& p) const;
};

struct SceneRendererOptions {
    bool hardware_decode = true;
    bool audio_enabled = false;
    float volume = 1.0F;
};

struct SceneAudio;

// Turns scene.json image objects into textured quads and post-processing layers.
class SceneRenderer {
public:
    SceneRenderer ();
    ~SceneRenderer ();
    SceneRenderer (const SceneRenderer&) = delete;
    SceneRenderer& operator= (const SceneRenderer&) = delete;
    SceneRenderer (SceneRenderer&& other) noexcept;
    SceneRenderer& operator= (SceneRenderer&& other) noexcept;

    // Requires a current OpenGL context: textures are uploaded here.
    [[nodiscard]] static std::expected<SceneRenderer, core::Error>
    build (const Scene& scene, const AssetContainer& assets, const SceneRendererOptions& options = {});

    void render (gfx::Renderer& renderer) const;

    // Advances every video layer. Returns true when any frame changed.
    bool update_videos ();
    // Must be called after presenting frames so mpv can pace decoding.
    void report_video_swap ();

    void update_input (std::optional<core::Vec2> normalized_cursor, float dt);

    [[nodiscard]] std::size_t layer_count () const noexcept { return this->layers_.size (); }
    [[nodiscard]] const core::Mat4& view_projection () const noexcept { return this->view_projection_; }
    [[nodiscard]] core::Color clear_color () const noexcept { return this->clear_color_; }
    [[nodiscard]] int scene_width () const noexcept { return this->scene_width_; }
    [[nodiscard]] int scene_height () const noexcept { return this->scene_height_; }
    [[nodiscard]] bool uses_offscreen () const noexcept { return this->uses_offscreen_; }
    [[nodiscard]] const SceneCamera& camera () const noexcept { return this->camera_; }
    [[nodiscard]] core::Vec2 parallax_displacement () const noexcept { return this->parallax_displacement_; }
    [[nodiscard]] const std::array<core::Vec2, 4>& layer_corners (std::size_t index) const {
	return this->layers_.at (index).corners;
    }
    [[nodiscard]] const std::array<core::Vec2, 4>& layer_base_corners (std::size_t index) const {
	return this->layers_.at (index).base_corners;
    }
    [[nodiscard]] core::Rect layer_uv (std::size_t index, float elapsed = 0.0F) const;
    [[nodiscard]] std::size_t layer_frame_count (std::size_t index) const {
	return this->layers_.at (index).frames.size ();
    }
    [[nodiscard]] gfx::BlendMode layer_blend (std::size_t index) const { return this->layers_.at (index).blend; }
    [[nodiscard]] bool layer_has_puppet_mesh (std::size_t index) const {
	return this->layers_.at (index).puppet_gpu.has_value ();
    }
    [[nodiscard]] bool has_audio () const noexcept { return this->audio_ != nullptr; }

private:
    struct Layer {
	std::shared_ptr<gfx::Texture> texture;
	std::array<core::Vec2, 4> corners;
	std::array<core::Vec2, 4> base_corners {};
	core::Vec2 parallax_depth { 0.0F, 0.0F };
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
	mutable std::optional<gfx::Framebuffer> fbo_a;
	mutable std::optional<gfx::Framebuffer> fbo_b;
	mutable gfx::Texture effect_view;
	bool is_particle = false;
	std::size_t particle_system_index = 0;
	int order_index = 0;

	struct PuppetGpu {
	    gfx::GlVertexArray vao;
	    gfx::GlBuffer vbo_positions;
	    gfx::GlBuffer vbo_uvs;
	    gfx::GlBuffer ebo;
	    GLsizei index_count = 0;
	    std::vector<core::Vec3> base_positions;
	    std::vector<core::Vec3> current_positions;
	    gfx::GlVertexArray local_vao;
	    gfx::GlBuffer local_vbo_positions;
	};

	std::optional<PuppetGpu> puppet_gpu;
    };

    void render_effect_pass (
	const CompiledPass& pass, gfx::Framebuffer*& current_composite, gfx::Framebuffer*& next_composite,
	float current_time = 0.0F
    ) const;

    void render_particles (gfx::Renderer& renderer, ParticleSystemState& ps, float dt, float current_time) const;
    void draw_puppet_layer (const Layer& layer, const gfx::Texture& texture) const;
    [[nodiscard]] core::Rect calculate_layer_uv (const Layer& layer, float elapsed) const;

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
    gfx::Shader puppet_shader_;
    gfx::GlVertexArray quad_vao_;
    gfx::GlBuffer quad_vbo_;
    mutable std::chrono::steady_clock::time_point last_render_time_ { std::chrono::steady_clock::now () };
    mutable std::vector<ParticleSystemState> particle_systems_;
    SceneCamera camera_;
    core::Vec2 parallax_displacement_ { 0.0F, 0.0F };
    std::unique_ptr<SceneAudio> audio_;
};

} // namespace we::scene
