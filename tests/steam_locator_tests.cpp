#include <algorithm>
#include <filesystem>
#include <initializer_list>
#include <string>

#include "support/harness.hpp"
#include "support/temp_dir.hpp"
#include "we/steam/steam_locator.hpp"

using we::steam::SteamDiscovery;
using we::steam::SteamDiscoveryOptions;
using we::steam::SteamInstallKind;
using we::steam::SteamLibrary;
using we::steam::SteamLocator;

namespace {

[[nodiscard]] const SteamLibrary* find_library (const SteamDiscovery& discovery, const std::filesystem::path& root) {
	const auto found = std::ranges::find_if (discovery.libraries, [&root] (const SteamLibrary& library) {
		return library.root == root;
	});

	return found == discovery.libraries.end () ? nullptr : &*found;
}

[[nodiscard]] std::string libraryfolders (std::initializer_list<std::filesystem::path> paths) {
	std::string result = "\"libraryfolders\"\n{\n";
	std::size_t index = 0;

	for (const auto& path : paths) {
		result += "    \"" + std::to_string (index++) + "\" { \"path\" \"" + path.string () + "\" }\n";
	}

	result += "}\n";
	return result;
}

} // namespace

TEST (steam_discovers_native_library) {
	we::test::TempDir temp;
	const auto home = temp.path () / "home";
	const auto steam = home / ".local" / "share" / "Steam";
	const auto steamapps = steam / "steamapps";

	we::test::ensure_directory (steamapps / "common" / "wallpaper_engine" / "projects");
	we::test::ensure_directory (steamapps / "workshop" / "content" / "431960" / "123");
	we::test::write_text (steamapps / "appmanifest_431960.acf", R"("AppState" { "appid" "431960" })");
	we::test::write_text (steamapps / "libraryfolders.vdf", libraryfolders ({steam, steam}));

	SteamDiscoveryOptions options;
	options.home = home;
	options.scan_mounts = false;
	options.scan_proton_prefixes = false;

	const SteamDiscovery discovery = SteamLocator (options).discover ();

	CHECK_EQ (discovery.libraries.size (), std::size_t {1});

	const SteamLibrary* library = find_library (discovery, steam);

	CHECK (library != nullptr);
	CHECK_EQ (library->kind, SteamInstallKind::native);
	CHECK (library->has_wallpaper_engine);
	CHECK (library->has_app_manifest);
	CHECK_EQ (library->wallpaper_engine_dir.string (), (steamapps / "common" / "wallpaper_engine").string ());
	CHECK_EQ (library->workshop_content_dir.string (), (steamapps / "workshop" / "content" / "431960").string ());
	CHECK_EQ (discovery.wallpaper_engine_installations.size (), std::size_t {1});
}

TEST (steam_discovers_libraries_recursively_from_vdf) {
	we::test::TempDir temp;
	const auto home = temp.path () / "home";
	const auto steam = home / ".local" / "share" / "Steam";
	const auto second = temp.path () / "library2";
	const auto third = temp.path () / "library3";

	we::test::ensure_directory (steam / "steamapps");
	we::test::ensure_directory (second / "steamapps" / "common" / "wallpaper_engine");
	we::test::ensure_directory (third / "steamapps" / "common" / "wallpaper_engine");

	we::test::write_text (steam / "steamapps" / "libraryfolders.vdf", libraryfolders ({steam, second}));
	we::test::write_text (second / "steamapps" / "libraryfolders.vdf", libraryfolders ({second, third}));

	SteamDiscoveryOptions options;
	options.home = home;
	options.scan_mounts = false;
	options.scan_proton_prefixes = false;

	const SteamDiscovery discovery = SteamLocator (options).discover ();

	CHECK_EQ (discovery.libraries.size (), std::size_t {3});

	const SteamLibrary* library = find_library (discovery, second);
	const SteamLibrary* recursive = find_library (discovery, third);

	CHECK (library != nullptr);
	CHECK_EQ (library->kind, SteamInstallKind::native);
	CHECK (library->has_wallpaper_engine);

	CHECK (recursive != nullptr);
	CHECK (recursive->has_wallpaper_engine);
	CHECK_EQ (discovery.wallpaper_engine_installations.size (), std::size_t {2});
}

