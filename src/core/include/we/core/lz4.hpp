#pragma once

#include <cstddef>
#include <expected>
#include <span>
#include <vector>

#include "we/core/error.hpp"

namespace we::core::lz4 {

// Decompresses a raw LZ4 block (the format used by Wallpaper Engine for
// compressed texture mipmaps, not the LZ4 frame format).
//
// The uncompressed size is stored in the container, so the caller always knows
// the exact output size. Every read and write is bounds checked; malformed
// input produces an Error instead of undefined behaviour.
[[nodiscard]] std::expected<std::vector<std::byte>, Error> decompress_block (
	std::span<const std::byte> input, std::size_t output_size);

} // namespace we::core::lz4
