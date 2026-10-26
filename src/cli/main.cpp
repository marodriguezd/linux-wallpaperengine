#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <print>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "we/app/engine.hpp"
#include "we/display/display.hpp"
#include "we/power/battery.hpp"
#include "we/presets/presets.hpp"
#include "we/scene/asset_container.hpp"
#include "we/scene/project.hpp"
#include "we/steam/package_archive.hpp"
#include "we/steam/steam_locator.hpp"
#include "we/steam/vfs.hpp"

namespace {

namespace app = we::app;
namespace presets = we::presets;
namespace display = we::display;
namespace gfx = we::gfx;
namespace power = we::power;
namespace scene = we::scene;

using we::steam::CaseInsensitiveVfs;
using we::steam::PackageArchive;
using we::steam::SteamDiscovery;
using we::steam::SteamDiscoveryOptions;
using we::steam::SteamLocator;
using we::steam::VfsEntry;

constexpr std::string_view program_name = "we";

void print_usage () {
    std::print (
	stderr,
	"{0} {1} - Wallpaper Engine engine tooling\n"
	"\n"
	"Usage:\n"
	"  {0} run PATH|WORKSHOP_ID [options]\n"
	"  {0} list [--json]\n"
	"  {0} steam [--json] [--home DIR] [--library DIR]... [--no-mounts] [--no-proton]\n"
	"  {0} doctor\n"
	"  {0} vfs --root DIR [--root DIR]... [--list] [--cat] PATH\n"
	"  {0} pkg ARCHIVE [--list] [--cat ENTRY | --extract ENTRY --out FILE]\n"
	"  {0} --version\n"
	"\n"
	"Commands:\n"
	"  run     Play a wallpaper directory, scene.pkg or Steam workshop id.\n"
	"  list    List Wallpaper Engine projects found in the Steam workshop.\n"
	"  steam   Discover Steam libraries and Wallpaper Engine installations.\n"
	"  doctor  Report display, OpenGL, battery and Steam status.\n"
	"  vfs     Resolve a Wallpaper Engine path against one or more roots.\n"
	"  pkg     Inspect or extract a scene.pkg archive without writing it to disk.\n"
	"\n"
	"run options:\n"
	"  --backend auto|x11-desktop|x11-root|wayland|headless\n"
	"  --output NAME          Render on a single monitor.\n"
	"  --fps N                Frames per second on AC (default 60).\n"
	"  --preset NAME         Apply a saved or built-in preset (battery, balanced, smooth).\n"
	"  --battery-fps N        Frames per second on battery (default 15, 0 pauses).\n"
	"  --pause-on-battery     Freeze rendering when unplugged.\n"
	"  --no-pause-fullscreen  Keep rendering behind fullscreen windows.\n"
	"  --no-vsync             Disable vertical sync.\n"
	"  --audio                Enable video audio.\n"
	"  --volume N             Video volume, 0..100 (default 60).\n"
	"  --headless             Offscreen rendering (useful with --frames).\n"
	"  --width N --height N   Headless resolution (default 1920x1080).\n"
	"  --frames N             Stop after N rendered frames.\n"
	"  --screenshot FILE      Save the final frame as PNG.\n"
	"  --verbose              Debug logging.\n",
	program_name, WE_VERSION
    );
}

void print_error (std::string_view message) { std::print (stderr, "{}: {}\n", program_name, message); }

[[nodiscard]] std::string json_escape (std::string_view text) {
    std::string result;
    result.reserve (text.size ());

    for (const char value : text) {
	switch (value) {
	    case '"':
		result += "\\\"";
		break;
	    case '\\':
		result += "\\\\";
		break;
	    case '\n':
		result += "\\n";
		break;
	    case '\r':
		result += "\\r";
		break;
	    case '\t':
		result += "\\t";
		break;
	    default:
		if (static_cast<unsigned char> (value) < 0x20) {
		    result += std::format ("\\u{:04x}", static_cast<unsigned char> (value));
		} else {
		    result.push_back (value);
		}
		break;
	}
    }

    return result;
}

void add_json_string (std::string& output, std::string_view name, std::string_view value, bool last = false) {
    output += "      \"";
    output += name;
    output += "\": \"";
    output += json_escape (value);
    output += last ? "\"\n" : "\",\n";
}

void print_steam_json (const SteamDiscovery& discovery) {
    std::string output = "{\n  \"libraries\": [\n";

    for (std::size_t index = 0; index < discovery.libraries.size (); ++index) {
	const auto& library = discovery.libraries[index];
	output += "    {\n";
	add_json_string (output, "root", library.root.string ());
	add_json_string (output, "steamapps", library.steamapps.string ());
	add_json_string (output, "kind", we::steam::to_string (library.kind));
	add_json_string (output, "wallpaper_engine", library.wallpaper_engine_dir.string ());
	add_json_string (output, "workshop", library.workshop_content_dir.string ());
	output += std::format ("      \"has_app_manifest\": {}\n", library.has_app_manifest ? "true" : "false");
	output += index + 1 == discovery.libraries.size () ? "    }\n" : "    },\n";
    }

    output += "  ],\n  \"wallpaper_engine_installations\": [\n";

    for (std::size_t index = 0; index < discovery.wallpaper_engine_installations.size (); ++index) {
	output += "    \"" + json_escape (discovery.wallpaper_engine_installations[index].string ()) + "\"";
	output += index + 1 == discovery.wallpaper_engine_installations.size () ? "\n" : ",\n";
    }

    output += "  ],\n  \"warnings\": [\n";

    for (std::size_t index = 0; index < discovery.warnings.size (); ++index) {
	output += "    \"" + json_escape (discovery.warnings[index]) + "\"";
	output += index + 1 == discovery.warnings.size () ? "\n" : ",\n";
    }

    output += "  ]\n}\n";
    std::print ("{}", output);
}

void print_steam_human (const SteamDiscovery& discovery) {
    std::print ("Steam libraries ({}):\n", discovery.libraries.size ());

    for (const auto& library : discovery.libraries) {
	std::print ("  {} [{}]\n", library.root.string (), we::steam::to_string (library.kind));

	if (library.has_wallpaper_engine) {
	    std::print ("    wallpaper engine: {}\n", library.wallpaper_engine_dir.string ());
	}

	if (!library.workshop_content_dir.empty ()) {
	    std::print ("    workshop: {}\n", library.workshop_content_dir.string ());
	}

	if (library.has_app_manifest) {
	    std::print ("    app manifest 431960: yes\n");
	}
    }

    std::print ("\nWallpaper Engine installations ({}):\n", discovery.wallpaper_engine_installations.size ());

    for (const auto& installation : discovery.wallpaper_engine_installations) {
	std::print ("  {}\n", installation.string ());
    }
}

int command_steam (std::span<const std::string_view> arguments) {
    SteamDiscoveryOptions options;
    bool json = false;

    for (std::size_t index = 0; index < arguments.size (); ++index) {
	const std::string_view argument = arguments[index];

	const auto value = [&arguments, &index] (std::string_view name) -> std::optional<std::string_view> {
	    if (index + 1 >= arguments.size ()) {
		print_error (std::format ("{} requires a value", name));
		return std::nullopt;
	    }

	    return arguments[++index];
	};

	if (argument == "--json") {
	    json = true;
	} else if (argument == "--home") {
	    const auto home = value (argument);

	    if (!home) {
		return 2;
	    }

	    options.home = std::filesystem::path (*home);
	} else if (argument == "--library") {
	    const auto library = value (argument);

	    if (!library) {
		return 2;
	    }

	    options.extra_library_roots.emplace_back (*library);
	} else if (argument == "--no-mounts") {
	    options.scan_mounts = false;
	} else if (argument == "--no-proton") {
	    options.scan_proton_prefixes = false;
	} else if (argument == "--help" || argument == "-h") {
	    print_usage ();
	    return 0;
	} else {
	    print_error (std::format ("unknown option '{}'", argument));
	    return 2;
	}
    }

    const SteamLocator locator (std::move (options));
    const SteamDiscovery discovery = locator.discover ();

    if (json) {
	print_steam_json (discovery);
    } else {
	print_steam_human (discovery);
    }

    for (const auto& warning : discovery.warnings) {
	print_error (warning);
    }

    return 0;
}

int command_vfs (std::span<const std::string_view> arguments) {
    std::vector<std::filesystem::path> roots;
    bool list = false;
    bool cat = false;
    std::optional<std::string_view> path;

    for (std::size_t index = 0; index < arguments.size (); ++index) {
	const std::string_view argument = arguments[index];

	if (argument == "--root") {
	    if (index + 1 >= arguments.size ()) {
		print_error ("--root requires a value");
		return 2;
	    }

	    roots.emplace_back (arguments[++index]);
	} else if (argument == "--list") {
	    list = true;
	} else if (argument == "--cat") {
	    cat = true;
	} else if (argument == "--help" || argument == "-h") {
	    print_usage ();
	    return 0;
	} else if (argument.starts_with ("--")) {
	    print_error (std::format ("unknown option '{}'", argument));
	    return 2;
	} else if (path.has_value ()) {
	    print_error ("only one path may be resolved at a time");
	    return 2;
	} else {
	    path = argument;
	}
    }

    if (!path.has_value ()) {
	print_error ("missing PATH");
	return 2;
    }

    if (roots.empty ()) {
	print_error ("at least one --root is required");
	return 2;
    }

    if (list && cat) {
	print_error ("--list and --cat cannot be combined");
	return 2;
    }

    const CaseInsensitiveVfs vfs (std::move (roots));

    if (list) {
	const auto entries = vfs.list (*path);

	if (!entries) {
	    print_error (entries.error ().message);
	    return 1;
	}

	for (const VfsEntry& entry : *entries) {
	    std::print ("{} {:>10} {}\n", entry.directory ? 'd' : 'f', entry.size, entry.name);
	}

	return 0;
    }

    if (cat) {
	const auto text = vfs.read_text (*path);

	if (!text) {
	    print_error (text.error ().message);
	    return 1;
	}

	std::print ("{}", *text);
	return 0;
    }

    const auto resolved = vfs.resolve (*path);

    if (!resolved) {
	print_error (resolved.error ().message);
	return 1;
    }

    std::print ("{}\n", resolved->string ());
    return 0;
}

int command_pkg (std::span<const std::string_view> arguments) {
    std::optional<std::filesystem::path> archive_path;
    std::optional<std::string_view> cat_entry;
    std::optional<std::string_view> extract_entry;
    std::optional<std::filesystem::path> output_path;

    for (std::size_t index = 0; index < arguments.size (); ++index) {
	const std::string_view argument = arguments[index];

	const auto value = [&arguments, &index] (std::string_view name) -> std::optional<std::string_view> {
	    if (index + 1 >= arguments.size ()) {
		print_error (std::format ("{} requires a value", name));
		return std::nullopt;
	    }

	    return arguments[++index];
	};

	if (argument == "--cat") {
	    const auto entry = value (argument);

	    if (!entry) {
		return 2;
	    }

	    cat_entry = *entry;
	} else if (argument == "--extract") {
	    const auto entry = value (argument);

	    if (!entry) {
		return 2;
	    }

	    extract_entry = *entry;
	} else if (argument == "--out") {
	    const auto path = value (argument);

	    if (!path) {
		return 2;
	    }

	    output_path = std::filesystem::path (*path);
	} else if (argument == "--list") {
	    // Listing is the default.
	} else if (argument == "--help" || argument == "-h") {
	    print_usage ();
	    return 0;
	} else if (argument.starts_with ("--")) {
	    print_error (std::format ("unknown option '{}'", argument));
	    return 2;
	} else if (archive_path.has_value ()) {
	    print_error ("only one archive may be opened at a time");
	    return 2;
	} else {
	    archive_path = std::filesystem::path (argument);
	}
    }

    if (!archive_path.has_value ()) {
	print_error ("missing ARCHIVE");
	return 2;
    }

    if (cat_entry.has_value () && extract_entry.has_value ()) {
	print_error ("--cat and --extract cannot be combined");
	return 2;
    }

    if (extract_entry.has_value () && !output_path.has_value ()) {
	print_error ("--extract requires --out");
	return 2;
    }

    auto archive = PackageArchive::open (*archive_path);

    if (!archive) {
	print_error (archive.error ().message);
	return 1;
    }

    if (cat_entry.has_value ()) {
	const auto data = archive->view (*cat_entry);

	if (!data) {
	    print_error (data.error ().message);
	    return 1;
	}

	std::fwrite (data->data (), 1, data->size (), stdout);
	return 0;
    }

    if (extract_entry.has_value ()) {
	const auto data = archive->view (*extract_entry);

	if (!data) {
	    print_error (data.error ().message);
	    return 1;
	}

	std::ofstream output (*output_path, std::ios::binary | std::ios::trunc);

	if (!output) {
	    print_error (std::format ("cannot open '{}' for writing", output_path->string ()));
	    return 1;
	}

	output.write (reinterpret_cast<const char*> (data->data ()), static_cast<std::streamsize> (data->size ()));

	if (!output) {
	    print_error (std::format ("cannot write '{}'", output_path->string ()));
	    return 1;
	}

	std::print ("extracted {} bytes to {}\n", data->size (), output_path->string ());
	return 0;
    }

    std::print ("{} ({}, {} entries)\n", archive_path->string (), archive->version (), archive->entries ().size ());

    for (const auto& entry : archive->entries ()) {
	std::print ("  {:>10} {}\n", entry.size, entry.name);
    }

    return 0;
}

[[nodiscard]] bool parse_integer (std::string_view value, long long& result) {
    const auto parsed = std::from_chars (value.data (), value.data () + value.size (), result);
    return parsed.ec == std::errc {} && parsed.ptr == value.data () + value.size ();
}

[[nodiscard]] std::optional<std::filesystem::path> resolve_target (std::string_view value) {
    if (value.empty ()) {
	return std::nullopt;
    }

    const std::filesystem::path direct_path (value);
    std::error_code error;

    if (std::filesystem::exists (direct_path, error)) {
	return direct_path;
    }

    const SteamLocator locator (SteamDiscoveryOptions {});
    const SteamDiscovery discovery = locator.discover ();

    for (const auto& library : discovery.libraries) {
	if (library.workshop_content_dir.empty ()) {
	    continue;
	}

	const auto candidate = library.workshop_content_dir / std::string (value);

	if (std::filesystem::is_directory (candidate, error) || std::filesystem::is_regular_file (candidate, error)) {
	    return candidate;
	}
    }

    return std::nullopt;
}

int command_run (std::span<const std::string_view> arguments) {
    std::optional<std::string> target;
    std::optional<std::string> preset_name;
    bool explicit_fps = false;
    app::EngineOptions options;
    long long number = 0;

    const auto value = [&arguments] (std::size_t& index, std::string_view name) -> std::optional<std::string_view> {
	if (index + 1 >= arguments.size ()) {
	    print_error (std::format ("{} requires a value", name));
	    return std::nullopt;
	}

	return arguments[++index];
    };

    for (std::size_t index = 0; index < arguments.size (); ++index) {
	const std::string_view argument = arguments[index];

	if (argument == "--backend") {
	    const auto backend = value (index, argument);

	    if (!backend) {
		return 2;
	    }

	    const auto parsed = display::backend_from_string (*backend);

	    if (!parsed) {
		print_error (std::format ("unknown display backend '{}'", *backend));
		return 2;
	    }

	    options.display.backend = *parsed;
	} else if (argument == "--headless") {
	    options.display.backend = display::Backend::headless;
	} else if (argument == "--output") {
	    const auto output = value (index, argument);

	    if (!output) {
		return 2;
	    }

	    options.display.output = std::string (*output);
	} else if (argument == "--preset") {
	    const auto name = value (index, argument);

	    if (!name) {
		return 2;
	    }

	    preset_name = std::string (*name);
	} else if (argument == "--fps") {
	    const auto fps = value (index, argument);

	    if (!fps || !parse_integer (*fps, number) || number < presets::min_fps || number > presets::max_fps) {
		print_error ("--fps expects a number between 1 and 1000");
		return 2;
	    }

	    options.fps_ac = static_cast<int> (number);
	    explicit_fps = true;
	} else if (argument == "--battery-fps") {
	    const auto fps = value (index, argument);

	    if (!fps || !parse_integer (*fps, number) || number < 0 || number > 1000) {
		print_error ("--battery-fps expects a number between 0 and 1000");
		return 2;
	    }

	    options.fps_battery = static_cast<int> (number);
	} else if (argument == "--pause-on-battery") {
	    options.pause_on_battery = true;
	} else if (argument == "--no-pause-fullscreen") {
	    options.pause_on_fullscreen = false;
	} else if (argument == "--no-vsync") {
	    options.display.vsync = false;
	} else if (argument == "--audio") {
	    options.audio = true;
	} else if (argument == "--volume") {
	    const auto volume = value (index, argument);

	    if (!volume || !parse_integer (*volume, number) || number < 0 || number > 100) {
		print_error ("--volume expects a number between 0 and 100");
		return 2;
	    }

	    options.volume = static_cast<float> (number) / 100.0F;
	} else if (argument == "--width" || argument == "--height") {
	    const auto size = value (index, argument);

	    if (!size || !parse_integer (*size, number) || number <= 0 || number > 16384) {
		print_error (std::format ("{} expects a positive resolution", argument));
		return 2;
	    }

	    if (argument == "--width") {
		options.display.headless_width = static_cast<int> (number);
	    } else {
		options.display.headless_height = static_cast<int> (number);
	    }
	} else if (argument == "--frames") {
	    const auto frames = value (index, argument);

	    if (!frames || !parse_integer (*frames, number) || number < 0) {
		print_error ("--frames expects a non-negative number");
		return 2;
	    }

	    options.max_frames = static_cast<std::uint64_t> (number);
	} else if (argument == "--screenshot") {
	    const auto path = value (index, argument);

	    if (!path) {
		return 2;
	    }

	    options.screenshot = std::filesystem::path (*path);
	} else if (argument == "--verbose") {
	    options.verbose = true;
	} else if (argument == "--bg") {
	    const auto bg = value (index, argument);
	    if (!bg) {
		return 2;
	    }
	    target = std::string (*bg);
	} else if (argument == "--screen-root") {
	    const auto output = value (index, argument);
	    if (!output) {
		return 2;
	    }
	    options.display.output = std::string (*output);
	} else if (argument == "--screen-all") {
	    // Default in `we` is rendering to all outputs; accepted for compatibility
	} else if (argument == "--profile") {
	    const auto name = value (index, argument);
	    if (!name) {
		return 2;
	    }
	    preset_name = std::string (*name);
	} else if (argument == "--fps-battery") {
	    const auto fps = value (index, argument);
	    if (!fps || !parse_integer (*fps, number) || number < 0 || number > 1000) {
		print_error ("--fps-battery expects a number between 0 and 1000");
		return 2;
	    }
	    options.fps_battery = static_cast<int> (number);
	} else if (argument == "--no-fullscreen-pause") {
	    options.pause_on_fullscreen = false;
	} else if (
	    argument == "--scaling" || argument == "--assets-dir" || argument == "--hwdec" || argument == "--idle-pause"
	) {
	    const auto dummy = value (index, argument);
	    if (!dummy) {
		return 2;
	    }
	} else if (argument == "--disable-mouse") {
	    options.disable_mouse = true;
	} else if (argument == "--disable-parallax") {
	    options.disable_parallax = true;
	} else if (argument == "--silent") {
	    options.audio = false;
	} else if (argument == "--disable-particles" || argument == "--no-audio-processing") {
	    // Accepted no-op flags for backwards compatibility
	} else if (argument == "--help" || argument == "-h") {
	    print_usage ();
	    return 0;
	} else if (argument.starts_with ("--")) {
	    print_error (std::format ("unknown option '{}'", argument));
	    return 2;
	} else if (target.has_value ()) {
	    print_error ("only one wallpaper may be played at a time");
	    return 2;
	} else {
	    target = std::string (argument);
	}
    }

    if (!target.has_value ()) {
	print_error ("missing PATH or WORKSHOP_ID");
	return 2;
    }

    if (preset_name) {
	auto user_presets = presets::load (presets::default_path ());

	if (!user_presets) {
	    print_error (user_presets.error ().message);
	    return 1;
	}

	const auto preset = presets::find (*user_presets, *preset_name);

	if (!preset) {
	    print_error (std::format ("unknown preset '{}'", *preset_name));
	    return 2;
	}

	// An explicit --fps always wins over the preset, whatever the order.
	if (!explicit_fps) {
	    options.fps_ac = preset->fps;
	}
    }

    const auto resolved = resolve_target (*target);

    if (!resolved.has_value ()) {
	print_error (std::format ("cannot find wallpaper '{}'", *target));
	return 1;
    }

    auto engine = app::Engine::open (*resolved, options);

    if (!engine) {
	print_error (engine.error ().message);
	return 1;
    }

    std::print (
	"playing '{}' on {} output(s) using {}\n", (*engine)->project ().title, (*engine)->output_count (),
	display::to_string (options.display.backend)
    );

    return (*engine)->run ();
}

int command_list (std::span<const std::string_view> arguments) {
    bool json = false;

    for (const auto argument : arguments) {
	if (argument == "--json") {
	    json = true;
	} else if (argument == "--help" || argument == "-h") {
	    print_usage ();
	    return 0;
	} else {
	    print_error (std::format ("unknown option '{}'", argument));
	    return 2;
	}
    }

    const SteamLocator locator (SteamDiscoveryOptions {});
    const SteamDiscovery discovery = locator.discover ();

    struct Item {
	std::string id;
	std::string title;
	std::string type;
	std::string file;
	std::string path;
	std::string preview;
    };

    std::vector<Item> items;

    for (const auto& library : discovery.libraries) {
	std::error_code error;

	if (library.workshop_content_dir.empty ()
	    || !std::filesystem::is_directory (library.workshop_content_dir, error)) {
	    continue;
	}

	for (const auto& entry : std::filesystem::directory_iterator (library.workshop_content_dir, error)) {
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
		Item { .id = entry.path ().filename ().string (),
		       .title = project->title,
		       .type = std::string (scene::to_string (project->type)),
		       .file = project->file,
		       .path = entry.path ().string (),
		       .preview = project->preview }
	    );
	}
    }

