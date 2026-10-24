#include "we/steam/steam_locator.hpp"

#include <algorithm>
#include <cstdlib>
#include <initializer_list>
#include <ranges>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>

#include "we/core/text.hpp"
#include "we/steam/vdf.hpp"

namespace we::steam {
namespace {

constexpr std::string_view wallpaper_engine_app_id = "431960";
constexpr std::string_view wallpaper_engine_folder = "wallpaper_engine";
constexpr std::size_t maximum_libraries = 512;

[[nodiscard]] bool directory_exists (const std::filesystem::path& path) {
	std::error_code error_code;
	return std::filesystem::is_directory (path, error_code);
}

[[nodiscard]] std::filesystem::path normalize_candidate (std::filesystem::path path) {
	if (path.empty ()) {
		return path;
	}

	if (path.is_relative ()) {
		std::error_code error_code;

		if (const auto absolute = std::filesystem::absolute (path, error_code); !error_code) {
			path = absolute;
		}
	}

	std::string text = path.lexically_normal ().string ();

	while (text.size () > 1 && text.back () == '/') {
		text.pop_back ();
	}

	return text;
}

[[nodiscard]] std::string canonical_key (const std::filesystem::path& path) {
	std::error_code error_code;

	if (const auto canonical = std::filesystem::weakly_canonical (path, error_code); !error_code) {
		return canonical.string ();
	}

	return normalize_candidate (path).string ();
}

[[nodiscard]] std::optional<std::filesystem::path> find_child_ci (
	const std::filesystem::path& parent, std::string_view wanted) {
	std::error_code error_code;

	std::filesystem::directory_iterator iterator (
		parent, std::filesystem::directory_options::skip_permission_denied, error_code);

	if (error_code) {
		return std::nullopt;
	}

	const std::filesystem::directory_iterator end;

	while (iterator != end) {
		const std::string name = iterator->path ().filename ().string ();

		if (core::iequals (name, wanted)) {
			return iterator->path ();
		}

		iterator.increment (error_code);

		if (error_code) {
			break;
		}
	}

	return std::nullopt;
}

[[nodiscard]] std::optional<std::filesystem::path> resolve_ci (
	const std::filesystem::path& base, std::initializer_list<std::string_view> components) {
	std::filesystem::path current = base;

	for (const std::string_view component : components) {
		const auto child = find_child_ci (current, component);

		if (!child) {
			return std::nullopt;
		}

		current = *child;
	}

	return current;
}

[[nodiscard]] std::filesystem::path resolve_home (const SteamDiscoveryOptions& options) {
	if (options.home.has_value ()) {
		return normalize_candidate (*options.home);
	}

	if (const char* home = std::getenv ("HOME"); home != nullptr && *home != '\0') {
		return normalize_candidate (home);
	}

	std::error_code error_code;

	if (const auto current = std::filesystem::current_path (error_code); !error_code) {
		return current;
	}

	return {};
}

[[nodiscard]] std::filesystem::path resolve_xdg_data (
	const SteamDiscoveryOptions& options, const std::filesystem::path& home) {
	if (options.xdg_data_home.has_value ()) {
		return normalize_candidate (*options.xdg_data_home);
	}

	// Do not read the environment when the caller pinned a home directory for
	// reproducible discovery (tests, packaging checks).
	if (!options.home.has_value ()) {
		if (const char* data_home = std::getenv ("XDG_DATA_HOME"); data_home != nullptr && *data_home != '\0') {
			return normalize_candidate (data_home);
		}
	}

	if (home.empty ()) {
		return {};
	}

	return home / ".local" / "share";
}

[[nodiscard]] std::vector<std::filesystem::path> default_mount_roots (const std::filesystem::path& home) {
	std::vector<std::filesystem::path> roots {"/mnt", "/media"};

	std::string user;

	if (const char* env_user = std::getenv ("USER"); env_user != nullptr && *env_user != '\0') {
		user = env_user;
	} else if (!home.empty ()) {
		user = home.filename ().string ();
	}

	if (!user.empty ()) {
		roots.emplace_back (std::filesystem::path ("/run/media") / user);
	}

	return roots;
}

[[nodiscard]] SteamInstallKind classify_root (const std::filesystem::path& root) {
	const std::string text = root.string ();

	if (text.find ("/.var/app/com.valvesoftware.Steam/") != std::string::npos) {
		return SteamInstallKind::flatpak;
	}

	if (text.find ("/snap/steam/") != std::string::npos) {
		return SteamInstallKind::snap;
	}

	return SteamInstallKind::native;
}

[[nodiscard]] bool should_skip_mount_directory (std::string_view name) {
	if (name.empty () || name.front () == '.') {
		return true;
	}

	static constexpr std::string_view ignored[] = {
		"steamapps",
		"windows",
		"users",
		"programdata",
		"system volume information",
		"$recycle.bin",
		"recovery",
		"lost+found",
		"config.msi",
	};

	for (const std::string_view candidate : ignored) {
		if (core::iequals (name, candidate)) {
			return true;
		}
	}

	return false;
}

class Discovery {
public:
	explicit Discovery (const SteamDiscoveryOptions& options) : options_ (options) { }

