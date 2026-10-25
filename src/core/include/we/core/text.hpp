#pragma once

#include <charconv>
#include <cstddef>
#include <optional>
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
    result.reserve (text.size ());

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

// Exception-free numeric parsing helpers using std::from_chars.
[[nodiscard]] inline std::optional<float> parse_float (std::string_view text) noexcept {
    while (!text.empty ()
	   && (text.front () == ' ' || text.front () == '\t' || text.front () == '\r' || text.front () == '\n')) {
	text.remove_prefix (1);
    }
    while (!text.empty ()
	   && (text.back () == ' ' || text.back () == '\t' || text.back () == '\r' || text.back () == '\n')) {
	text.remove_suffix (1);
    }
    if (text.empty ()) {
	return std::nullopt;
    }
    float value = 0.0F;
    const auto [ptr, ec] = std::from_chars (text.data (), text.data () + text.size (), value);
    if (ec == std::errc {} && ptr == text.data () + text.size ()) {
	return value;
    }
    return std::nullopt;
}

[[nodiscard]] inline std::optional<int> parse_int (std::string_view text, int base = 10) noexcept {
    while (!text.empty ()
	   && (text.front () == ' ' || text.front () == '\t' || text.front () == '\r' || text.front () == '\n')) {
	text.remove_prefix (1);
    }
    while (!text.empty ()
	   && (text.back () == ' ' || text.back () == '\t' || text.back () == '\r' || text.back () == '\n')) {
	text.remove_suffix (1);
    }
    if (text.empty ()) {
	return std::nullopt;
    }
    int value = 0;
    const auto [ptr, ec] = std::from_chars (text.data (), text.data () + text.size (), value, base);
    if (ec == std::errc {} && ptr == text.data () + text.size ()) {
	return value;
    }
    return std::nullopt;
}

} // namespace we::core
