#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

#include "we/core/error.hpp"
#include "we/scene/asset_container.hpp"

namespace we::scene {

enum class WallpaperType : std::uint8_t { scene, video, web, unknown };

[[nodiscard]] std::string_view to_string (WallpaperType type) noexcept;
[[nodiscard]] WallpaperType wallpaper_type_from_string (std::string_view value) noexcept;

struct Project {
    std::string title;
    std::string file;
    std::string workshop_id;
    WallpaperType type = WallpaperType::unknown;
};

// Reads project.json. Falls back to a scene project when a container only has
// scene.pkg/scene.json (useful for copied workshop content).
[[nodiscard]] std::expected<Project, core::Error> load_project (const AssetContainer& assets);

} // namespace we::scene
