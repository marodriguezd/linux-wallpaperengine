#pragma once

#include <cstddef>
#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "we/core/error.hpp"

namespace we::steam {

class VdfObject;

// A single KeyValues value: either a string or a nested object. The tree keeps
// insertion order because library order determines resolution priority.
class VdfValue {
public:
	VdfValue () = delete;

	explicit VdfValue (std::string value);
	explicit VdfValue (VdfObject object);

	~VdfValue ();

	VdfValue (const VdfValue&) = delete;
	VdfValue& operator= (const VdfValue&) = delete;

	VdfValue (VdfValue&& other) noexcept;
	VdfValue& operator= (VdfValue&& other) noexcept;

	[[nodiscard]] bool is_string () const noexcept;
	[[nodiscard]] bool is_object () const noexcept;

	// Precondition: is_string().
	[[nodiscard]] std::string_view as_string () const;

	// Precondition: is_object().
	[[nodiscard]] const VdfObject& as_object () const;

	// Convenience lookup on object values, case-insensitive. Returns nullptr
	// for string values or when the key is absent.
	[[nodiscard]] const VdfValue* get (std::string_view key) const;

private:
	std::variant<std::string, std::unique_ptr<VdfObject>> value_;
};

class VdfObject {
public:
	using Entry = std::pair<std::string, VdfValue>;

	[[nodiscard]] std::size_t size () const noexcept { return this->entries_.size (); }
	[[nodiscard]] bool empty () const noexcept { return this->entries_.empty (); }
	[[nodiscard]] const std::vector<Entry>& entries () const noexcept { return this->entries_; }

	// Case-insensitive key lookup (Valve keys are ASCII).
	[[nodiscard]] const VdfValue* find (std::string_view key) const;

	void append (std::string key, VdfValue value);

private:
	std::vector<Entry> entries_;
};

inline bool VdfValue::is_string () const noexcept {
	return std::holds_alternative<std::string> (this->value_);
}

inline bool VdfValue::is_object () const noexcept {
	return std::holds_alternative<std::unique_ptr<VdfObject>> (this->value_);
}

inline std::string_view VdfValue::as_string () const {
	return std::get<std::string> (this->value_);
}

inline const VdfObject& VdfValue::as_object () const {
	return *std::get<std::unique_ptr<VdfObject>> (this->value_);
}

inline const VdfValue* VdfValue::get (std::string_view key) const {
	if (!this->is_object ()) {
		return nullptr;
	}

	return this->as_object ().find (key);
}

// Parses a KeyValues document. Accepts both quoted and unquoted tokens, `//`
// comments and an optional UTF-8 BOM, matching the files Steam writes.
[[nodiscard]] std::expected<VdfValue, core::Error> parse_vdf (std::string_view text);

[[nodiscard]] std::expected<VdfValue, core::Error> parse_vdf_file (const std::filesystem::path& path);

} // namespace we::steam
