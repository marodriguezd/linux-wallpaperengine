#include "we/steam/package_archive.hpp"

#include <cctype>
#include <optional>
#include <utility>

#include "we/core/text.hpp"

namespace we::steam {
namespace {

constexpr std::uint32_t maximum_header_size = 64;
constexpr std::uint32_t maximum_name_size = 4096;
constexpr std::uint32_t maximum_entry_count = 1'000'000;

class Reader {
public:
	explicit Reader (std::span<const std::byte> data) : data_ (data) { }

	[[nodiscard]] bool read_u32 (std::uint32_t& value) {
		if (this->remaining () < 4) {
			return false;
		}

		value = static_cast<std::uint32_t> (std::to_integer<std::uint8_t> (this->data_[this->position_]))
			| (static_cast<std::uint32_t> (std::to_integer<std::uint8_t> (this->data_[this->position_ + 1])) << 8)
			| (static_cast<std::uint32_t> (std::to_integer<std::uint8_t> (this->data_[this->position_ + 2])) << 16)
			| (static_cast<std::uint32_t> (std::to_integer<std::uint8_t> (this->data_[this->position_ + 3])) << 24);

		this->position_ += 4;
		return true;
	}

	[[nodiscard]] bool read_string (std::size_t length, std::string& output) {
		if (length > this->remaining ()) {
			return false;
		}

		if (length > 0) {
			output.assign (
				reinterpret_cast<const char*> (this->data_.data () + this->position_), length);
		} else {
			output.clear ();
		}

		this->position_ += length;
		return true;
	}

	[[nodiscard]] std::size_t position () const noexcept { return this->position_; }