	[[nodiscard]] SteamDiscovery run () {
		this->home_ = resolve_home (this->options_);

		for (const auto& root : SteamLocator::default_steam_roots (this->options_)) {
			this->add_library (root, classify_root (root));
		}

		for (const auto& extra : this->options_.extra_library_roots) {
			this->add_library (extra, SteamInstallKind::custom);
		}

		this->drain_vdf_queue ();

		if (this->options_.scan_mounts) {
			this->scan_mounts ();
			this->drain_vdf_queue ();
		}

		if (this->options_.scan_proton_prefixes) {
			this->scan_proton_prefixes ();
			this->drain_vdf_queue ();
		}

		return std::move (this->result_);
	}

private:
	void add_library (std::filesystem::path candidate, SteamInstallKind kind) {
		candidate = normalize_candidate (std::move (candidate));

		if (candidate.empty ()) {
			return;
		}

		if (candidate.filename () == "steamapps") {
			candidate = candidate.parent_path ();
		}

		const std::filesystem::path steamapps = candidate / "steamapps";

		if (!directory_exists (steamapps)) {
			return;
		}

		const std::string key = canonical_key (candidate);

		if (this->library_by_key_.contains (key)) {
			return;
		}

		if (this->result_.libraries.size () >= maximum_libraries) {
			if (std::ranges::find (this->result_.warnings, "too many Steam libraries, stopping discovery")
				== this->result_.warnings.end ()) {
				this->result_.warnings.emplace_back ("too many Steam libraries, stopping discovery");
			}

			return;
		}

		SteamLibrary library {
			.root = candidate,
			.steamapps = steamapps,
			.kind = kind,
			.has_wallpaper_engine = false,
			.has_app_manifest = false,
			.wallpaper_engine_dir = {},
			.workshop_content_dir = {},
		};

		if (const auto common = steamapps / "common"; directory_exists (common)) {
			if (const auto wallpaper_engine = find_child_ci (common, wallpaper_engine_folder);
				wallpaper_engine.has_value () && directory_exists (*wallpaper_engine)) {
				library.has_wallpaper_engine = true;
				library.wallpaper_engine_dir = *wallpaper_engine;
				this->result_.wallpaper_engine_installations.push_back (*wallpaper_engine);
			}
		}

		std::error_code error_code;
		const std::filesystem::path manifest
			= steamapps / ("appmanifest_" + std::string (wallpaper_engine_app_id) + ".acf");
		library.has_app_manifest = std::filesystem::exists (manifest, error_code);

		const std::filesystem::path workshop
			= steamapps / "workshop" / "content" / std::string (wallpaper_engine_app_id);

		if (directory_exists (workshop)) {
			library.workshop_content_dir = workshop;
		}

		this->library_by_key_.emplace (key, this->result_.libraries.size ());
		this->pending_vdf_.emplace_back (steamapps / "libraryfolders.vdf", kind);
		this->result_.libraries.push_back (std::move (library));
	}

	void drain_vdf_queue () {
		for (std::size_t index = 0; index < this->pending_vdf_.size (); ++index) {
			const auto& [vdf_path, kind] = this->pending_vdf_[index];

			std::error_code error_code;

			if (!std::filesystem::is_regular_file (vdf_path, error_code)) {
				continue;
			}

			auto libraries = SteamLocator::parse_library_folders (vdf_path);

			if (!libraries) {
				this->result_.warnings.push_back (
					"cannot read '" + vdf_path.string () + "': " + libraries.error ().message);
				continue;
			}

			for (const auto& library : *libraries) {
				this->add_library (library, kind);
			}
		}
	}

