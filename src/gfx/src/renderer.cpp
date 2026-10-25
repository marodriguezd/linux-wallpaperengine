#include "we/gfx/renderer.hpp"

#include "we/core/log.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace we::gfx {
namespace {

    constexpr std::string_view vertex_source = R"(
#version 330 core
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColor;
layout(location = 3) in float aWorldZ;

uniform mat4 uProjection;

out vec2 vTexCoord;
out vec4 vColor;
out vec4 vClip;
out vec3 vWorldPos;

void main() {
    vTexCoord = aTexCoord;
    vColor = aColor;
    vWorldPos = vec3(aPosition, aWorldZ);
    gl_Position = uProjection * vec4(vWorldPos, 1.0);
    vClip = gl_Position;
}
)";

    constexpr std::string_view fragment_source = R"(
#version 330 core
in vec2 vTexCoord;
in vec4 vColor;
in vec4 vClip;
in vec3 vWorldPos;

uniform sampler2D uTexture;
uniform sampler2D uNormalMap;
uniform sampler2D uReflection;
uniform sampler2D uPbrMask;

uniform bool uReflective;
uniform bool uLit;
uniform bool uHasNormalMap;
uniform bool uHasPbrMask;
uniform vec2 uTangent;
uniform vec2 uBitangent;
uniform float uReflectivity;
uniform float uLayerRoughness;
uniform float uLayerMetallic;
uniform float uReflectivityDistance;
uniform float uMipCount;
uniform vec2 uScreenSize;

uniform vec3 uSpecularTint;
uniform vec3 uEmissiveColor;
uniform float uEmissiveBrightness;
uniform bool uMetallicMap;
uniform bool uRoughnessMap;
uniform bool uReflectionMap;
uniform bool uEmissiveMap;

uniform int uLightCount;
uniform vec4 uLightPosition[4];
uniform vec4 uLightColor[4];
uniform vec3 uLightAmbient;

out vec4 fragColor;

const float kPi = 3.14159265359;

vec3 fresnel_schlick(float cosine, vec3 f0) {
    return f0 + (1.0 - f0) * pow(max(1.0 - cosine, 0.001), 5.0);
}

float distribution_ggx(vec3 n, vec3 h, float roughness) {
    float rSqr = roughness * roughness;
    float rSqr2 = rSqr * rSqr;
    float nh = max(dot(n, h), 0.0);
    float denominator = nh * nh * (rSqr2 - 1.0) + 1.0;
    return rSqr2 / (kPi * denominator * denominator);
}

float schlick_ggx(float nv, float roughness) {
    float roughnessBase = roughness + 1.0;
    float roughnessScaled = (roughnessBase * roughnessBase) / 8.0;
    return nv / (nv * (1.0 - roughnessScaled) + roughnessScaled);
}

float geometry_smith(vec3 n, vec3 v, vec3 l, float roughness) {
    return schlick_ggx(max(dot(n, v), 0.001), roughness)
        * schlick_ggx(max(dot(n, l), 0.001), roughness);
}

// Mirrors ComputePBRLightShadow in common_pbr_2.h. Wallpaper Engine folds the
// light intensity into the colour and attenuates by pow(1 - d/r, exponent).
vec3 compute_point_light(vec3 n, vec3 to_light, vec3 v, vec3 albedo, vec3 light_color,
                         float radius, float exponent, vec3 specular_tint, vec3 f0,
                         float roughness, float metallic) {
    float distance = length(to_light);

    if (distance <= 0.0001) {
        return vec3(0.0);
    }

    vec3 l = to_light / distance;
    vec3 h = normalize(v + l);
    float falloff = clamp(1.0 - distance / radius, 0.0, 1.0);
    vec3 radiance = light_color * pow(falloff, exponent);

    float ndf = distribution_ggx(n, h, roughness);
    float g = geometry_smith(n, v, l, roughness);
    vec3 f = fresnel_schlick(max(dot(h, v), 0.0), f0);
    vec3 diffuse = (1.0 - metallic) * (vec3(1.0) - f);
    float nl = max(dot(n, l), 0.0);
    float denominator = 4.0 * max(dot(n, v), 0.0) * nl;
    vec3 specular = ndf * g * f / max(denominator, 0.001);

    return (diffuse * albedo / kPi + specular * specular_tint) * radiance * nl;
}