    std::ranges::sort (items, [] (const Item& left, const Item& right) { return left.title < right.title; });

    if (json) {
	std::string output = "[\n";

	for (std::size_t index = 0; index < items.size (); ++index) {
	    output += "  {\"id\": \"" + json_escape (items[index].id) + "\", \"title\": \""
		+ json_escape (items[index].title) + "\", \"type\": \"" + json_escape (items[index].type)
		+ "\", \"file\": \"" + json_escape (items[index].file) + "\", \"path\": \""
		+ json_escape (items[index].path) + "\", \"preview\": \"" + json_escape (items[index].preview) + "\"}";
	    output += index + 1 == items.size () ? "\n" : ",\n";
	}

	output += "]\n";
	std::print ("{}", output);
	return 0;
    }

    for (const auto& item : items) {
	std::print ("{:>12}  {:<8}  {}\n", item.id, item.type, item.title);
    }

    std::print ("\n{} wallpaper(s)\n", items.size ());
    return 0;
}

int command_doctor () {
    std::print ("linux-wallpaperengine {}\n", WE_VERSION);

    const char* display_env = std::getenv ("DISPLAY");
    const char* wayland = std::getenv ("WAYLAND_DISPLAY");
    std::print (
	"display: DISPLAY={} WAYLAND_DISPLAY={}\n", display_env != nullptr ? display_env : "(unset)",
	wayland != nullptr ? wayland : "(unset)"
    );

    auto headless = gfx::Context::create_headless ();

    if (headless) {
	std::print ("OpenGL: {} / {}\n", headless->version (), headless->renderer ());
    } else {
	std::print ("OpenGL: unavailable ({})\n", headless.error ().message);
    }

    display::DriverOptions driver_options;
    auto driver = display::create_driver (driver_options);

    if (driver) {
	std::print (
	    "driver: {} ({}) with {} output(s)\n", (*driver)->name (), display::to_string ((*driver)->backend ()),
	    (*driver)->outputs ().size ()
	);

	for (const auto& output : (*driver)->outputs ()) {
	    std::print (
		"  {}: {}x{}+{}+{} @ {:.2f} Hz{}\n", output.name, output.width, output.height, output.x, output.y,
		output.refresh_hz, output.primary ? " primary" : ""
	    );
	}
    } else {
	std::print ("driver: unavailable ({})\n", driver.error ().message);
    }

    const we::power::BatteryMonitor battery;
    const auto power = battery.poll ();
    std::print (
	"power: {}{}\n", power.source == we::power::PowerSource::battery ? "battery" : "ac",
	power.battery_percent >= 0 ? " " + std::to_string (power.battery_percent) + "%" : ""
    );

    const SteamLocator locator (SteamDiscoveryOptions {});
    const auto discovery = locator.discover ();
    std::print (
	"steam: {} libraries, {} wallpaper engine installation(s)\n", discovery.libraries.size (),
	discovery.wallpaper_engine_installations.size ()
    );

    for (const auto& warning : discovery.warnings) {
	std::print ("warning: {}\n", warning);
    }

    return 0;
}

} // namespace

int main (int argc, char* argv[]) {
    std::vector<std::string_view> arguments;
    arguments.reserve (argc > 1 ? static_cast<std::size_t> (argc - 1) : 0);

    for (int index = 1; index < argc; ++index) {
	arguments.emplace_back (argv[index]);
    }

    if (arguments.empty ()) {
	print_usage ();
	return 2;
    }

    const std::span<const std::string_view> all (arguments);
    const std::string_view command = arguments.front ();
    const auto rest = all.subspan (1);

    if (command == "run") {
	return command_run (rest);
    }

    if (command == "list") {
	return command_list (rest);
    }

    if (command == "doctor") {
	return command_doctor ();
    }

    if (command == "steam") {
	return command_steam (rest);
    }

    if (command == "vfs") {
	return command_vfs (rest);
    }

    if (command == "pkg") {
	return command_pkg (rest);
    }

    if (command == "--version" || command == "-v") {
	std::print ("{} {}\n", program_name, WE_VERSION);
	return 0;
    }

    if (command == "--help" || command == "-h" || command == "help") {
	print_usage ();
	return 0;
    }

    // Direct wallpaper playback fallback: `we <target>` or `we --screen-all ...`
    return command_run (all);
}