TEST (steam_discovers_windows_mounts) {
	we::test::TempDir temp;
	const auto home = temp.path () / "home";
	const auto mount = temp.path () / "mnt";
	const auto windows_steam = mount / "GameDrive" / "SteamLibrary";
	const auto program_files_steam = mount / "WindowsDrive" / "Program Files (x86)" / "Steam";

	we::test::ensure_directory (home / ".local" / "share" / "Steam" / "steamapps");
	we::test::ensure_directory (windows_steam / "steamapps" / "common" / "wallpaper_engine");
	we::test::ensure_directory (program_files_steam / "steamapps" / "common" / "wallpaper_engine");

	SteamDiscoveryOptions options;
	options.home = home;
	options.mount_roots = {mount};
	options.scan_proton_prefixes = false;

	const SteamDiscovery discovery = SteamLocator (options).discover ();

	const SteamLibrary* mounted = find_library (discovery, windows_steam);
	const SteamLibrary* windows = find_library (discovery, program_files_steam);

	CHECK (mounted != nullptr);
	CHECK_EQ (mounted->kind, SteamInstallKind::windows_mount);
	CHECK (mounted->has_wallpaper_engine);

	CHECK (windows != nullptr);
	CHECK_EQ (windows->kind, SteamInstallKind::windows_mount);
	CHECK (windows->has_wallpaper_engine);
}

TEST (steam_discovers_proton_prefix_installations) {
	we::test::TempDir temp;
	const auto home = temp.path () / "home";
	const auto steam = home / ".local" / "share" / "Steam";
	const auto proton_steam = steam / "steamapps" / "compatdata" / "999" / "pfx" / "drive_c" / "Program Files (x86)"
		/ "Steam";

	we::test::ensure_directory (steam / "steamapps");
	we::test::ensure_directory (proton_steam / "steamapps" / "common" / "wallpaper_engine");

	SteamDiscoveryOptions options;
	options.home = home;
	options.scan_mounts = false;

	const SteamDiscovery discovery = SteamLocator (options).discover ();

	const SteamLibrary* prefix = find_library (discovery, proton_steam);

	CHECK (prefix != nullptr);
	CHECK_EQ (prefix->kind, SteamInstallKind::proton_prefix);
	CHECK (prefix->has_wallpaper_engine);
}

TEST (steam_discovers_flatpak_library) {
	we::test::TempDir temp;
	const auto home = temp.path () / "home";
	const auto flatpak = home / ".var" / "app" / "com.valvesoftware.Steam" / ".local" / "share" / "Steam";

	we::test::ensure_directory (flatpak / "steamapps" / "common" / "wallpaper_engine");

	SteamDiscoveryOptions options;
	options.home = home;
	options.scan_mounts = false;
	options.scan_proton_prefixes = false;

	const SteamDiscovery discovery = SteamLocator (options).discover ();
	const SteamLibrary* library = find_library (discovery, flatpak);

	CHECK (library != nullptr);
	CHECK_EQ (library->kind, SteamInstallKind::flatpak);
	CHECK (library->has_wallpaper_engine);
}

TEST (steam_default_roots_include_flatpak_and_snap) {
	we::test::TempDir temp;
	const auto home = temp.path () / "home";

	SteamDiscoveryOptions options;
	options.home = home;

	const auto roots = SteamLocator::default_steam_roots (options);

	const auto flatpak = home / ".var" / "app" / "com.valvesoftware.Steam" / ".local" / "share" / "Steam";
	const auto snap = home / "snap" / "steam" / "common" / ".local" / "share" / "Steam";

	CHECK (std::ranges::find (roots, flatpak) != roots.end ());
	CHECK (std::ranges::find (roots, snap) != roots.end ());
}
