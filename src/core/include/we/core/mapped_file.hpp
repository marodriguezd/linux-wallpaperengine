#pragma once

#include <cstddef>
#include <expected>
#include <filesystem>
#include <span>
#include <vector>

#include "we/core/error.hpp"

namespace we::core {

// Read-only view over a file. Files are memory mapped when possible, so callers
// can keep spans into the mapping instead of copying whole archives around.
//
// Lifetime contract: every span returned by bytes() is invalidated when the
// MappedFile is destroyed or moved from.
class MappedFile {
public:
	MappedFile () = default;
	~MappedFile ();

	MappedFile (const MappedFile&) = delete;
	MappedFile& operator= (const MappedFile&) = delete;

	MappedFile (MappedFile&& other) noexcept;
	MappedFile& operator= (MappedFile&& other) noexcept;

	[[nodiscard]] static std::expected<MappedFile, Error> open (const std::filesystem::path& path);

	// Ownership transfer of already loaded bytes. Used by tests and by callers
	// that receive data from somewhere other than a file.
	[[nodiscard]] static MappedFile from_owned (std::vector<std::byte> data);

	[[nodiscard]] std::span<const std::byte> bytes () const noexcept;
	[[nodiscard]] std::size_t size () const noexcept { return this->size_; }
	[[nodiscard]] bool empty () const noexcept { return this->size_ == 0; }
	[[nodiscard]] bool is_mapped () const noexcept { return this->mapped_; }

private:
	void reset () noexcept;

	const std::byte* data_ = nullptr;
	std::size_t size_ = 0;
	bool mapped_ = false;
	std::vector<std::byte> owned_;
};

} // namespace we::core
