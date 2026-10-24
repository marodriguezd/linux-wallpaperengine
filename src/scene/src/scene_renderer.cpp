#include "we/scene/scene_renderer.hpp"

#include "we/core/log.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace we::scene {
namespace {

core::Vec2 rotate (core::Vec2 value, float angle_radians) {
    const float cosine = std::cos (angle_radians);
    const float sine = std::sin (angle_radians);
    return { value.x * cosine - value.y * sine, value.x * sine + value.y * cosine };
}

// scene.json places objects in a top-left, y-down space; the renderer draws in
// a centred, y-up space. Every object (images and lights) goes through here.
core::Vec2 scene_to_world (core::Vec2 point, int scene_width, int scene_height) {
    return { point.x - static_cast<float> (scene_width) * 0.5F,
             static_cast<float> (scene_height) * 0.5F - point.y };
}

gfx::BlendMode parse_blend (std::string_view value) {
    if (value == "additive") {
        return gfx::BlendMode::additive;
    }

    if (value == "translucent") {
        return gfx::BlendMode::translucent;
    }

    return gfx::BlendMode::normal;
}

} // namespace

std::expected<SceneRenderer, core::Error> SceneRenderer::build (const Scene& scene, const AssetContainer& assets) {
    SceneRenderer result;
    result.scene_width_ = std::max (scene.camera.width, 1);
    result.scene_height_ = std::max (scene.camera.height, 1);
    result.clear_color_ = scene.clear_color;

    // Wallpaper Engine's orthographic path translates the projection by the
    // saved camera eye and its view matrix cancels that offset again
    // (Camera::setOrthogonalProjection + lookAt), so scene placement is defined
    // by the centred scene bounds alone.
    result.view_projection_ = core::Mat4::ortho (
        -static_cast<float> (result.scene_width_) * 0.5F, static_cast<float> (result.scene_width_) * 0.5F,
        -static_cast<float> (result.scene_height_) * 0.5F, static_cast<float> (result.scene_height_) * 0.5F,
        -1000.0F, 1000.0F
    );
    result.ambient_ = scene.ambient;

    for (const auto& light : scene.lights) {
        gfx::PointLight converted;
        const auto position = scene_to_world ({ light.origin.x, light.origin.y },
                                              result.scene_width_, result.scene_height_);
        converted.position = core::Vec3 { position.x, position.y, light.origin.z };
        converted.color = core::Vec3 { light.color.r, light.color.g, light.color.b };
        converted.intensity = light.intensity;
        converted.radius = light.radius;
        converted.exponent = light.exponent;
        result.lights_.push_back (converted);
    }

    if (result.lights_.size () > static_cast<std::size_t> (gfx::max_point_lights)) {
        core::log_info ("scene: only the first ", gfx::max_point_lights, " of ", scene.lights.size (),
                        " point lights are rendered");
        result.lights_.resize (static_cast<std::size_t> (gfx::max_point_lights));
    }

    const auto load_extra_texture = [&assets, &result] (const std::string& path,
                                                        const char* role) -> std::shared_ptr<gfx::Texture> {
        if (path.empty ()) {
            return nullptr;
        }

        if (const auto cached = result.textures_.find (path); cached != result.textures_.end ()) {
            return cached->second;
        }

        auto data = load_texture_data (assets, path);

        if (!data) {
            core::log_warn ("cannot load ", role, " '", path, "': ", data.error ().message);
            return nullptr;
        }

        if (data->is_video) {
            core::log_warn (role, " '", path, "' is a video texture");
            return nullptr;
        }

        auto uploaded = upload_texture (*data);

        if (!uploaded) {
            core::log_warn ("cannot upload ", role, " '", path, "': ", uploaded.error ().message);
            return nullptr;
        }

        auto texture = std::make_shared<gfx::Texture> (std::move (*uploaded));
        result.textures_.emplace (path, texture);
        result.texture_data_.emplace (path, std::move (*data));
        return texture;
    };

    for (const auto& image : scene.images) {
        if (!image.visible || image.texture_path.empty ()) {
            continue;
        }

        std::shared_ptr<gfx::Texture> texture;
        const auto cached = result.textures_.find (image.texture_path);
        std::unique_ptr<VideoPlayer> video;

        if (cached != result.textures_.end ()) {
            texture = cached->second;
        } else {
            auto data = load_texture_data (assets, image.texture_path);

            if (!data) {
                core::log_warn ("cannot load texture '", image.texture_path, "': ", data.error ().message);
                continue;
            }

            if (data->is_video) {
                auto player = VideoPlayer::create ();

                if (!player) {
                    core::log_warn ("cannot create video for '", image.texture_path, "': ", player.error ().message);
                    continue;
                }

                std::vector<std::byte> stream (data->video.size ());
                std::memcpy (stream.data (), data->video.data (), data->video.size ());
                auto loaded = (*player)->load (stream, image.texture_path);

                if (!loaded) {
                    core::log_warn ("cannot load video '", image.texture_path, "': ", loaded.error ().message);
                    continue;
                }

                (*player)->set_loop (true);
                (*player)->play ();
                video = std::move (*player);
                // Video layers have no pixel mipmap to upload; the first frame sets the size.
                data->width = image.size.x > 0.0F ? static_cast<int> (image.size.x) : 1;
                data->height = image.size.y > 0.0F ? static_cast<int> (image.size.y) : 1;
                data->texture_width = data->width;
                data->texture_height = data->height;
            }

            if (!data->is_video) {
                auto uploaded = upload_texture (*data);

                if (!uploaded) {
                    core::log_warn ("cannot upload texture '", image.texture_path, "': ", uploaded.error ().message);
                    continue;
                }

                texture = std::make_shared<gfx::Texture> (std::move (*uploaded));
            }

            result.textures_.emplace (image.texture_path, texture);
            result.texture_data_.emplace (image.texture_path, std::move (*data));
        }

        const auto metadata = result.texture_data_.find (image.texture_path);

        if (metadata == result.texture_data_.end ()) {
            continue;
        }

        std::shared_ptr<gfx::Texture> normal_texture;

        if (image.reflective) {
            core::log_debug ("image '", image.name, "': reflective material, normal map '",
                             image.normal_texture_path.empty () ? "<none>" : image.normal_texture_path, "'");
        }

        if (!image.normal_texture_path.empty ()) {
            normal_texture = load_extra_texture (image.normal_texture_path, "normal map");
        }

        std::shared_ptr<gfx::Texture> pbr_mask_texture;

        if (!image.pbr_mask_texture_path.empty ()) {
            pbr_mask_texture = load_extra_texture (image.pbr_mask_texture_path, "PBR mask");
        }

        float width = image.size.x;
        float height = image.size.y;

        if (width <= 0.0F || height <= 0.0F) {
            width = static_cast<float> (metadata->second.width);
            height = static_cast<float> (metadata->second.height);
        }

        if (width <= 0.0F || height <= 0.0F) {
            continue;
        }

        if (image.fullscreen) {
            width = static_cast<float> (result.scene_width_);
            height = static_cast<float> (result.scene_height_);
        }

        const float scaled_width = width * image.scale.x;
        const float scaled_height = height * image.scale.y;
        core::Vec2 center { image.origin.x, image.origin.y };

        if (image.fullscreen) {
            center = { static_cast<float> (result.scene_width_) * 0.5F,
                       static_cast<float> (result.scene_height_) * 0.5F };
        } else {
            if (image.alignment.find ("top") != std::string::npos) {
                center.y -= scaled_height * 0.5F;
            } else if (image.alignment.find ("bottom") != std::string::npos) {
                center.y += scaled_height * 0.5F;
            }

            if (image.alignment.find ("left") != std::string::npos) {
                center.x += scaled_width * 0.5F;
            } else if (image.alignment.find ("right") != std::string::npos) {
                center.x -= scaled_width * 0.5F;
            }
        }

        center = scene_to_world (center, result.scene_width_, result.scene_height_);

        // Wallpaper Engine stores angles in radians and applies them in its
        // y-down scene space, so a positive z angle turns clockwise on screen.
        // The world space is y-up, hence the negation.
        const float angle = -image.angles.z;
        const float half_width = scaled_width * 0.5F;
        const float half_height = scaled_height * 0.5F;

        const std::array<core::Vec2, 4> local = {
            core::Vec2 { -half_width, half_height },
            core::Vec2 { half_width, half_height },
            core::Vec2 { half_width, -half_height },
            core::Vec2 { -half_width, -half_height },
        };

        Layer layer;
        layer.texture = std::move (texture);
        layer.video = std::move (video);
        layer.blend = parse_blend (image.blending);
        layer.tint = core::Color { image.color.r, image.color.g, image.color.b, image.color.a * image.alpha };
        layer.z = image.origin.z;
        layer.lighting = image.lighting;
        layer.reflective = image.reflective && normal_texture != nullptr;
        layer.normal_texture = std::move (normal_texture);
        layer.pbr_mask_texture = std::move (pbr_mask_texture);
        layer.metallic_map = image.metallic_map;
        layer.roughness_map = image.roughness_map;
        layer.reflection_map = image.reflection_map;
        layer.emissive_map = image.emissive_map;
        layer.specular_tint = image.specular_tint;
        layer.emissive_color = image.emissive_color;
        layer.emissive_brightness = image.emissive_brightness;
        layer.reflectivity = image.reflectivity;
        layer.roughness = image.roughness;
        layer.metallic = image.metallic;
        layer.reflectivity_distance = image.reflectivity_distance;
        layer.tangent = rotate (core::Vec2 { 1.0F, 0.0F }, angle);
        layer.bitangent = rotate (core::Vec2 { 0.0F, 1.0F }, angle);

        for (std::size_t index = 0; index < local.size (); ++index) {
            const auto rotated = rotate (local[index], angle);
            layer.corners[index] = center + rotated;
        }

        // The TEX payload stores the cropped bitmap (the mipmap dimensions are
        // its image width/height), while the header's full width/height only
        // records the canvas it was cropped from. Sampling the whole uploaded
        // texture is therefore what reproduces Wallpaper Engine's UVs.
        layer.uv = core::Rect { 0.0F, 0.0F, 1.0F, 1.0F };

        result.layers_.push_back (std::move (layer));
    }

    core::log_info ("scene: ", result.layers_.size (), " image layers, ", result.lights_.size (),
                    " point lights, ", scene.skipped_objects, " objects not rendered");
    return result;
}