	[[nodiscard]] std::size_t remaining () const noexcept { return this->data_.size () - this->position_; }

private:
	std::span<const std::byte> data_;
	std::size_t position_ = 0;
};

[[nodiscard]] std::optional<std::string> normalize_entry_name (std::string_view raw_name) {
	std::string name = core::to_forward_slashes (raw_name);

	while (!name.empty () && name.back () == '\0') {
		name.pop_back ();
	}

	while (!name.empty () && name.front () == '/') {
		name.erase (0, 1);
	}

	if (name.empty ()) {
		return std::nullopt;
	}

	// Reject traversal so an entry can never address data outside the archive
	// namespace when it is later written out or looked up by a virtual path.
	std::size_t start = 0;

	while (start <= name.size ()) {
		const std::size_t end = name.find ('/', start);
		const std::string_view component
			= std::string_view (name).substr (start, end == std::string::npos ? std::string::npos : end - start);

		if (component.empty () || component == "." || component == "..") {
			return std::nullopt;
		}

		if (end == std::string::npos) {
			break;
		}

		start = end + 1;
	}

	return name;
}

[[nodiscard]] std::string normalize_query (std::string_view name) {
	std::string result = core::to_forward_slashes (name);

	while (!result.empty () && result.front () == '/') {
		result.erase (0, 1);
	}

	return core::to_lower_ascii (result);
}

} // namespace

std::expected<PackageArchive, core::Error> PackageArchive::open (const std::filesystem::path& path) {
	auto file = core::MappedFile::open (path);

	if (!file) {
		return std::unexpected (std::move (file.error ()));
	}

	PackageArchive archive;
	archive.file_ = std::move (*file);
	archive.source_ = path.string ();

	if (auto parsed = archive.parse (); !parsed) {
		return std::unexpected (std::move (parsed.error ()));
	}

	return archive;
}

std::expected<PackageArchive, core::Error> PackageArchive::from_memory (std::vector<std::byte> data) {
	PackageArchive archive;
	archive.file_ = core::MappedFile::from_owned (std::move (data));
	archive.source_ = "<memory>";

	if (auto parsed = archive.parse (); !parsed) {
		return std::unexpected (std::move (parsed.error ()));
	}

	return archive;
}

std::expected<void, core::Error> PackageArchive::parse () {
	const auto bytes = this->file_.bytes ();

	if (bytes.size () < 8) {
		return core::fail (core::ErrorCode::malformed_data, this->source_ + ": file is too small to be a PKGV archive");
	}

	Reader reader (bytes);

	std::uint32_t header_size = 0;

	if (!reader.read_u32 (header_size) || header_size == 0 || header_size > maximum_header_size) {
		return core::fail (core::ErrorCode::malformed_data, this->source_ + ": invalid header size");
	}

	std::string header;

	if (!reader.read_string (header_size, header)) {
		return core::fail (core::ErrorCode::malformed_data, this->source_ + ": truncated header");
	}

	while (!header.empty () && header.back () == '\0') {
		header.pop_back ();
	}

	if (header.size () < 8 || header.compare (0, 4, "PKGV") != 0) {
		return core::fail (
			core::ErrorCode::malformed_data,
			this->source_ + ": expected a PKGV header, got '" + header + "'");
	}

	for (std::size_t index = 4; index < 8; ++index) {
		if (std::isdigit (static_cast<unsigned char> (header[index])) == 0) {
			return core::fail (
				core::ErrorCode::malformed_data, this->source_ + ": invalid PKGV version '" + header + "'");
		}
	}

	this->version_ = header.substr (0, 8);

	std::uint32_t entry_count = 0;

	if (!reader.read_u32 (entry_count)) {
		return core::fail (core::ErrorCode::malformed_data, this->source_ + ": truncated entry count");
	}

	if (entry_count > maximum_entry_count) {
		return core::fail (core::ErrorCode::malformed_data, this->source_ + ": unreasonable entry count");
	}

	// Every entry needs at least a 4 byte name length, one name byte and two
	// 4 byte fields. Reject counts the file cannot possibly contain before
	// reserving memory for them.
	if (entry_count > reader.remaining () / 13) {
		return core::fail (core::ErrorCode::malformed_data, this->source_ + ": entry directory exceeds the archive");
	}

	this->entries_.reserve (entry_count);

	for (std::uint32_t index = 0; index < entry_count; ++index) {
		std::uint32_t name_size = 0;

		if (!reader.read_u32 (name_size) || name_size == 0 || name_size > maximum_name_size) {
			return core::fail (core::ErrorCode::malformed_data, this->source_ + ": invalid entry name size");
		}

		std::string raw_name;

		if (!reader.read_string (name_size, raw_name)) {
			return core::fail (core::ErrorCode::malformed_data, this->source_ + ": truncated entry name");
		}

		auto name = normalize_entry_name (raw_name);

		if (!name) {
			return core::fail (
				core::ErrorCode::malformed_data, this->source_ + ": invalid entry name '" + raw_name + "'");
		}

		std::uint32_t offset = 0;
		std::uint32_t size = 0;

		if (!reader.read_u32 (offset) || !reader.read_u32 (size)) {
			return core::fail (core::ErrorCode::malformed_data, this->source_ + ": truncated entry directory");
		}

		this->index_.try_emplace (core::to_lower_ascii (*name), this->entries_.size ());
		this->entries_.push_back (PackageEntry {.name = std::move (*name), .offset = offset, .size = size});
	}

	this->base_offset_ = reader.position ();

	const std::uint64_t available = bytes.size () - this->base_offset_;

	for (const auto& entry : this->entries_) {
		if (entry.offset > available || entry.size > available - entry.offset) {
			return core::fail (
				core::ErrorCode::malformed_data,
				this->source_ + ": entry '" + entry.name + "' points outside the archive");
		}
	}

	return {};
}

const PackageEntry* PackageArchive::find (std::string_view name) const {
	const auto found = this->index_.find (normalize_query (name));

	if (found == this->index_.end ()) {
		return nullptr;
	}

	return &this->entries_[found->second];
}

std::expected<std::span<const std::byte>, core::Error> PackageArchive::view (std::string_view name) const {
	const PackageEntry* entry = this->find (name);

	if (entry == nullptr) {
		return core::fail (
			core::ErrorCode::not_found, this->source_ + ": no entry named '" + std::string (name) + "'");
	}

	const auto bytes = this->file_.bytes ();
	const auto start = static_cast<std::size_t> (this->base_offset_ + entry->offset);

	return bytes.subspan (start, entry->size);
}

} // namespace we::steam
