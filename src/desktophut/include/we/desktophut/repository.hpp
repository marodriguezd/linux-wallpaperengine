#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "we/core/error.hpp"
#include "we/steam/steam_locator.hpp"

namespace we::desktophut {

inline constexpr std::string_view steam_deletion_refusal_message
    = "Los fondos de Wallpaper Engine deben gestionarse y eliminarse desde Steam / Wallpaper Engine.";

struct WallpaperItem {
    std::string id; // ID or slug (e.g. "1234567" or "neon-city-corvette-live-wallpaper-eeye")
    std::string title; // Title from project.json
    std::string type; // "video", "scene", "web", etc.
    std::string file; // Main entry file (e.g. "wallpaper.mp4", "scene.json")
    std::string path; // Full filesystem directory path
    std::string preview; // Preview image relative file or URL
    std::string source; // "wallpaperengine", "desktophut", "local"
};

// Root directory for local wallpapers:
// Returns $XDG_DATA_HOME/linux-wallpaperengine/wallpapers (fallback ~/.local/share/linux-wallpaperengine/wallpapers)
[[nodiscard]] std::filesystem::path local_wallpapers_dir ();

// Subdirectory for DesktopHut: local_wallpapers_dir() / "desktophut"
[[nodiscard]] std::filesystem::path desktophut_wallpapers_dir ();

// Enumerates all local wallpapers from local_wallpapers_dir() and subdirectories.
[[nodiscard]] std::vector<WallpaperItem>
list_local_wallpapers (const std::filesystem::path& root = local_wallpapers_dir ());

// Enumerates only DesktopHut wallpapers from desktophut_wallpapers_dir().
[[nodiscard]] std::vector<WallpaperItem>
list_desktophut_wallpapers (const std::filesystem::path& dir = desktophut_wallpapers_dir ());

// Enumerates all wallpapers: Steam Workshop items + local wallpapers.
[[nodiscard]] std::vector<WallpaperItem> list_all_wallpapers (const steam::SteamDiscovery& discovery);

// Checks if a given path or id belongs to Steam Workshop or Wallpaper Engine installations.
[[nodiscard]] bool is_steam_wallpaper (std::string_view target_or_path, const steam::SteamDiscovery& discovery);

// Resolves a target string (direct path, Steam workshop ID, desktophut slug, or local ID) to a filesystem path.
[[nodiscard]] std::optional<std::filesystem::path>
resolve_wallpaper (std::string_view target, const steam::SteamDiscovery& discovery);

// Deletes a wallpaper if local. Strictly refuses if Steam workshop wallpaper!
// Returns success (void) or core::Error.
[[nodiscard]] std::expected<void, core::Error>
delete_wallpaper (std::string_view target, const steam::SteamDiscovery& discovery);

// Deletes a DesktopHut wallpaper specifically by ID or slug.
[[nodiscard]] std::expected<void, core::Error>
delete_desktophut_wallpaper (std::string_view id, const steam::SteamDiscovery& discovery);

} // namespace we::desktophut
