#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "support/harness.hpp"
#include "support/temp_dir.hpp"
#include "we/presets/presets.hpp"

using we::core::ErrorCode;
using we::presets::Preset;

TEST (presets_missing_file_loads_empty) {
	we::test::TempDir temp;
	auto loaded = we::presets::load (temp.path () / "absent.json");
	CHECK (loaded.has_value ());
	CHECK (loaded->empty ());
}

TEST (presets_round_trip_creates_parent_directories) {
	we::test::TempDir temp;
	const auto path = temp.path () / "nested" / "presets.json";
	const std::vector<Preset> input {
		{.name = "cine", .fps = 24, .backend = "x11_root"},
		{.name = "eco", .fps = 10, .backend = "auto"},
	};

	CHECK (we::presets::save (path, input).has_value ());

	auto loaded = we::presets::load (path);
	CHECK (loaded.has_value ());
	CHECK_EQ (loaded->size (), std::size_t {2});
	CHECK_EQ ((*loaded)[0].name, "cine");
	CHECK_EQ ((*loaded)[0].fps, 24);
	CHECK_EQ ((*loaded)[0].backend, "x11_root");
	CHECK_EQ ((*loaded)[1].fps, 10);
}

TEST (presets_reject_out_of_range_fps) {
	we::test::TempDir temp;
	const auto path = temp.path () / "presets.json";
	std::ofstream (path) << R"({"presets":[{"name":"bad","fps":0}]})";

	auto loaded = we::presets::load (path);
	CHECK (!loaded.has_value ());
	CHECK_EQ (loaded.error ().code, ErrorCode::out_of_range);
}

TEST (presets_reject_malformed_json) {
	we::test::TempDir temp;
	const auto path = temp.path () / "presets.json";
	std::ofstream (path) << "{ not json";

	auto loaded = we::presets::load (path);
	CHECK (!loaded.has_value ());
	CHECK_EQ (loaded.error ().code, ErrorCode::malformed_data);
}

TEST (presets_save_rejects_invalid_entries) {
	we::test::TempDir temp;
	auto saved = we::presets::save (temp.path () / "presets.json", {{.name = "", .fps = 30}});
	CHECK (!saved.has_value ());
	CHECK_EQ (saved.error ().code, ErrorCode::invalid_argument);
}

TEST (presets_find_prefers_user_over_builtin) {
	const std::vector<Preset> user {{.name = "balanced", .fps = 45, .backend = "auto"}};

	auto user_hit = we::presets::find (user, "balanced");
	CHECK (user_hit.has_value ());
	CHECK_EQ (user_hit->fps, 45);

	auto builtin_hit = we::presets::find (user, "battery");
	CHECK (builtin_hit.has_value ());
	CHECK_EQ (builtin_hit->fps, 15);

	CHECK (!we::presets::find (user, "missing").has_value ());
}

TEST (presets_default_path_uses_xdg_config_home) {
	::setenv ("XDG_CONFIG_HOME", "/tmp/we_xdg_test", 1);
	const auto path = we::presets::default_path ();
	::unsetenv ("XDG_CONFIG_HOME");
	CHECK_EQ (path.string (), std::string ("/tmp/we_xdg_test/we-wallpaper/presets.json"));
}
