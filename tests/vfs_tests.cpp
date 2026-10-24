#include <string>
#include <string_view>

#include "support/harness.hpp"
#include "support/temp_dir.hpp"
#include "we/steam/vfs.hpp"

using we::steam::CaseInsensitiveVfs;
using we::steam::VfsErrorCode;

namespace {

struct VfsFixture {
	we::test::TempDir temp;
	std::filesystem::path library;
	std::filesystem::path wallpaper;
	std::filesystem::path project;

	VfsFixture () {
		this->library = this->temp.path () / "SteamLibrary";
		this->wallpaper = this->library / "steamapps" / "common" / "WallPaper_Engine";
		this->project = this->wallpaper / "projects" / "MyWallpaper";

		we::test::write_text (this->project / "scene.json", "{}");
		we::test::write_text (this->project / "Materials" / "tex.TEX", "texture");
		we::test::write_text (this->temp.path () / "secret.txt", "secret");
	}

	[[nodiscard]] CaseInsensitiveVfs make_vfs () const {
		return CaseInsensitiveVfs ({this->library});
	}
};

} // namespace

TEST (vfs_normalizes_windows_paths) {
	CHECK_EQ (CaseInsensitiveVfs::normalize (R"(C:\A\..\B\c/)"), "B/c");
	CHECK_EQ (CaseInsensitiveVfs::normalize ("/../../etc/passwd"), "etc/passwd");
	CHECK_EQ (CaseInsensitiveVfs::normalize (R"(\a\\b\c)"), "a/b/c");
	CHECK_EQ (CaseInsensitiveVfs::normalize ("."), "");
	CHECK_EQ (CaseInsensitiveVfs::normalize ("C:"), "");
}

TEST (vfs_resolves_absolute_windows_suffix_case_insensitively) {
	VfsFixture fixture;
	auto vfs = fixture.make_vfs ();

	const auto resolved = vfs.resolve (
		R"(C:\Program Files (x86)\Steam\steamapps\common\wallpaper_engine\projects\mywallpaper\SCENE.JSON)");

	CHECK (resolved.has_value ());
	CHECK_EQ (resolved->string (), (fixture.project / "scene.json").string ());
}

TEST (vfs_reads_text_through_case_insensitive_path) {
	VfsFixture fixture;
	auto vfs = fixture.make_vfs ();

	const auto text = vfs.read_text (
		R"(C:\Program Files (x86)\Steam\steamapps\common\WALLPAPER_ENGINE\projects\MYWALLPAPER\materials\tex.tex)");

	CHECK (text.has_value ());
	CHECK_EQ (*text, "texture");
}

TEST (vfs_lists_directories_sorted) {
	VfsFixture fixture;
	auto vfs = fixture.make_vfs ();

	const auto entries = vfs.list ("steamapps/common/wallpaper_engine/projects/mywallpaper");

	CHECK (entries.has_value ());
	CHECK_EQ (entries->size (), std::size_t {2});
	CHECK_EQ ((*entries)[0].name, "Materials");
	CHECK ((*entries)[0].directory);
	CHECK_EQ ((*entries)[1].name, "scene.json");
	CHECK (!(*entries)[1].directory);
	CHECK_EQ ((*entries)[1].size, std::uint64_t {2});
}

TEST (vfs_reports_missing_files) {
	VfsFixture fixture;
	auto vfs = fixture.make_vfs ();

	const auto resolved = vfs.resolve ("steamapps/common/nope/scene.json");

	CHECK (!resolved.has_value ());
	CHECK_EQ (resolved.error ().code, VfsErrorCode::not_found);
}

TEST (vfs_cannot_escape_roots) {
	VfsFixture fixture;
	auto vfs = fixture.make_vfs ();

	const auto resolved = vfs.resolve ("../../secret.txt");

	CHECK (!resolved.has_value ());
	CHECK_EQ (resolved.error ().code, VfsErrorCode::not_found);
}

TEST (vfs_rejects_reading_directories) {
	VfsFixture fixture;
	auto vfs = fixture.make_vfs ();

	const auto data = vfs.read ("steamapps/common");

	CHECK (!data.has_value ());
	CHECK_EQ (data.error ().code, VfsErrorCode::not_a_file);
}

TEST (vfs_caches_directory_indexes) {
	VfsFixture fixture;
	auto vfs = fixture.make_vfs ();

	const auto first = vfs.resolve ("steamapps/common/wallpaper_engine/projects/mywallpaper/scene.json");
	const auto second = vfs.resolve ("STEAMAPPS/COMMON/WALLPAPER_ENGINE/PROJECTS/MYWALLPAPER/SCENE.JSON");

	CHECK (first.has_value ());
	CHECK (second.has_value ());
	CHECK_EQ (first->string (), second->string ());
}

TEST (vfs_search_path_priority) {
	we::test::TempDir temp;
	we::test::write_text (temp.path () / "first" / "shared.txt", "first");
	we::test::write_text (temp.path () / "second" / "shared.txt", "second");

	CaseInsensitiveVfs vfs ({temp.path () / "first", temp.path () / "second"});

	const auto text = vfs.read_text ("shared.txt");

	CHECK (text.has_value ());
	CHECK_EQ (*text, "first");
}
