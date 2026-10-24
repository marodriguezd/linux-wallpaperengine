#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace we::core {

// ASCII-only helpers. Filesystem names and VDF keys are compared byte-wise with
// ASCII folding; locale-dependent case conversion must never be used here.

[[nodiscard]] constexpr char ascii_lower (char value) noexcept {
	if (value >= 'A' && value <= 'Z') {
		return static_cast<char> (value - 'A' + 'a');
	}

	return value;
}

[[nodiscard]] inline std::string to_lower_ascii (std::string_view text) {
	std::string result;
	result.reserve (text.size());

	for (const char value : text) {
		result.push_back (ascii_lower (value));
	}

	return result;
}

[[nodiscard]] constexpr bool iequals (std::string_view left, std::string_view right) noexcept {
	if (left.size () != right.size ()) {
		return false;
	}

	for (std::size_t index = 0; index < left.size (); ++index) {
		if (ascii_lower (left[index]) != ascii_lower (right[index])) {
			return false;
		}
	}

	return true;
}

[[nodiscard]] constexpr bool istarts_with (std::string_view text, std::string_view prefix) noexcept {
	if (text.size () < prefix.size ()) {
		return false;
	}

	return iequals (text.substr (0, prefix.size ()), prefix);
}

// Replaces every '\\' with '/'. Wallpaper Engine stores Windows paths even in
// files produced on Linux, so normalization happens at the VFS boundary.
[[nodiscard]] inline std::string to_forward_slashes (std::string_view path) {
	std::string result (path);
	for (char& value : result) {
		if (value == '\\') {
			value = '/';
		}
	}

	return result;
}

} // namespace we::core
