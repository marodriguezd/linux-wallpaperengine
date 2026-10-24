#include "we/scene/project.hpp"

#include "we/core/log.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>

namespace we::scene {
namespace {

std::string lowercase (std::string value) {
    std::ranges::transform (value, value.begin (), [] (unsigned char character) {
        return static_cast<char> (std::tolower (character));
    });
    return value;
}

std::string workshop_id (const nlohmann::json& root) {
    const auto found = root.find ("workshopid");

    if (found == root.end ()) {
        return {};
    }

    if (found->is_string ()) {
        return found->get<std::string> ();
    }

    if (found->is_number ()) {
        return std::to_string (found->get<long long> ());
    }

    return {};
}

} // namespace

std::string_view to_string (WallpaperType type) noexcept {
    switch (type) {
        case WallpaperType::scene:
            return "scene";
        case WallpaperType::video:
            return "video";
        case WallpaperType::web:
            return "web";
        case WallpaperType::unknown:
            return "unknown";
    }

    return "unknown";
}

WallpaperType wallpaper_type_from_string (std::string_view value) noexcept {
    if (value == "scene") {
        return WallpaperType::scene;
    }

    if (value == "video") {
        return WallpaperType::video;
    }

    if (value == "web") {
        return WallpaperType::web;
    }

    return WallpaperType::unknown;
}

std::expected<Project, core::Error> load_project (const AssetContainer& assets) {
    if (!assets.exists ("project.json")) {
        if (assets.exists ("scene.json")) {
            return Project { .title = assets.root ().filename ().string (),
                             .file = "scene.json",
                             .workshop_id = {},
                             .type = WallpaperType::scene };
        }

        return core::fail (core::ErrorCode::not_found, "project.json is missing from " + assets.root ().string ());
    }

    const auto source = assets.read_text ("project.json");

    if (!source) {
        return std::unexpected (std::move (source.error ()));
    }

    nlohmann::json root;

    try {
        root = nlohmann::json::parse (*source);
    } catch (const std::exception& exception) {
        return core::fail (
            core::ErrorCode::malformed_data,
            "invalid project.json in " + assets.root ().string () + ": " + exception.what ()
        );
    }

    if (!root.is_object ()) {
        return core::fail (core::ErrorCode::malformed_data, "project.json must contain an object");
    }

    Project project;
    project.title = root.value ("title", assets.root ().filename ().string ());
    project.file = root.value ("file", "");
    project.workshop_id = workshop_id (root);
    project.type = wallpaper_type_from_string (lowercase (root.value ("type", "")));

    if (project.file.empty ()) {
        return core::fail (core::ErrorCode::malformed_data, "project.json has no 'file' entry");
    }

    if (project.type == WallpaperType::unknown) {
        core::log_warn ("unknown wallpaper type in ", assets.root ().string (), "; treating it as a scene");
        project.type = WallpaperType::scene;
    }

    return project;
}

} // namespace we::scene