void main() {
    vec4 color = texture(uTexture, vTexCoord) * vColor;

    float metallic = uLayerMetallic;
    float roughness = uLayerRoughness;
    vec4 component_maps = vec4(0.0);

    if (uHasPbrMask) {
        component_maps = texture(uPbrMask, vTexCoord);

        if (uMetallicMap) {
            metallic = component_maps.r;
        }

        if (uRoughnessMap) {
            roughness = component_maps.g;
        }
    }

    vec3 normal = vec3(0.0, 0.0, 1.0);
    vec2 normalXY = vec2(0.0);
    float normalZ = 1.0;

    if (uHasNormalMap) {
        vec2 encoded = texture(uNormalMap, vTexCoord).xy * 2.0 - 1.0;
        normalZ = sqrt(max(0.0, 1.0 - dot(encoded, encoded)));
        normalXY = uTangent * encoded.x + uBitangent * encoded.y;
        normal = normalize(vec3(normalXY, normalZ));
    }

    if (uLit) {
        // 2D scenes are orthographic, so the view vector is constant.
        vec3 view = vec3(0.0, 0.0, 1.0);
        vec3 f0 = mix(vec3(0.04), color.rgb, metallic);
        vec3 light = vec3(0.0);

        for (int index = 0; index < uLightCount; ++index) {
            vec4 position = uLightPosition[index];
            vec4 light_color = uLightColor[index];
            light += compute_point_light(normal, position.xyz - vWorldPos, view, color.rgb,
                light_color.rgb, position.w, light_color.w, uSpecularTint, f0, roughness, metallic);
        }

        if (uEmissiveMap) {
            light = max(light, uEmissiveColor * color.rgb
                * (component_maps.a * uEmissiveBrightness));
        }

        color.rgb = uLightAmbient * color.rgb + light;
    }

    if (uReflective) {
        float fresnel = max(0.001, normalZ);
        vec2 screenUV = vClip.xy / vClip.w * 0.5 + 0.5;
        float aspect = uScreenSize.x / uScreenSize.y;
        screenUV += normalXY * vec2(0.15, 0.15 * aspect) * pow(fresnel, 4.0) * uReflectivityDistance;

        float reflectivity = uReflectivity;

        if (uReflectionMap) {
            reflectivity *= component_maps.b;
        }

        vec3 reflection = textureLod(uReflection, screenUV, roughness * uMipCount).rgb;
        reflection *= (1.0 - fresnel) * reflectivity;
        reflection = pow(max(vec3(0.001), reflection), vec3(2.0 - metallic));
        color.rgb += clamp(reflection, 0.0, 1.0) * fresnel;
    }

    fragColor = color;
}
)";

    void apply_blend (BlendMode mode) {
	glEnable (GL_BLEND);

	switch (mode) {
	    case BlendMode::normal:
	    case BlendMode::translucent:
		glBlendFuncSeparate (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		break;
	    case BlendMode::additive:
		glBlendFuncSeparate (GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE);
		break;
	}
    }

} // namespace

Renderer::~Renderer () { this->destroy (); }

Renderer::Renderer (Renderer&& other) noexcept :
    shader_ (std::move (other.shader_)), vao_ (other.vao_), vbo_ (other.vbo_), white_ (std::move (other.white_)),
    neutral_normal_ (std::move (other.neutral_normal_)), reflection_ (std::move (other.reflection_)),
    reflection_width_ (other.reflection_width_), reflection_height_ (other.reflection_height_),
    vertices_ (std::move (other.vertices_)), current_texture_ (other.current_texture_),
    current_blend_ (other.current_blend_), light_positions_ (other.light_positions_),
    light_colors_ (other.light_colors_), light_count_ (other.light_count_), ambient_ (other.ambient_),
    projection_ (other.projection_), framebuffer_width_ (other.framebuffer_width_),
    framebuffer_height_ (other.framebuffer_height_), draw_calls_ (other.draw_calls_), in_frame_ (other.in_frame_) {
    other.vao_ = 0;
    other.vbo_ = 0;
    other.reflection_width_ = 0;
    other.reflection_height_ = 0;
    other.current_texture_ = nullptr;
    other.in_frame_ = false;
}

