#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "we/core/error.hpp"
#include "we/core/mapped_file.hpp"

namespace we::steam {

struct PackageEntry {
	std::string name;
	std::uint64_t offset = 0;
	std::uint32_t size = 0;
};

// Reader for the PKGV archives Wallpaper Engine stores scenes and assets in.
//
// The file is memory mapped and `view()` returns spans directly into the
// mapping: no per-asset copy and no extraction to disk. The archive keeps the
// mapping alive for as long as the spans are used.
//
// PKGV directory entries are stored uncompressed. LZ4 blocks appear inside the
// TEXB texture payloads contained in these archives; the decoder lives in
// we/core/lz4.hpp and is applied by the texture loader.
class PackageArchive {
public:
	PackageArchive (const PackageArchive&) = delete;
	PackageArchive& operator= (const PackageArchive&) = delete;

	PackageArchive (PackageArchive&& other) noexcept = default;
	PackageArchive& operator= (PackageArchive&& other) noexcept = default;

	[[nodiscard]] static std::expected<PackageArchive, core::Error> open (const std::filesystem::path& path);

	[[nodiscard]] static std::expected<PackageArchive, core::Error> from_memory (std::vector<std::byte> data);

	// e.g. "PKGV0001".
	[[nodiscard]] std::string_view version () const noexcept { return this->version_; }

	[[nodiscard]] std::span<const PackageEntry> entries () const noexcept { return this->entries_; }

	[[nodiscard]] const PackageEntry* find (std::string_view name) const;

	[[nodiscard]] bool contains (std::string_view name) const { return this->find (name) != nullptr; }

	// Zero-copy access to an entry. The returned span is valid for the lifetime
	// of the archive.
	[[nodiscard]] std::expected<std::span<const std::byte>, core::Error> view (std::string_view name) const;

	// Full archive contents (the mapped file).
	[[nodiscard]] std::span<const std::byte> data () const noexcept { return this->file_.bytes (); }

	[[nodiscard]] bool is_mapped () const noexcept { return this->file_.is_mapped (); }

private:
	PackageArchive () = default;

	[[nodiscard]] std::expected<void, core::Error> parse ();

	core::MappedFile file_;
	std::string source_;
	std::string version_;
	std::uint64_t base_offset_ = 0;
	std::vector<PackageEntry> entries_;
	std::unordered_map<std::string, std::size_t> index_;
};

} // namespace we::steam
