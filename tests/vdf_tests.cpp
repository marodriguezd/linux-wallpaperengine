#include <string>
#include <string_view>

#include "support/harness.hpp"
#include "support/temp_dir.hpp"
#include "we/steam/steam_locator.hpp"
#include "we/steam/vdf.hpp"

using we::core::ErrorCode;
using we::steam::parse_vdf;
using we::steam::parse_vdf_file;
using we::steam::SteamLocator;

TEST (vdf_parses_modern_libraryfolders) {
	constexpr std::string_view document = R"vdf(
// library list written by Steam
"libraryfolders"
{
	"0"
	{
		"path" "/home/me/.local/share/Steam"
		"apps" { "431960" "12345" }
	}
	"1"
	{
		"path" "/mnt/games/SteamLibrary"
	}
}
)vdf";

	const auto parsed = parse_vdf (document);

	CHECK (parsed.has_value ());
	CHECK (parsed->is_object ());

	const auto* folders = parsed->get ("LibraryFolders");
	CHECK (folders != nullptr);
	CHECK (folders->is_object ());
	CHECK_EQ (folders->as_object ().size (), std::size_t {2});

	const auto* first = folders->get ("0");
	CHECK (first != nullptr);
	CHECK_EQ (first->get ("path")->as_string (), "/home/me/.local/share/Steam");
	CHECK_EQ (first->get ("APPS")->get ("431960")->as_string (), "12345");
}

TEST (vdf_decodes_escaped_windows_paths) {
	constexpr std::string_view document = R"vdf(
"key"
{
	"path" "C:\\Program Files (x86)\\Steam"
}
)vdf";

	const auto parsed = parse_vdf (document);

	CHECK (parsed.has_value ());
	CHECK_EQ (parsed->get ("key")->get ("path")->as_string (), R"(C:\Program Files (x86)\Steam)");
}

TEST (vdf_accepts_unquoted_tokens_comments_and_bom) {
	constexpr std::string_view document
		= "\xEF\xBB\xBF" R"vdf(
libraryfolders // header
{
	path /mnt/games/SteamLibrary // unquoted value
}
)vdf";

	const auto parsed = parse_vdf (document);

	CHECK (parsed.has_value ());
	CHECK_EQ (parsed->get ("libraryfolders")->get ("path")->as_string (), "/mnt/games/SteamLibrary");
}

TEST (vdf_reconstructs_legacy_libraryfolders) {
	we::test::TempDir temp;
	const auto path = temp / "libraryfolders.vdf";

	we::test::write_text (
		path,
		R"vdf(
"LibraryFolders"
{
	"TimeNextStatsReport" "123"
	"1" "/mnt/games/SteamLibrary"
	"2" { "path" "/mnt/other/SteamLibrary" }
	"3" "D:\\WindowsLibrary"
}
)vdf");

	const auto libraries = SteamLocator::parse_library_folders (path);

	CHECK (libraries.has_value ());
	CHECK_EQ (libraries->size (), std::size_t {2});
	CHECK_EQ ((*libraries)[0].string (), "/mnt/games/SteamLibrary");
	CHECK_EQ ((*libraries)[1].string (), "/mnt/other/SteamLibrary");
}

TEST (vdf_reports_unterminated_objects) {
	const auto parsed = parse_vdf (R"vdf("libraryfolders" { "0" { "path" "/tmp" )vdf");

	CHECK (!parsed.has_value ());
	CHECK_EQ (parsed.error ().code, ErrorCode::malformed_data);
	CHECK (parsed.error ().message.find ("unterminated") != std::string::npos);
}

TEST (vdf_rejects_excessive_nesting) {
	std::string document;

	for (int depth = 0; depth < 100; ++depth) {
		document += "\"k\" {";
	}

	document += "\"value\" \"x\"";

	for (int depth = 0; depth < 100; ++depth) {
		document += '}';
	}

	const auto parsed = parse_vdf (document);

	CHECK (!parsed.has_value ());
	CHECK_EQ (parsed.error ().code, ErrorCode::malformed_data);
	CHECK (parsed.error ().message.find ("too deep") != std::string::npos);
}

TEST (vdf_file_reading_reports_missing_files) {
	const auto parsed = parse_vdf_file ("/definitely/not/a/vdf/file.vdf");

	CHECK (!parsed.has_value ());
	CHECK_EQ (parsed.error ().code, ErrorCode::io);
}
