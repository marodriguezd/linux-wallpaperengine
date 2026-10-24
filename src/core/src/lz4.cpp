#include "we/core/lz4.hpp"

#include <cstdint>
#include <cstring>
#include <string>
#include <utility>

namespace we::core::lz4 {
namespace {

constexpr std::size_t minimum_match = 4;
constexpr std::size_t length_extension_marker = 15;

[[nodiscard]] std::uint8_t byte_at (std::span<const std::byte> input, std::size_t index) noexcept {
	return std::to_integer<std::uint8_t> (input[index]);
}

[[nodiscard]] auto corrupt (std::string message) {
	return fail (ErrorCode::malformed_data, std::move (message));
}

} // namespace

std::expected<std::vector<std::byte>, Error> decompress_block (
	std::span<const std::byte> input, std::size_t output_size) {
	std::vector<std::byte> output (output_size);

	std::size_t input_position = 0;
	std::size_t output_position = 0;
	const std::size_t input_size = input.size ();

	while (input_position < input_size) {
		const std::uint8_t token = byte_at (input, input_position++);

		std::size_t literal_length = token >> 4;

		if (literal_length == length_extension_marker) {
			std::uint8_t extension = 0;

			do {
				if (input_position >= input_size) {
					return corrupt ("truncated literal length");
				}

				extension = byte_at (input, input_position++);
				literal_length += extension;
			} while (extension == 0xFF);
		}

		if (literal_length > input_size - input_position) {
			return corrupt ("literal run exceeds the compressed block");
		}

		if (literal_length > output_size - output_position) {
			return corrupt ("literal run exceeds the decompressed size");
		}

		if (literal_length > 0) {
			std::memcpy (
				output.data () + output_position, input.data () + input_position, literal_length);
			input_position += literal_length;
			output_position += literal_length;
		}

		// The last sequence of a block contains literals only.
		if (input_position == input_size) {
			break;
		}

		if (input_size - input_position < 2) {
			return corrupt ("truncated match offset");
		}

		const std::uint16_t offset = static_cast<std::uint16_t> (
			byte_at (input, input_position) | (byte_at (input, input_position + 1) << 8));
		input_position += 2;

		if (offset == 0) {
			return corrupt ("match offset is zero");
		}

		if (offset > output_position) {
			return corrupt ("match offset points before the decompressed data");
		}

		std::size_t match_length = token & 0x0F;

		if (match_length == length_extension_marker) {
			std::uint8_t extension = 0;

			do {
				if (input_position >= input_size) {
					return corrupt ("truncated match length");
				}

				extension = byte_at (input, input_position++);
				match_length += extension;
			} while (extension == 0xFF);
		}

		match_length += minimum_match;

		if (match_length > output_size - output_position) {
			return corrupt ("match exceeds the decompressed size");
		}

		// Matches may overlap the output being written (offset < match length),
		// so copy byte by byte, exactly like the reference decoder.
		for (std::size_t index = 0; index < match_length; ++index) {
			output[output_position + index] = output[output_position - offset + index];
		}

		output_position += match_length;
	}

	if (output_position != output_size) {
		return corrupt (
			"decompressed size mismatch: expected " + std::to_string (output_size) + " bytes, got "
			+ std::to_string (output_position));
	}

	return output;
}

} // namespace we::core::lz4
