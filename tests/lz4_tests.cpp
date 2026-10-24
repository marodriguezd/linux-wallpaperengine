#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

#include "support/harness.hpp"
#include "we/core/lz4.hpp"

namespace {

using we::core::lz4::decompress_block;

[[nodiscard]] std::vector<std::byte> bytes (std::initializer_list<unsigned int> values) {
	std::vector<std::byte> result;
	result.reserve (values.size ());

	for (const unsigned int value : values) {
		result.push_back (static_cast<std::byte> (value));
	}

	return result;
}

[[nodiscard]] std::string as_string (std::span<const std::byte> data) {
	return std::string (reinterpret_cast<const char*> (data.data ()), data.size ());
}

} // namespace

TEST (lz4_decodes_literal_only_block) {
	// token: 5 literals, no match
	auto result = decompress_block (bytes ({0x50, 'h', 'e', 'l', 'l', 'o'}), 5);

	CHECK (result.has_value ());
	CHECK_EQ (as_string (*result), "hello");
}

TEST (lz4_decodes_overlapping_match) {
	// one literal "a", match length 4 at offset 1 -> "aaaaa"
	auto result = decompress_block (bytes ({0x10, 'a', 0x01, 0x00}), 5);

	CHECK (result.has_value ());
	CHECK_EQ (as_string (*result), "aaaaa");
}

TEST (lz4_decodes_extended_match_length) {
	// one literal "a", match length 19 (nibble 15 + extension 0 + 4)
	auto result = decompress_block (bytes ({0x1F, 'a', 0x01, 0x00, 0x00}), 20);

	CHECK (result.has_value ());
	CHECK_EQ (as_string (*result), std::string (20, 'a'));
}

TEST (lz4_rejects_zero_offset) {
	auto result = decompress_block (bytes ({0x10, 'a', 0x00, 0x00}), 5);

	CHECK (!result.has_value ());
	CHECK_EQ (result.error ().code, we::core::ErrorCode::malformed_data);
}

TEST (lz4_rejects_truncated_literals) {
	auto result = decompress_block (bytes ({0x50, 'a'}), 5);

	CHECK (!result.has_value ());
}

TEST (lz4_rejects_size_mismatch) {
	auto result = decompress_block (bytes ({0x20, 'h', 'i'}), 3);

	CHECK (!result.has_value ());
}

TEST (lz4_handles_empty_block) {
	auto result = decompress_block ({}, 0);

	CHECK (result.has_value ());
	CHECK (result->empty ());
}