Renderer& Renderer::operator= (Renderer&& other) noexcept {
    if (this != &other) {
	this->destroy ();
	this->shader_ = std::move (other.shader_);
	this->vao_ = other.vao_;
	this->vbo_ = other.vbo_;
	this->white_ = std::move (other.white_);
	this->neutral_normal_ = std::move (other.neutral_normal_);
	this->reflection_ = std::move (other.reflection_);
	this->reflection_width_ = other.reflection_width_;
	this->reflection_height_ = other.reflection_height_;
	this->vertices_ = std::move (other.vertices_);
	this->current_texture_ = other.current_texture_;
	this->current_blend_ = other.current_blend_;
	this->light_positions_ = other.light_positions_;
	this->light_colors_ = other.light_colors_;
	this->light_count_ = other.light_count_;
	this->ambient_ = other.ambient_;
	this->projection_ = other.projection_;
	this->framebuffer_width_ = other.framebuffer_width_;
	this->framebuffer_height_ = other.framebuffer_height_;
	this->draw_calls_ = other.draw_calls_;
	this->in_frame_ = other.in_frame_;
	other.vao_ = 0;
	other.vbo_ = 0;
	other.reflection_width_ = 0;
	other.reflection_height_ = 0;
	other.current_texture_ = nullptr;
	other.in_frame_ = false;
    }

    return *this;
}

void Renderer::destroy () noexcept {
    if (this->vbo_ != 0) {
	glDeleteBuffers (1, &this->vbo_);
	this->vbo_ = 0;
    }

    if (this->vao_ != 0) {
	glDeleteVertexArrays (1, &this->vao_);
	this->vao_ = 0;
    }
}

std::expected<Renderer, core::Error> Renderer::create () {
    auto shader = Shader::build (vertex_source, fragment_source, "sprite renderer");
    if (!shader) {
	return std::unexpected (std::move (shader.error ()));
    }

    const std::array<std::uint8_t, 4> white_pixel { 255, 255, 255, 255 };
    auto white = Texture::from_rgba8 (1, 1, white_pixel, TextureOptions { .mipmaps = false, .nearest = true });
    if (!white) {
	return std::unexpected (std::move (white.error ()));
    }

    // Flat tangent-space normal, used when a reflective material has no map.
    const std::array<std::uint8_t, 4> normal_pixel { 128, 128, 255, 255 };
    auto neutral_normal
	= Texture::from_rgba8 (1, 1, normal_pixel, TextureOptions { .mipmaps = false, .nearest = true });
    if (!neutral_normal) {
	return std::unexpected (std::move (neutral_normal.error ()));
    }

    Renderer result;
    result.shader_ = std::move (*shader);
    result.white_ = std::move (*white);
    result.neutral_normal_ = std::move (*neutral_normal);

    glGenVertexArrays (1, &result.vao_);
    glGenBuffers (1, &result.vbo_);
    glBindVertexArray (result.vao_);
    glBindBuffer (GL_ARRAY_BUFFER, result.vbo_);

    constexpr GLsizei stride = sizeof (Vertex);
    glEnableVertexAttribArray (0);
    glVertexAttribPointer (0, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*> (offsetof (Vertex, x)));
    glEnableVertexAttribArray (1);
    glVertexAttribPointer (1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*> (offsetof (Vertex, u)));
    glEnableVertexAttribArray (2);
    glVertexAttribPointer (2, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*> (offsetof (Vertex, r)));
    glEnableVertexAttribArray (3);
    glVertexAttribPointer (3, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*> (offsetof (Vertex, z)));

    glBindVertexArray (0);
    glBindBuffer (GL_ARRAY_BUFFER, 0);

    if (!check_gl ("Renderer::create")) {
	return core::fail (core::ErrorCode::io, "OpenGL rejected the sprite renderer setup");
    }

    return result;
}

