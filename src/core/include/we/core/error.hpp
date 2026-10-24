#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <utility>

namespace we::core {

// Coarse error categories. Components add context through `message`, the code
// is what callers branch on (never parse the message).
enum class ErrorCode : std::uint8_t {
	none = 0,
	not_found,
	io,
	invalid_argument,
	malformed_data,
	unsupported,
	out_of_range,
};

[[nodiscard]] constexpr std::string_view to_string (ErrorCode code) noexcept {
	switch (code) {
		case ErrorCode::none:
			return "none";
		case ErrorCode::not_found:
			return "not_found";
		case ErrorCode::io:
			return "io";
		case ErrorCode::invalid_argument:
			return "invalid_argument";
		case ErrorCode::malformed_data:
			return "malformed_data";
		case ErrorCode::unsupported:
			return "unsupported";
		case ErrorCode::out_of_range:
			return "out_of_range";
	}

	return "unknown";
}

struct Error {
	ErrorCode code = ErrorCode::none;
	std::string message;

	[[nodiscard]] explicit operator bool () const noexcept { return code != ErrorCode::none; }

	[[nodiscard]] static Error make (ErrorCode code, std::string message) {
		return Error {.code = code, .message = std::move (message)};
	}
};

[[nodiscard]] inline auto fail (ErrorCode code, std::string message) {
	return std::unexpected (Error::make (code, std::move (message)));
}

} // namespace we::core