	void scan_mounts () {
		const std::vector<std::filesystem::path> roots = this->options_.mount_roots.empty ()
			? default_mount_roots (this->home_)
			: this->options_.mount_roots;

		this->visits_ = 0;

		for (const auto& mount : roots) {
			if (!directory_exists (mount)) {
				continue;
			}

			// The mount point itself may be the library root.
			if (const auto steamapps = find_child_ci (mount, "steamapps");
				steamapps.has_value () && directory_exists (*steamapps)) {
				this->add_library (mount, SteamInstallKind::windows_mount);
				continue;
			}

			std::error_code error_code;

			std::filesystem::directory_iterator iterator (
				mount, std::filesystem::directory_options::skip_permission_denied, error_code);

			if (error_code) {
				continue;
			}

			const std::filesystem::directory_iterator end;

			while (iterator != end) {
				if (this->visits_ >= this->options_.max_directory_visits) {
					this->warn_visit_limit ();
					return;
				}

				std::error_code entry_error;

				if (iterator->is_directory (entry_error) && !iterator->is_symlink (entry_error)) {
					this->scan_mount_directory (iterator->path (), 0);
				}

				iterator.increment (error_code);

				if (error_code) {
					break;
				}
			}
		}
	}

	void scan_mount_directory (const std::filesystem::path& directory, std::uint32_t depth) {
		if (this->visits_ >= this->options_.max_directory_visits) {
			this->warn_visit_limit ();
			return;
		}

		++this->visits_;

		// A library root is any directory that owns a `steamapps` directory.
		if (const auto steamapps = find_child_ci (directory, "steamapps");
			steamapps.has_value () && directory_exists (*steamapps)) {
			this->add_library (directory, SteamInstallKind::windows_mount);
			return;
		}

		if (depth >= this->options_.max_mount_depth) {
			return;
		}

		std::error_code error_code;

		std::filesystem::directory_iterator iterator (
			directory, std::filesystem::directory_options::skip_permission_denied, error_code);

		if (error_code) {
			return;
		}

		const std::filesystem::directory_iterator end;

		while (iterator != end) {
			if (this->visits_ >= this->options_.max_directory_visits) {
				this->warn_visit_limit ();
				return;
			}

			std::error_code entry_error;

			if (iterator->is_directory (entry_error) && !iterator->is_symlink (entry_error)
				&& !should_skip_mount_directory (iterator->path ().filename ().string ())) {
				this->scan_mount_directory (iterator->path (), depth + 1);
			}

			iterator.increment (error_code);

			if (error_code) {
				break;
			}
		}
	}

	void scan_proton_prefixes () {
		// Copy because add_library() appends to the library list while scanning.
		const std::vector<SteamLibrary> libraries = this->result_.libraries;
		constexpr std::uint32_t maximum_prefixes = 128;

		for (const auto& library : libraries) {
			const std::filesystem::path compatdata = library.steamapps / "compatdata";

			if (!directory_exists (compatdata)) {
				continue;
			}

			std::error_code error_code;

			std::filesystem::directory_iterator iterator (
				compatdata, std::filesystem::directory_options::skip_permission_denied, error_code);

			if (error_code) {
				continue;
			}

			const std::filesystem::directory_iterator end;
			std::uint32_t prefixes = 0;

			while (iterator != end && prefixes < maximum_prefixes) {
				std::error_code entry_error;

				if (!iterator->is_directory (entry_error) || iterator->is_symlink (entry_error)) {
					iterator.increment (error_code);

					if (error_code) {
						break;
					}

					continue;
				}

				++prefixes;

				const std::filesystem::path drive_c = iterator->path () / "pfx" / "drive_c";

				if (directory_exists (drive_c)) {
					for (const auto& steam_directory :
						 { resolve_ci (drive_c, {"Program Files (x86)", "Steam"}),
						   resolve_ci (drive_c, {"Program Files", "Steam"}),
						   resolve_ci (drive_c, {"Steam"}),
						   resolve_ci (drive_c, {"SteamLibrary"}) }) {
						if (steam_directory.has_value ()) {
							this->add_library (*steam_directory, SteamInstallKind::proton_prefix);
						}
					}
				}

				iterator.increment (error_code);

				if (error_code) {
					break;
				}
			}
		}
	}