void Renderer::begin (int framebuffer_width, int framebuffer_height, core::Color clear_color) {
    this->framebuffer_width_ = framebuffer_width;
    this->framebuffer_height_ = framebuffer_height;
    this->vertices_.clear ();
    this->vertices_.reserve (4096);
    this->current_texture_ = nullptr;
    this->current_blend_ = BlendMode::normal;
    this->draw_calls_ = 0;
    this->in_frame_ = true;
    this->light_count_ = 0;
    this->ambient_ = {};

    glViewport (0, 0, framebuffer_width, framebuffer_height);
    glDisable (GL_DEPTH_TEST);
    glDisable (GL_CULL_FACE);
    glDisable (GL_SCISSOR_TEST);
    glEnable (GL_BLEND);
    glBlendFuncSeparate (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glClearColor (clear_color.r, clear_color.g, clear_color.b, clear_color.a);
    glClear (GL_COLOR_BUFFER_BIT);

    this->shader_.use ();
    this->shader_.set_int ("uTexture", 0);
    this->shader_.set_int ("uNormalMap", 1);
    this->shader_.set_int ("uReflection", 2);
    this->shader_.set_int ("uPbrMask", 3);
    this->shader_.set_int ("uReflective", 0);
    this->shader_.set_int ("uLit", 0);
    this->shader_.set_int ("uLightCount", 0);
    this->shader_.set_vec3 ("uLightAmbient", {});
    this->shader_.set_vec2 (
	"uScreenSize", core::Vec2 { static_cast<float> (framebuffer_width), static_cast<float> (framebuffer_height) }
    );
    const int max_dimension = std::max (framebuffer_width, framebuffer_height);
    this->shader_.set_float (
	"uMipCount", std::floor (std::log2 (static_cast<float> (std::max (max_dimension, 1)))) + 1.0F
    );

    this->projection_ = core::Mat4::ortho (
	0.0F, static_cast<float> (framebuffer_width), static_cast<float> (framebuffer_height), 0.0F, -1.0F, 1.0F
    );
    this->shader_.set_mat4 ("uProjection", this->projection_);

    glBindVertexArray (this->vao_);
}

void Renderer::set_projection (const core::Mat4& projection) {
    this->projection_ = projection;
    this->shader_.use ();
    this->shader_.set_mat4 ("uProjection", this->projection_);
}

void Renderer::prepare_draw (const Texture& texture, BlendMode blend) {
    if (this->current_texture_ != &texture || this->current_blend_ != blend) {
	this->flush ();
	this->current_texture_ = &texture;
	this->current_blend_ = blend;
    }
}

void Renderer::draw (
    const Texture& texture, const core::Rect& destination, const core::Rect& uv, core::Color tint, BlendMode blend
) {
    if (!this->in_frame_) {
	return;
    }

    this->prepare_draw (texture, blend);
    this->push_quad (destination, uv, tint);

    if (this->vertices_.size () >= 4096) {
	this->flush ();
    }
}

void Renderer::draw_quad (
    const Texture& texture, const std::array<core::Vec2, 4>& corners, const core::Rect& uv, core::Color tint,
    BlendMode blend, float z
) {
    if (!this->in_frame_) {
	return;
    }

    this->prepare_draw (texture, blend);
    this->push_quad (corners, uv, tint, z);

    if (this->vertices_.size () >= 4096) {
	this->flush ();
    }
}

void Renderer::draw_rect (const core::Rect& destination, core::Color color, BlendMode blend) {
    this->draw (this->white_, destination, core::Rect { 0.0F, 0.0F, 1.0F, 1.0F }, color, blend);
}

void Renderer::capture_frame () {
    if (!this->in_frame_ || this->framebuffer_width_ <= 0 || this->framebuffer_height_ <= 0) {
	return;
    }

    this->flush ();

    GLint vp[4] = {};
    glGetIntegerv (GL_VIEWPORT, vp);
    const int width = vp[2] > 0 ? vp[2] : this->framebuffer_width_;
    const int height = vp[3] > 0 ? vp[3] : this->framebuffer_height_;

    if (!this->reflection_.valid () || this->reflection_width_ != width || this->reflection_height_ != height) {
	const std::size_t size = static_cast<std::size_t> (width) * static_cast<std::size_t> (height) * 4U;
	const std::vector<std::uint8_t> blank (size, 0);
	auto texture = Texture::from_rgba8 (width, height, blank);

	if (!texture) {
	    core::log_warn ("cannot create the reflection buffer: ", texture.error ().message);
	    return;
	}

	this->reflection_ = std::move (*texture);
	this->reflection_width_ = width;
	this->reflection_height_ = height;
    }

    GLint current_draw_fbo = 0;
    glGetIntegerv (GL_DRAW_FRAMEBUFFER_BINDING, &current_draw_fbo);
    glBindFramebuffer (GL_FRAMEBUFFER, current_draw_fbo);
    this->reflection_.bind (0);
    glCopyTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height);
    glGenerateMipmap (GL_TEXTURE_2D);
    glBindTexture (GL_TEXTURE_2D, 0);
    glBindFramebuffer (GL_FRAMEBUFFER, current_draw_fbo);
}

