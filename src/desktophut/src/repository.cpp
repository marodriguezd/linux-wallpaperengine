#include "we/desktophut/repository.hpp"

#include "we/core/log.hpp"
#include "we/scene/asset_container.hpp"
#include "we/scene/project.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <format>
#include <system_error>

namespace we::desktophut {
namespace {

    bool is_subpath_of (const std::filesystem::path& path, const std::filesystem::path& base) {
	if (base.empty () || path.empty ()) {
	    return false;
	}

	std::error_code ec;
	const auto norm_path = std::filesystem::weakly_canonical (path, ec);
	if (ec) {
	    return false;
	}

	const auto norm_base = std::filesystem::weakly_canonical (base, ec);
	if (ec) {
	    return false;
	}

	auto path_it = norm_path.begin ();
	auto base_it = norm_base.begin ();

	while (base_it != norm_base.end ()) {
	    if (path_it == norm_path.end () || *path_it != *base_it) {
		return false;
	    }
	    ++path_it;
	    ++base_it;
	}

	return true;
    }

    std::string lowercase (std::string_view text) {
	std::string result;
	result.reserve (text.size ());
	for (const char c : text) {
	    result.push_back (static_cast<char> (std::tolower (static_cast<unsigned char> (c))));
	}
	return result;
    }

} // namespace

std::filesystem::path local_wallpapers_dir () {
    const char* xdg_data = std::getenv ("XDG_DATA_HOME");
    if (xdg_data != nullptr && *xdg_data != '\0') {
	return std::filesystem::path (xdg_data) / "linux-wallpaperengine" / "wallpapers";
    }

    const char* home = std::getenv ("HOME");
    if (home != nullptr && *home != '\0') {
	return std::filesystem::path (home) / ".local" / "share" / "linux-wallpaperengine" / "wallpapers";
    }

    return std::filesystem::temp_directory_path () / "linux-wallpaperengine" / "wallpapers";
}

std::filesystem::path desktophut_wallpapers_dir () { return local_wallpapers_dir () / "desktophut"; }

std::vector<WallpaperItem> list_desktophut_wallpapers (const std::filesystem::path& dir) {
    std::vector<WallpaperItem> items;
    std::error_code ec;

    if (!std::filesystem::is_directory (dir, ec)) {
	return items;
    }

    for (const auto& entry : std::filesystem::directory_iterator (dir, ec)) {
	if (!entry.is_directory ()) {
	    continue;
	}

	auto assets = scene::AssetContainer::open (entry.path ());
	if (!assets) {
	    continue;
	}

	auto project = scene::load_project (*assets);
	if (!project) {
	    continue;
	}

	items.push_back (
	    WallpaperItem { .id = entry.path ().filename ().string (),
			    .title = project->title,
			    .type = std::string (scene::to_string (project->type)),
			    .file = project->file,
			    .path = entry.path ().string (),
			    .preview = project->preview,
			    .source = "desktophut" }
	);
    }

    std::ranges::sort (items, [] (const WallpaperItem& a, const WallpaperItem& b) {
	return lowercase (a.title) < lowercase (b.title);
    });

    return items;
}

std::vector<WallpaperItem> list_local_wallpapers (const std::filesystem::path& root) {
    std::vector<WallpaperItem> items;
    std::error_code ec;

    if (!std::filesystem::is_directory (root, ec)) {
	return items;
    }

    for (const auto& entry : std::filesystem::directory_iterator (root, ec)) {
	if (!entry.is_directory ()) {
	    continue;
	}

	// Check if entry itself is a wallpaper (has project.json or scene.pkg)
	auto assets = scene::AssetContainer::open (entry.path ());
	if (assets) {
	    auto project = scene::load_project (*assets);
	    if (project) {
		items.push_back (
		    WallpaperItem { .id = entry.path ().filename ().string (),
				    .title = project->title,
				    .type = std::string (scene::to_string (project->type)),
				    .file = project->file,
				    .path = entry.path ().string (),
				    .preview = project->preview,
				    .source = "local" }
		);
		continue;
	    }
	}

	// Otherwise, iterate children in this subdirectory (e.g. desktophut/<slug>)
	const std::string sub_source = entry.path ().filename ().string ();
	for (const auto& sub_entry : std::filesystem::directory_iterator (entry.path (), ec)) {
	    if (!sub_entry.is_directory ()) {
		continue;
	    }

	    auto sub_assets = scene::AssetContainer::open (sub_entry.path ());
	    if (!sub_assets) {
		continue;
	    }

	    auto sub_project = scene::load_project (*sub_assets);
	    if (!sub_project) {
		continue;
	    }

	    items.push_back (
		WallpaperItem { .id = sub_entry.path ().filename ().string (),
				.title = sub_project->title,
				.type = std::string (scene::to_string (sub_project->type)),
				.file = sub_project->file,
				.path = sub_entry.path ().string (),
				.preview = sub_project->preview,
				.source = sub_source }
	    );
	}
    }

    std::ranges::sort (items, [] (const WallpaperItem& a, const WallpaperItem& b) {
	return lowercase (a.title) < lowercase (b.title);
    });

    return items;
}

std::vector<WallpaperItem> list_all_wallpapers (const steam::SteamDiscovery& discovery) {
    std::vector<WallpaperItem> items = list_local_wallpapers ();

    for (const auto& library : discovery.libraries) {
	std::error_code ec;

	if (library.workshop_content_dir.empty ()
	    || !std::filesystem::is_directory (library.workshop_content_dir, ec)) {
	    continue;
	}

	for (const auto& entry : std::filesystem::directory_iterator (library.workshop_content_dir, ec)) {
	    if (!entry.is_directory ()) {
		continue;
	    }

	    auto assets = scene::AssetContainer::open (entry.path ());
	    if (!assets) {
		continue;
	    }

	    auto project = scene::load_project (*assets);
	    if (!project) {
		continue;
	    }

	    items.push_back (
		WallpaperItem { .id = entry.path ().filename ().string (),
				.title = project->title,
				.type = std::string (scene::to_string (project->type)),
				.file = project->file,
				.path = entry.path ().string (),
				.preview = project->preview,
				.source = "wallpaperengine" }
	    );
	}
    }

    std::ranges::sort (items, [] (const WallpaperItem& a, const WallpaperItem& b) {
	return lowercase (a.title) < lowercase (b.title);
    });

    return items;
}

bool is_steam_wallpaper (std::string_view target_or_path, const steam::SteamDiscovery& discovery) {
    if (target_or_path.empty ()) {
	return false;
    }

    const std::filesystem::path p (target_or_path);
    std::error_code ec;

    // Check if target is a numeric ID and exists in any workshop directory
    const bool is_numeric = !target_or_path.empty ()
	&& std::ranges::all_of (target_or_path, [] (char c) { return std::isdigit (static_cast<unsigned char> (c)); });

    if (is_numeric) {
	for (const auto& library : discovery.libraries) {
	    if (library.workshop_content_dir.empty ()) {
		continue;
	    }
	    const auto candidate = library.workshop_content_dir / std::string (target_or_path);
	    if (std::filesystem::exists (candidate, ec)) {
		return true;
	    }
	}
    }

    // Check against workshop and installation roots in discovery
    for (const auto& library : discovery.libraries) {
	if (!library.workshop_content_dir.empty () && is_subpath_of (p, library.workshop_content_dir)) {
	    return true;
	}
	if (!library.wallpaper_engine_dir.empty () && is_subpath_of (p, library.wallpaper_engine_dir)) {
	    return true;
	}
	if (!library.root.empty () && is_subpath_of (p, library.steamapps)) {
	    return true;
	}
    }

    for (const auto& inst : discovery.wallpaper_engine_installations) {
	if (is_subpath_of (p, inst)) {
	    return true;
	}
    }

    // Check path substrings for standard Steam / 431960 markers
    const std::string path_str = p.string ();
    if (path_str.contains ("431960") || path_str.contains ("workshop/content") || path_str.contains ("Wallpaper_Engine")
	|| path_str.contains ("wallpaper_engine")) {
	// Confirm it's not a local wallpaper test named 431960
	if (!is_subpath_of (p, local_wallpapers_dir ())) {
	    return true;
	}
    }

    return false;
}

std::optional<std::filesystem::path>
resolve_wallpaper (std::string_view target, const steam::SteamDiscovery& discovery) {
    if (target.empty ()) {
	return std::nullopt;
    }

    std::error_code ec;

    // 1. Direct path
    const std::filesystem::path direct_path (target);
    if (std::filesystem::exists (direct_path, ec)) {
	return direct_path;
    }

    // 2. If starts with desktophut-
    if (target.starts_with ("desktophut-")) {
	const std::string_view stripped = target.substr (11);
	const auto candidate = desktophut_wallpapers_dir () / std::string (stripped);
	if (std::filesystem::exists (candidate, ec)) {
	    return candidate;
	}
    }

    // 3. DesktopHut local directory
    const auto dh_candidate = desktophut_wallpapers_dir () / std::string (target);
    if (std::filesystem::exists (dh_candidate, ec)) {
	return dh_candidate;
    }

    // 4. Local wallpapers root
    const auto local_candidate = local_wallpapers_dir () / std::string (target);
    if (std::filesystem::exists (local_candidate, ec)) {
	return local_candidate;
    }

    // 5. Steam workshop content directory
    for (const auto& library : discovery.libraries) {
	if (library.workshop_content_dir.empty ()) {
	    continue;
	}

	const auto candidate = library.workshop_content_dir / std::string (target);
	if (std::filesystem::is_directory (candidate, ec) || std::filesystem::is_regular_file (candidate, ec)) {
	    return candidate;
	}
    }

    // 6. Search local items by ID / slug
    const auto local_items = list_local_wallpapers ();
    for (const auto& item : local_items) {
	if (item.id == target || ("desktophut-" + item.id) == target) {
	    return std::filesystem::path (item.path);
	}
    }

    return std::nullopt;
}

std::expected<void, core::Error> delete_wallpaper (std::string_view target, const steam::SteamDiscovery& discovery) {
    if (target.empty ()) {
	return core::fail (core::ErrorCode::invalid_argument, "wallpaper identifier cannot be empty");
    }

    // Safety check 1: check target directly against Steam
    if (is_steam_wallpaper (target, discovery)) {
	return core::fail (core::ErrorCode::unsupported, std::string (steam_deletion_refusal_message));
    }

    const auto resolved = resolve_wallpaper (target, discovery);
    if (!resolved) {
	return core::fail (core::ErrorCode::not_found, std::format ("cannot find wallpaper '{}'", target));
    }

    // Safety check 2: check resolved path against Steam
    if (is_steam_wallpaper (resolved->string (), discovery)) {
	return core::fail (core::ErrorCode::unsupported, std::string (steam_deletion_refusal_message));
    }

    // Safety check 3: safeguard against deleting arbitrary system directories
    const std::filesystem::path norm_path = std::filesystem::weakly_canonical (*resolved);
    const auto root_path = norm_path.root_path ();
    if (norm_path == root_path || norm_path == std::filesystem::path ("/home")
	|| norm_path == std::filesystem::path ("/usr") || norm_path == std::filesystem::path ("/etc")
	|| norm_path == std::filesystem::path ("/var")) {
	return core::fail (core::ErrorCode::invalid_argument, "refusing to delete protected system path");
    }

    // Ensure it contains project.json or scene.pkg or is under local wallpapers
    std::error_code ec;
    const bool is_local = is_subpath_of (norm_path, local_wallpapers_dir ());
    const bool has_project = std::filesystem::exists (norm_path / "project.json", ec)
	|| std::filesystem::exists (norm_path / "scene.pkg", ec);

    if (!is_local && !has_project) {
	return core::fail (
	    core::ErrorCode::invalid_argument,
	    std::format ("path '{}' is not a valid wallpaper directory", norm_path.string ())
	);
    }

    std::filesystem::remove_all (norm_path, ec);
    if (ec) {
	return core::fail (
	    core::ErrorCode::io, std::format ("failed to delete '{}': {}", norm_path.string (), ec.message ())
	);
    }

    return {};
}

std::expected<void, core::Error>
delete_desktophut_wallpaper (std::string_view id, const steam::SteamDiscovery& discovery) {
    if (id.empty ()) {
	return core::fail (core::ErrorCode::invalid_argument, "wallpaper ID cannot be empty");
    }

    // Protection check
    if (is_steam_wallpaper (id, discovery)) {
	return core::fail (core::ErrorCode::unsupported, std::string (steam_deletion_refusal_message));
    }

    std::string slug (id);
    if (slug.starts_with ("desktophut-")) {
	slug.erase (0, 11);
    }

    const auto candidate = desktophut_wallpapers_dir () / slug;
    std::error_code ec;
    if (std::filesystem::exists (candidate, ec)) {
	return delete_wallpaper (candidate.string (), discovery);
    }

    return delete_wallpaper (id, discovery);
}

} // namespace we::desktophut
