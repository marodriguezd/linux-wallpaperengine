#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "support/harness.hpp"
#include "support/temp_dir.hpp"
#include "we/steam/package_archive.hpp"

using we::core::ErrorCode;
using we::steam::PackageArchive;

namespace {

void append_u32 (std::vector<std::byte>& output, std::uint32_t value) {
	for (unsigned int shift = 0; shift < 32; shift += 8) {
		output.push_back (static_cast<std::byte> ((value >> shift) & 0xFFU));
	}
}

void append_raw (std::vector<std::byte>& output, std::string_view value) {
	for (const char character : value) {
		output.push_back (static_cast<std::byte> (static_cast<unsigned char> (character)));
	}
}

struct File {
	std::string name;
	std::string content;
};

[[nodiscard]] std::vector<std::byte> make_archive (
	std::string_view version, const std::vector<File>& files, bool nul_terminated_header = false) {
	std::vector<std::byte> output;

	std::string header (version);

	if (nul_terminated_header) {
		header.push_back ('\0');
	}

	append_u32 (output, static_cast<std::uint32_t> (header.size ()));
	append_raw (output, header);
	append_u32 (output, static_cast<std::uint32_t> (files.size ()));

	std::uint32_t offset = 0;

	for (const auto& file : files) {
		append_u32 (output, static_cast<std::uint32_t> (file.name.size ()));
		append_raw (output, file.name);
		append_u32 (output, offset);
		append_u32 (output, static_cast<std::uint32_t> (file.content.size ()));
		offset += static_cast<std::uint32_t> (file.content.size ());
	}

	for (const auto& file : files) {
		append_raw (output, file.content);
	}

	return output;
}

[[nodiscard]] std::string as_string (std::span<const std::byte> data) {
	return std::string (reinterpret_cast<const char*> (data.data ()), data.size ());
}

} // namespace

TEST (pkg_reads_entries_and_zero_copy_views) {
	auto archive = PackageArchive::from_memory (
		make_archive ("PKGV0001", {{"materials/tex.json", "{\"x\":1}"}, {"scene.json", "{}"}}));

	CHECK (archive.has_value ());
	CHECK_EQ (archive->version (), "PKGV0001");
	CHECK_EQ (archive->entries ().size (), std::size_t {2});

	const auto view = archive->view (R"(Materials\TEX.JSON)");

	CHECK (view.has_value ());
	CHECK_EQ (as_string (*view), "{\"x\":1}");

	const auto data = archive->data ();
	CHECK (view->data () >= data.data ());
	CHECK (view->data () + view->size () <= data.data () + data.size ());
}

TEST (pkg_accepts_newer_versions_and_null_terminated_headers) {
	auto archive = PackageArchive::from_memory (
		make_archive ("PKGV0009", {{"scene.json", "{}"}}, true));

	CHECK (archive.has_value ());
	CHECK_EQ (archive->version (), "PKGV0009");
	CHECK (archive->contains ("scene.json"));
	CHECK (!archive->contains ("missing.json"));
	CHECK (archive->find ("missing.json") == nullptr);
}

TEST (pkg_maps_files_for_zero_copy_access) {
	we::test::TempDir temp;
	const auto path = temp / "scene.pkg";

	const auto bytes = make_archive ("PKGV0001", {{"scene.json", "{}"}});
	we::test::write_bytes (path, bytes);

	auto archive = PackageArchive::open (path);

	CHECK (archive.has_value ());
	CHECK (archive->is_mapped ());
	CHECK_EQ (archive->entries ().size (), std::size_t {1});

	const auto view = archive->view ("scene.json");

	CHECK (view.has_value ());
	CHECK_EQ (as_string (*view), "{}");
}

TEST (pkg_rejects_invalid_headers) {
	auto empty = PackageArchive::from_memory ({});
	CHECK (!empty.has_value ());
	CHECK_EQ (empty.error ().code, ErrorCode::malformed_data);

	auto invalid = PackageArchive::from_memory (make_archive ("XXXX0001", {{"a.txt", "a"}}));
	CHECK (!invalid.has_value ());
	CHECK_EQ (invalid.error ().code, ErrorCode::malformed_data);
}

TEST (pkg_rejects_out_of_bounds_entries) {
	std::vector<std::byte> bytes;
	append_u32 (bytes, 8);
	append_raw (bytes, "PKGV0001");
	append_u32 (bytes, 1);
	append_u32 (bytes, 5);
	append_raw (bytes, "a.txt");
	append_u32 (bytes, 100);
	append_u32 (bytes, 4);

	auto archive = PackageArchive::from_memory (std::move (bytes));

	CHECK (!archive.has_value ());
	CHECK_EQ (archive.error ().code, ErrorCode::malformed_data);
	CHECK (archive.error ().message.find ("outside the archive") != std::string::npos);
}

TEST (pkg_rejects_path_traversal_names) {
	auto archive = PackageArchive::from_memory (make_archive ("PKGV0001", {{"../evil.txt", "evil"}}));

	CHECK (!archive.has_value ());
	CHECK_EQ (archive.error ().code, ErrorCode::malformed_data);
}

TEST (pkg_reports_missing_entries) {
	auto archive = PackageArchive::from_memory (make_archive ("PKGV0001", {{"scene.json", "{}"}}));

	CHECK (archive.has_value ());

	const auto view = archive->view ("nope.json");

	CHECK (!view.has_value ());
	CHECK_EQ (view.error ().code, ErrorCode::not_found);
}