bool SceneRenderer::update_videos () {
    bool changed = false;

    for (auto& layer : this->layers_) {
        if (layer.video != nullptr && layer.video->update ()) {
            layer.texture = std::make_shared<gfx::Texture> (
                gfx::Texture::view (layer.video->texture ().id (), layer.video->width (), layer.video->height ())
            );
            changed = true;
        }
    }

    return changed;
}

void SceneRenderer::report_video_swap () {
    for (auto& layer : this->layers_) {
        if (layer.video != nullptr) {
            layer.video->report_swap ();
        }
    }
}

void SceneRenderer::render (gfx::Renderer& renderer) const {
    renderer.set_projection (this->view_projection_);
    renderer.set_lighting (this->lights_, this->ambient_);

    bool frame_captured = false;

    for (const auto& layer : this->layers_) {
        if (layer.texture == nullptr || !layer.texture->valid ()) {
            continue;
        }

        const bool reflective = layer.reflective && layer.normal_texture != nullptr
            && layer.normal_texture->valid ();

        if (layer.lighting || reflective) {
            if (reflective) {
                // One snapshot per run: later layers reuse the frame as it looked
                // before the first reflective layer of the run.
                if (!frame_captured) {
                    renderer.capture_frame ();
                    frame_captured = true;
                }
            }

            gfx::LightingOptions lighting_options;
            lighting_options.normal_map = layer.normal_texture.get ();
            lighting_options.pbr_mask = layer.pbr_mask_texture.get ();
            lighting_options.roughness = layer.roughness;
            lighting_options.metallic = layer.metallic;
            lighting_options.specular_tint = layer.specular_tint;
            lighting_options.emissive_color = layer.emissive_color;
            lighting_options.emissive_brightness = layer.emissive_brightness;
            lighting_options.metallic_map = layer.metallic_map;
            lighting_options.roughness_map = layer.roughness_map;
            lighting_options.reflection_map = layer.reflection_map;
            lighting_options.emissive_map = layer.emissive_map;
            lighting_options.tangent = layer.tangent;
            lighting_options.bitangent = layer.bitangent;

            gfx::ReflectionOptions reflection_options;
            reflection_options.normal_map = layer.normal_texture.get ();
            reflection_options.pbr_mask = layer.pbr_mask_texture.get ();
            reflection_options.metallic_map = layer.metallic_map;
            reflection_options.roughness_map = layer.roughness_map;
            reflection_options.reflection_map = layer.reflection_map;
            reflection_options.reflectivity = layer.reflectivity;
            reflection_options.roughness = layer.roughness;
            reflection_options.metallic = layer.metallic;
            reflection_options.reflectivity_distance = layer.reflectivity_distance;
            reflection_options.tangent = layer.tangent;
            reflection_options.bitangent = layer.bitangent;

            renderer.draw_quad_material (
                *layer.texture, layer.corners, layer.uv, layer.tint, layer.blend, layer.z,
                layer.lighting ? &lighting_options : nullptr, reflective ? &reflection_options : nullptr
            );

            if (!reflective) {
                frame_captured = false;
            }
        } else {
            renderer.draw_quad (*layer.texture, layer.corners, layer.uv, layer.tint, layer.blend, layer.z);
            frame_captured = false;
        }
    }
}

} // namespace we::scene
