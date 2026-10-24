#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "we/core/error.hpp"

namespace we::steam {

enum class SteamInstallKind : std::uint8_t {
	native,
	flatpak,
	snap,
	proton_prefix,
	windows_mount,
	custom,
};

[[nodiscard]] std::string_view to_string (SteamInstallKind kind) noexcept;

struct SteamLibrary {
	std::filesystem::path root;
	std::filesystem::path steamapps;
	SteamInstallKind kind = SteamInstallKind::custom;

	bool has_wallpaper_engine = false;
	bool has_app_manifest = false;

	// `<root>/steamapps/common/wallpaper_engine` when installed here.
	std::filesystem::path wallpaper_engine_dir;

	// `<root>/steamapps/workshop/content/431960` when the workshop is present.
	std::filesystem::path workshop_content_dir;
};

struct SteamDiscoveryOptions {
	// Overrides used by tests. When `home` is set, environment variables are
	// ignored for library discovery so results are reproducible.
	std::optional<std::filesystem::path> home;
	std::optional<std::filesystem::path> xdg_data_home;

	// Additional library roots (a directory containing `steamapps`) that are
	// probed before scanning mounts.
	std::vector<std::filesystem::path> extra_library_roots;

	// Mount points to scan for Windows libraries. Empty selects defaults
	// (/mnt, /media and /run/media/<user>).
	std::vector<std::filesystem::path> mount_roots;

	bool scan_mounts = true;
	bool scan_proton_prefixes = true;

	std::uint32_t max_directory_visits = 20'000;
	std::uint32_t max_mount_depth = 3;
};

struct SteamDiscovery {
	std::vector<SteamLibrary> libraries;
	std::vector<std::filesystem::path> wallpaper_engine_installations;
	std::vector<std::string> warnings;
};

// Finds every Steam library that can contain Wallpaper Engine content:
// native Steam, Flatpak, Snap, Proton prefixes and mounted Windows drives.
class SteamLocator {
public:
	explicit SteamLocator (SteamDiscoveryOptions options = {});

	[[nodiscard]] SteamDiscovery discover () const;

	[[nodiscard]] const SteamDiscoveryOptions& options () const noexcept { return this->options_; }

	// Standard Steam roots for the current user, in resolution order.
	[[nodiscard]] static std::vector<std::filesystem::path> default_steam_roots (const SteamDiscoveryOptions& options);

	// Parses `steamapps/libraryfolders.vdf`, supporting both the modern
	// (object with a `path` key) and legacy (numeric key, string path) layouts.
	[[nodiscard]] static std::expected<std::vector<std::filesystem::path>, core::Error> parse_library_folders (
		const std::filesystem::path& vdf_path);

private:
	SteamDiscoveryOptions options_;
};

} // namespace we::steam