	void warn_visit_limit () {
		const std::string message
			= "mount scan stopped after " + std::to_string (this->options_.max_directory_visits) + " directories";

		if (std::ranges::find (this->result_.warnings, message) == this->result_.warnings.end ()) {
			this->result_.warnings.push_back (message);
		}
	}

	SteamDiscoveryOptions options_;
	std::filesystem::path home_;
	SteamDiscovery result_;
	std::unordered_map<std::string, std::size_t> library_by_key_;
	std::vector<std::pair<std::filesystem::path, SteamInstallKind>> pending_vdf_;
	std::uint32_t visits_ = 0;
};

} // namespace

std::string_view to_string (SteamInstallKind kind) noexcept {
	switch (kind) {
		case SteamInstallKind::native:
			return "native";
		case SteamInstallKind::flatpak:
			return "flatpak";
		case SteamInstallKind::snap:
			return "snap";
		case SteamInstallKind::proton_prefix:
			return "proton_prefix";
		case SteamInstallKind::windows_mount:
			return "windows_mount";
		case SteamInstallKind::custom:
			return "custom";
	}

	return "unknown";
}

SteamLocator::SteamLocator (SteamDiscoveryOptions options) : options_ (std::move (options)) { }

SteamDiscovery SteamLocator::discover () const { return Discovery (this->options_).run (); }

std::vector<std::filesystem::path> SteamLocator::default_steam_roots (const SteamDiscoveryOptions& options) {
	const std::filesystem::path home = resolve_home (options);
	const std::filesystem::path data_home = resolve_xdg_data (options, home);

	std::vector<std::filesystem::path> roots;

	const auto push = [&roots] (const std::filesystem::path& path) {
		if (!path.empty ()) {
			roots.push_back (path);
		}
	};

	if (!options.home.has_value ()) {
		if (const char* steam_root = std::getenv ("STEAM_ROOT"); steam_root != nullptr && *steam_root != '\0') {
			push (normalize_candidate (steam_root));
		}
	}

	if (!data_home.empty ()) {
		push (data_home / "Steam");
	}

	if (!home.empty ()) {
		push (home / ".local" / "share" / "Steam");
		push (home / ".steam" / "steam");
		push (home / ".steam" / "root");
		push (home / ".var" / "app" / "com.valvesoftware.Steam" / ".local" / "share" / "Steam");
		push (home / "snap" / "steam" / "common" / ".local" / "share" / "Steam");
		push (home / "snap" / "steam" / "common" / ".steam" / "steam");
		push (home / "Steam");
	}

	return roots;
}

std::expected<std::vector<std::filesystem::path>, core::Error> SteamLocator::parse_library_folders (
	const std::filesystem::path& vdf_path) {
	auto document = parse_vdf_file (vdf_path);

	if (!document) {
		return std::unexpected (std::move (document.error ()));
	}

	if (!document->is_object ()) {
		return core::fail (
			core::ErrorCode::malformed_data, "'" + vdf_path.string () + "' does not contain a KeyValues object");
	}

	const VdfValue* folder_list = document->get ("libraryfolders");
	const VdfObject* entries = nullptr;

	if (folder_list != nullptr) {
		if (!folder_list->is_object ()) {
			return core::fail (
				core::ErrorCode::malformed_data, "'" + vdf_path.string () + "' has an invalid libraryfolders entry");
		}

		entries = &folder_list->as_object ();
	} else {
		entries = &document->as_object ();
	}

	std::vector<std::filesystem::path> result;
	std::vector<std::string> seen;

	for (const auto& [key, value] : entries->entries ()) {
		std::string candidate;

		if (value.is_string ()) {
			candidate = std::string (value.as_string ());
		} else if (value.is_object ()) {
			if (const VdfValue* path = value.get ("path"); path != nullptr && path->is_string ()) {
				candidate = std::string (path->as_string ());
			}
		}

		if (candidate.empty ()) {
			continue;
		}

		// Linux-only paths can be validated here. Windows drive letters refer
		// to another machine's namespace and are discovered through mounts.
		if (candidate.front () != '/') {
			continue;
		}

		const std::string normalized = normalize_candidate (std::filesystem::path (candidate)).string ();

		if (std::ranges::find (seen, normalized) != seen.end ()) {
			continue;
		}

		seen.push_back (normalized);
		result.emplace_back (normalized);
	}

	return result;
}

} // namespace we::steam