void Renderer::draw_quad_reflective (
    const Texture& texture, const std::array<core::Vec2, 4>& corners, const core::Rect& uv, core::Color tint,
    BlendMode blend, const ReflectionOptions& options, float z
) {
    this->draw_quad_material (texture, corners, uv, tint, blend, z, nullptr, &options);
}

void Renderer::set_lighting (std::span<const PointLight> lights, core::Vec3 ambient) {
    this->light_count_ = std::min (static_cast<int> (lights.size ()), max_point_lights);
    this->ambient_ = ambient;

    for (int index = 0; index < this->light_count_; ++index) {
	const PointLight& light = lights[static_cast<std::size_t> (index)];
	this->light_positions_[static_cast<std::size_t> (index)]
	    = core::Vec4 { light.position.x, light.position.y, light.position.z, light.radius };
	this->light_colors_[static_cast<std::size_t> (index)]
	    = core::Vec4 { light.color.x * light.intensity, light.color.y * light.intensity,
			   light.color.z * light.intensity, light.exponent };
    }

    this->shader_.use ();
    this->shader_.set_int ("uLightCount", this->light_count_);
    this->shader_.set_vec4_array ("uLightPosition", this->light_positions_);
    this->shader_.set_vec4_array ("uLightColor", this->light_colors_);
    this->shader_.set_vec3 ("uLightAmbient", ambient);
}

void Renderer::draw_quad_material (
    const Texture& texture, const std::array<core::Vec2, 4>& corners, const core::Rect& uv, core::Color tint,
    BlendMode blend, float z, const LightingOptions* lighting, const ReflectionOptions* reflection
) {
    if (!this->in_frame_) {
	return;
    }

    const bool lit = lighting != nullptr;
    const bool reflective = reflection != nullptr && this->reflection_.valid ();

    if (!lit && !reflective) {
	this->draw_quad (texture, corners, uv, tint, blend, z);
	return;
    }

    this->flush ();
    this->shader_.use ();
    this->shader_.set_int ("uLit", lit ? 1 : 0);
    this->shader_.set_int ("uReflective", reflective ? 1 : 0);

    const Texture* normal_map = nullptr;

    if (lit) {
	normal_map = lighting->normal_map;
	this->shader_.set_float ("uLayerRoughness", lighting->roughness);
	this->shader_.set_float ("uLayerMetallic", lighting->metallic);
	this->shader_.set_vec3 ("uSpecularTint", lighting->specular_tint);
	this->shader_.set_vec3 ("uEmissiveColor", lighting->emissive_color);
	this->shader_.set_float ("uEmissiveBrightness", lighting->emissive_brightness);
	this->shader_.set_int ("uMetallicMap", lighting->metallic_map ? 1 : 0);
	this->shader_.set_int ("uRoughnessMap", lighting->roughness_map ? 1 : 0);
	this->shader_.set_int ("uReflectionMap", lighting->reflection_map ? 1 : 0);
	this->shader_.set_int ("uEmissiveMap", lighting->emissive_map ? 1 : 0);
	this->shader_.set_vec2 ("uTangent", lighting->tangent);
	this->shader_.set_vec2 ("uBitangent", lighting->bitangent);

	const bool has_mask = lighting->pbr_mask != nullptr && lighting->pbr_mask->valid ();
	this->shader_.set_int ("uHasPbrMask", has_mask ? 1 : 0);
    } else {
	normal_map = reflection->normal_map;
	this->shader_.set_float ("uLayerRoughness", reflection->roughness);
	this->shader_.set_float ("uLayerMetallic", reflection->metallic);
	this->shader_.set_int ("uMetallicMap", reflection->metallic_map ? 1 : 0);
	this->shader_.set_int ("uRoughnessMap", reflection->roughness_map ? 1 : 0);
	this->shader_.set_int ("uReflectionMap", reflection->reflection_map ? 1 : 0);
	this->shader_.set_int ("uEmissiveMap", 0);
	this->shader_.set_vec2 ("uTangent", reflection->tangent);
	this->shader_.set_vec2 ("uBitangent", reflection->bitangent);

	const bool has_mask = reflection->pbr_mask != nullptr && reflection->pbr_mask->valid ();
	this->shader_.set_int ("uHasPbrMask", has_mask ? 1 : 0);
    }

    if (reflective) {
	this->shader_.set_float ("uReflectivity", reflection->reflectivity);
	this->shader_.set_float ("uReflectivityDistance", reflection->reflectivity_distance);
    }

    const bool has_normal = normal_map != nullptr && normal_map->valid ();
    this->shader_.set_int ("uHasNormalMap", has_normal ? 1 : 0);

    if (has_normal) {
	normal_map->bind (1);
    } else {
	this->neutral_normal_.bind (1);
    }

    if (reflective) {
	this->reflection_.bind (2);
    }

    const Texture* pbr_mask = lit ? lighting->pbr_mask : reflection->pbr_mask;

    if (pbr_mask != nullptr && pbr_mask->valid ()) {
	pbr_mask->bind (3);
    } else {
	this->white_.bind (3);
    }

    this->current_texture_ = &texture;
    this->current_blend_ = blend;
    this->push_quad (corners, uv, tint, z);
    this->flush ();

    this->shader_.set_int ("uLit", 0);
    this->shader_.set_int ("uReflective", 0);
}

