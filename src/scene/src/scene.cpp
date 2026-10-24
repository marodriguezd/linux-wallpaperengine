#include "we/scene/scene.hpp"

#include "we/core/log.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <optional>
#include <sstream>

namespace we::scene {
namespace {

using nlohmann::json;

std::string lowercase (std::string value) {
    std::ranges::transform (value, value.begin (), [] (unsigned char character) {
        return static_cast<char> (std::tolower (character));
    });
    return value;
}

std::optional<float> as_float (const json& value) {
    if (value.is_number ()) {
        return value.get<float> ();
    }

    if (value.is_object ()) {
        const auto nested = value.find ("value");

        if (nested != value.end () && nested->is_number ()) {
            return nested->get<float> ();
        }
    }

    return std::nullopt;
}

float number (const json& object, const char* key, float fallback) {
    const auto found = object.find (key);

    if (found == object.end ()) {
        return fallback;
    }

    return as_float (*found).value_or (fallback);
}

bool boolean (const json& object, const char* key, bool fallback) {
    const auto found = object.find (key);

    if (found == object.end ()) {
        return fallback;
    }

    if (found->is_boolean ()) {
        return found->get<bool> ();
    }

    return fallback;
}

std::string text (const json& object, const char* key, std::string fallback = {}) {
    const auto found = object.find (key);

    if (found == object.end ()) {
        return fallback;
    }

    if (found->is_string ()) {
        return found->get<std::string> ();
    }

    if (found->is_number ()) {
        return std::to_string (found->get<long long> ());
    }

    return fallback;
}

// Wallpaper Engine serialises vectors as "x y z" strings; the synthetic
// fixtures use JSON arrays. Both forms are accepted everywhere.
core::Vec3 vec3 (const json& object, const char* key, core::Vec3 fallback) {
    const auto found = object.find (key);

    if (found == object.end ()) {
        return fallback;
    }

    const json* value = &*found;

    if (value->is_object ()) {
        const auto nested = value->find ("value");

        if (nested != value->end ()) {
            value = &*nested;
        }
    }

    if (value->is_string ()) {
        std::istringstream stream (value->get<std::string> ());
        float x = 0.0F;
        float y = 0.0F;
        float z = 0.0F;

        if (!(stream >> x >> y >> z)) {
            return fallback;
        }

        return core::Vec3 { x, y, z };
    }

    if (!value->is_array () || value->size () < 3) {
        return fallback;
    }

    return core::Vec3 { (*value)[0].get<float> (), (*value)[1].get<float> (), (*value)[2].get<float> () };
}

core::Vec2 vec2 (const json& object, const char* key, core::Vec2 fallback) {
    const auto found = object.find (key);

    if (found == object.end ()) {
        return fallback;
    }

    const json* value = &*found;

    if (value->is_object ()) {
        const auto nested = value->find ("value");

        if (nested != value->end ()) {
            value = &*nested;
        }
    }

    if (value->is_string ()) {
        std::istringstream stream (value->get<std::string> ());
        float x = 0.0F;
        float y = 0.0F;

        if (!(stream >> x >> y)) {
            return fallback;
        }

        return core::Vec2 { x, y };
    }

    if (!value->is_array () || value->size () < 2) {
        return fallback;
    }

    return core::Vec2 { (*value)[0].get<float> (), (*value)[1].get<float> () };
}

core::Color color (const json& object, const char* key, core::Color fallback) {
    const auto found = object.find (key);

    if (found == object.end ()) {
        return fallback;
    }

    const json* value = &*found;

    if (value->is_object ()) {
        const auto nested = value->find ("value");

        if (nested != value->end ()) {
            value = &*nested;
        }
    }

    if (value->is_string ()) {
        // Wallpaper Engine stores colours as "r g b" strings (components in 0..1).
        std::istringstream stream (value->get<std::string> ());
        float r = 0.0F;
        float g = 0.0F;
        float b = 0.0F;

        if (!(stream >> r >> g >> b)) {
            return fallback;
        }

        float a = 1.0F;
        stream >> a;
        return core::Color { r, g, b, a };
    }

    if (!value->is_array () || value->size () < 3) {
        return fallback;
    }

    core::Color result { (*value)[0].get<float> (), (*value)[1].get<float> (), (*value)[2].get<float> (),
                         value->size () > 3 ? (*value)[3].get<float> () : 1.0F };
    return result;
}

struct ModelDoc {
    std::string material;
    int width = 0;
    int height = 0;
    bool solid_layer = false;
    bool fullscreen = false;
};

struct PassDoc {
    std::string shader;
    std::string blending = "normal";
    bool lit = false;
    bool lighting = false;
    bool reflective = false;
    bool pbr_masks = false;
    bool metallic_map = false;
    bool roughness_map = false;
    bool reflection_map = false;
    bool emissive_map = false;
    float reflectivity = 1.0F;
    float roughness = 0.7F;
    float metallic = 0.0F;
    float reflectivity_distance = 4.0F;
    core::Vec3 specular_tint { 1.0F, 1.0F, 1.0F };
    core::Vec3 emissive_color { 1.0F, 1.0F, 1.0F };
    float emissive_brightness = 1.0F;
    std::vector<std::string> textures;
};

std::expected<ModelDoc, core::Error> load_model (const AssetContainer& assets, const std::string& path) {
    auto source = assets.read_text (path);

    if (!source) {
        return std::unexpected (std::move (source.error ()));
    }

    json root;

    try {
        root = json::parse (*source);
    } catch (const std::exception& exception) {
        return core::fail (
            core::ErrorCode::malformed_data, "invalid model '" + path + "': " + exception.what ()
        );
    }

    ModelDoc model;
    model.material = text (root, "material");

    if (model.material.empty ()) {
        return core::fail (core::ErrorCode::malformed_data, "model '" + path + "' has no material");
    }

    model.width = static_cast<int> (number (root, "width", 0.0F));
    model.height = static_cast<int> (number (root, "height", 0.0F));
    model.solid_layer = boolean (root, "solidlayer", false);
    model.fullscreen = boolean (root, "fullscreen", false);
    return model;
}

std::expected<PassDoc, core::Error> load_material (const AssetContainer& assets, const std::string& path) {
    auto source = assets.read_text (path);

    if (!source) {
        return std::unexpected (std::move (source.error ()));
    }

    json root;

    try {
        root = json::parse (*source);
    } catch (const std::exception& exception) {
        return core::fail (
            core::ErrorCode::malformed_data, "invalid material '" + path + "': " + exception.what ()
        );
    }

    const auto passes = root.find ("passes");

    if (passes == root.end () || !passes->is_array () || passes->empty ()) {
        return core::fail (core::ErrorCode::malformed_data, "material '" + path + "' has no passes");
    }

    const auto& first = (*passes)[0];

    if (!first.is_object ()) {
        return core::fail (core::ErrorCode::malformed_data, "material '" + path + "' has an invalid first pass");
    }

    PassDoc pass;
    pass.shader = text (first, "shader");
    pass.blending = lowercase (text (first, "blending", "normal"));

    const auto combos = first.find ("combos");

    if (combos != first.end () && combos->is_object ()) {
        const auto combo_enabled = [&combos] (const char* name) {
            const auto entry = combos->find (name);
            return entry != combos->end () && entry->is_number () && entry->get<int> () != 0;
        };

        pass.lighting = combo_enabled ("LIGHTING");
        pass.reflective = combo_enabled ("REFLECTION");
        pass.lit = pass.lighting || pass.reflective;
        pass.pbr_masks = combo_enabled ("PBRMASKS");
        pass.metallic_map = combo_enabled ("METALLIC_MAP");
        pass.roughness_map = combo_enabled ("ROUGHNESS_MAP");
        pass.reflection_map = combo_enabled ("REFLECTION_MAP");
        pass.emissive_map = combo_enabled ("EMISSIVE_MAP");
    }

    const auto constants = first.find ("constantshadervalues");

    if (constants != first.end () && constants->is_object ()) {
        pass.reflectivity = number (*constants, "reflectivity", pass.reflectivity);
        pass.roughness = number (*constants, "roughness", pass.roughness);
        pass.metallic = number (*constants, "metallic", pass.metallic);
        pass.reflectivity_distance = number (*constants, "reflectivitydistance", pass.reflectivity_distance);
        pass.emissive_brightness = number (*constants, "emissivebrightness", pass.emissive_brightness);

        // Colours are stored as "r g b" strings, like object colours.
        pass.specular_tint = vec3 (*constants, "speculartint", pass.specular_tint);
        pass.emissive_color = vec3 (*constants, "emissivecolor", pass.emissive_color);
    }

    const auto textures = first.find ("textures");

    if (textures != first.end () && textures->is_array ()) {
        // Material slots map to g_Texture0..N, so keep empty slots in place:
        // the normal map is always slot 1 and the PBR mask always slot 2.
        pass.textures.reserve (textures->size ());

        for (const auto& entry : *textures) {
            if (entry.is_string ()) {
                pass.textures.push_back (entry.get<std::string> ());
            } else if (entry.is_object ()) {
                pass.textures.push_back (text (entry, "name"));
            } else {
                pass.textures.emplace_back ();
            }
        }
    }

    return pass;
}

std::optional<SceneLight> parse_point_light (const json& object) {
    const auto type = object.find ("light");

    if (type == object.end () || !type->is_string () || type->get<std::string> () != "lpoint") {
        return std::nullopt;
    }

    SceneLight light;
    light.id = object.contains ("id") && object["id"].is_number () ? object["id"].get<int> () : 0;
    light.origin = vec3 (object, "origin", light.origin);
    light.color = color (object, "color", light.color);
    light.intensity = number (object, "intensity", light.intensity);
    light.radius = number (object, "radius", light.radius);
    light.exponent = number (object, "exponent", light.exponent);
    light.density = number (object, "density", light.density);
    return light;
}

std::optional<SceneImage> parse_image (const AssetContainer& assets, const json& object) {
    SceneImage image;
    image.id = object.contains ("id") && object["id"].is_number () ? object["id"].get<int> () : 0;
    image.name = text (object, "name", "image-" + std::to_string (image.id));
    image.origin = vec3 (object, "origin", { 0.0F, 0.0F, 0.0F });
    image.scale = vec3 (object, "scale", { 1.0F, 1.0F, 1.0F });
    image.angles = vec3 (object, "angles", { 0.0F, 0.0F, 0.0F });
    image.alpha = number (object, "alpha", 1.0F);
    image.color = color (object, "color", { 1.0F, 1.0F, 1.0F, 1.0F });
    image.size = vec2 (object, "size", { 0.0F, 0.0F });
    image.parallax_depth = vec2 (object, "parallaxDepth", { 0.0F, 0.0F });
    image.visible = boolean (object, "visible", true);
    image.alignment = lowercase (text (object, "horizontalalign", text (object, "alignment", "center")));

    const auto effects = object.find ("effects");

    if (effects != object.end () && effects->is_array ()) {
        image.effect_count = static_cast<int> (effects->size ());
    }

    const std::string model_path = text (object, "image");

    if (model_path.empty ()) {
        return std::nullopt;
    }

    auto model = load_model (assets, model_path);

    if (!model) {
        core::log_warn ("skipping image '", image.name, "': ", model.error ().message);
        return std::nullopt;
    }

    image.fullscreen = model->fullscreen;
    image.solid_layer = model->solid_layer;

    if (image.size.x <= 0.0F || image.size.y <= 0.0F) {
        if (model->width > 0 && model->height > 0) {
            image.size = { static_cast<float> (model->width), static_cast<float> (model->height) };
        }
    }

    auto pass = load_material (assets, model->material);

    if (!pass) {
        core::log_warn ("skipping image '", image.name, "': ", pass.error ().message);
        return std::nullopt;
    }

    image.shader = pass->shader;
    image.blending = pass->blending;
    image.lit_material = pass->lit;
    image.lighting = pass->lighting;
    image.reflective = pass->reflective;
    image.reflectivity = pass->reflectivity;
    image.roughness = pass->roughness;
    image.metallic = pass->metallic;
    image.reflectivity_distance = pass->reflectivity_distance;
    image.metallic_map = pass->pbr_masks && pass->metallic_map;
    image.roughness_map = pass->pbr_masks && pass->roughness_map;
    image.reflection_map = pass->pbr_masks && pass->reflection_map;
    image.emissive_map = pass->pbr_masks && pass->emissive_map;
    image.specular_tint = pass->specular_tint;
    image.emissive_color = pass->emissive_color;
    image.emissive_brightness = pass->emissive_brightness;

    const auto texture_at = [&pass] (std::size_t index) -> std::string {
        return index < pass->textures.size () ? pass->textures[index] : std::string {};
    };

    // genericimage4 binds the albedo to g_Texture0, the normal map to
    // g_Texture1 and the PBR masks to g_Texture2.
    image.texture_path = texture_at (0);

    if (image.texture_path.empty ()) {
        core::log_warn ("image '", image.name, "' has no texture in material ", model->material);
        return std::nullopt;
    }

    if (pass->lit) {
        image.normal_texture_path = texture_at (1);
    }

    if (pass->pbr_masks) {
        image.pbr_mask_texture_path = texture_at (2);
    }

    // Names prefixed with _rt_ are render targets the engine creates at runtime
    // (e.g. _rt_FullFrameBuffer for post-processing), not files on disk.
    if (image.texture_path.starts_with ("_rt_")) {
        core::log_info (
            "image '", image.name, "': internal render target '", image.texture_path,
            "' is not implemented, layer skipped"
        );
        return std::nullopt;
    }

    return image;
}

} // namespace

std::expected<Scene, core::Error> load_scene (const AssetContainer& assets, const std::string& file) {
    auto source = assets.read_text (file);

    if (!source) {
        return std::unexpected (std::move (source.error ()));
    }

    json root;

    try {
        root = json::parse (*source);
    } catch (const std::exception& exception) {
        return core::fail (
            core::ErrorCode::malformed_data, "invalid scene '" + file + "': " + exception.what ()
        );
    }

    if (!root.is_object ()) {
        return core::fail (core::ErrorCode::malformed_data, "scene '" + file + "' must contain an object");
    }

    Scene scene;
    scene.source = file;

    const auto camera = root.find ("camera");

    if (camera != root.end () && camera->is_object ()) {
        scene.camera.center = vec3 (*camera, "center", scene.camera.center);
        scene.camera.eye = vec3 (*camera, "eye", scene.camera.eye);
        scene.camera.up = vec3 (*camera, "up", scene.camera.up);
        scene.camera.fov = number (*camera, "fov", scene.camera.fov);
        scene.camera.near_plane = number (*camera, "nearz", scene.camera.near_plane);
        scene.camera.far_plane = number (*camera, "farz", scene.camera.far_plane);
    }

    const auto general = root.find ("general");

    if (general != root.end () && general->is_object ()) {
        scene.clear_color = color (*general, "clearcolor", scene.clear_color);
        scene.ambient = vec3 (*general, "ambientcolor", scene.ambient);

        const auto projection = general->find ("orthogonalprojection");

        if (projection != general->end () && projection->is_object ()) {
            scene.camera.automatic_projection = boolean (*projection, "auto", false);
            scene.camera.width = static_cast<int> (number (*projection, "width", 0.0F));
            scene.camera.height = static_cast<int> (number (*projection, "height", 0.0F));
        }
    }

    const auto objects = root.find ("objects");

    if (objects == root.end () || !objects->is_array ()) {
        return core::fail (core::ErrorCode::malformed_data, "scene '" + file + "' has no objects array");
    }

    for (const auto& object : *objects) {
        if (!object.is_object ()) {
            ++scene.skipped_objects;
            continue;
        }

        if (object.contains ("image") && object["image"].is_string ()) {
            if (auto image = parse_image (assets, object); image.has_value ()) {
                scene.images.push_back (std::move (*image));
            } else {
                ++scene.skipped_objects;
            }
        } else if (object.contains ("light") && object["light"].is_string ()) {
            if (auto light = parse_point_light (object); light.has_value ()) {
                scene.lights.push_back (std::move (*light));
            } else {
                ++scene.skipped_objects;
            }
        } else {
            ++scene.skipped_objects;
        }
    }

    // Automatic projections take the bounding box of the transformed image
    // quads. Fullscreen layers follow the scene size, so they cannot define it.
    if (scene.camera.automatic_projection || scene.camera.width <= 0 || scene.camera.height <= 0) {
        float min_x = std::numeric_limits<float>::max ();
        float min_y = std::numeric_limits<float>::max ();
        float max_x = std::numeric_limits<float>::lowest ();
        float max_y = std::numeric_limits<float>::lowest ();
        bool have_bounds = false;

        for (const auto& image : scene.images) {
            if (!image.visible || image.fullscreen || image.size.x <= 0.0F || image.size.y <= 0.0F) {
                continue;
            }

            const float half_width = std::abs (image.size.x * image.scale.x) * 0.5F;
            const float half_height = std::abs (image.size.y * image.scale.y) * 0.5F;
            float center_x = image.origin.x;
            float center_y = image.origin.y;

            if (image.alignment.find ("top") != std::string::npos) {
                center_y -= half_height;
            } else if (image.alignment.find ("bottom") != std::string::npos) {
                center_y += half_height;
            }

            if (image.alignment.find ("left") != std::string::npos) {
                center_x += half_width;
            } else if (image.alignment.find ("right") != std::string::npos) {
                center_x -= half_width;
            }

            const float cosine = std::abs (std::cos (image.angles.z));
            const float sine = std::abs (std::sin (image.angles.z));
            const float extent_x = half_width * cosine + half_height * sine;
            const float extent_y = half_width * sine + half_height * cosine;

            min_x = std::min (min_x, center_x - extent_x);
            min_y = std::min (min_y, center_y - extent_y);
            max_x = std::max (max_x, center_x + extent_x);
            max_y = std::max (max_y, center_y + extent_y);
            have_bounds = true;
        }

        // The epsilon keeps trigonometric noise (e.g. cos(pi/2)) from rounding
        // an integer-sized bound up by one pixel.
        constexpr float tolerance = 1.0e-3F;

        if (have_bounds && max_x > min_x && max_y > min_y) {
            scene.camera.width = std::max (1, static_cast<int> (std::ceil (max_x - min_x - tolerance)));
            scene.camera.height = std::max (1, static_cast<int> (std::ceil (max_y - min_y - tolerance)));
        } else {
            scene.camera.width = 1920;
            scene.camera.height = 1080;
        }
    }

    if (scene.camera.near_plane == scene.camera.far_plane) {
        scene.camera.far_plane = scene.camera.near_plane + 1000.0F;
    }

    return scene;
}

} // namespace we::scene