void Renderer::push_quad (const core::Rect& destination, const core::Rect& uv, core::Color tint, float z) {
    const std::array<core::Vec2, 4> corners
	= { core::Vec2 { destination.x, destination.y }, core::Vec2 { destination.right (), destination.y },
	    core::Vec2 { destination.right (), destination.bottom () },
	    core::Vec2 { destination.x, destination.bottom () } };
    this->push_quad (corners, uv, tint, z);
}

void Renderer::push_quad (const std::array<core::Vec2, 4>& corners, const core::Rect& uv, core::Color tint, float z) {
    const float u0 = uv.x;
    const float v0 = uv.y;
    const float u1 = uv.x + uv.width;
    const float v1 = uv.y + uv.height;

    const auto vertex = [&tint, z] (core::Vec2 position, float u, float v) {
	return Vertex { position.x, position.y, z, u, v, tint.r, tint.g, tint.b, tint.a };
    };

    const Vertex top_left = vertex (corners[0], u0, v0);
    const Vertex top_right = vertex (corners[1], u1, v0);
    const Vertex bottom_right = vertex (corners[2], u1, v1);
    const Vertex bottom_left = vertex (corners[3], u0, v1);

    this->vertices_.push_back (top_left);
    this->vertices_.push_back (bottom_left);
    this->vertices_.push_back (top_right);

    this->vertices_.push_back (top_right);
    this->vertices_.push_back (bottom_left);
    this->vertices_.push_back (bottom_right);
}

void Renderer::flush () {
    if (this->vertices_.empty () || this->current_texture_ == nullptr) {
	this->vertices_.clear ();
	return;
    }

    apply_blend (this->current_blend_);
    this->current_texture_->bind (0);

    glBindVertexArray (this->vao_);
    glBindBuffer (GL_ARRAY_BUFFER, this->vbo_);
    glBufferData (
	GL_ARRAY_BUFFER, static_cast<GLsizeiptr> (this->vertices_.size () * sizeof (Vertex)), this->vertices_.data (),
	GL_STREAM_DRAW
    );
    glDrawArrays (GL_TRIANGLES, 0, static_cast<GLsizei> (this->vertices_.size ()));
    glBindBuffer (GL_ARRAY_BUFFER, 0);

    ++this->draw_calls_;
    this->vertices_.clear ();
}

void Renderer::end () {
    if (!this->in_frame_) {
	return;
    }

    this->flush ();
    glBindVertexArray (0);
    glDisable (GL_BLEND);
    this->in_frame_ = false;
}

} // namespace we::gfx
