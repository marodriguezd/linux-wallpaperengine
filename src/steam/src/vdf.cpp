#include "we/steam/vdf.hpp"

#include <cstdint>
#include <fstream>
#include <string>

#include "we/core/text.hpp"

namespace we::steam {
namespace {

constexpr std::size_t maximum_file_size = 64U * 1024U * 1024U;
constexpr std::size_t maximum_nesting_depth = 64;

[[nodiscard]] bool is_space (char value) noexcept {
	return value == ' ' || value == '\t' || value == '\r' || value == '\n' || value == '\0';
}

[[nodiscard]] bool is_token_terminator (char value) noexcept {
	return is_space (value) || value == '{' || value == '}' || value == '"';
}

class Parser {
public:
	explicit Parser (std::string_view text) : text_ (text) {
		if (this->text_.size () >= 3 && static_cast<unsigned char> (this->text_[0]) == 0xEF
			&& static_cast<unsigned char> (this->text_[1]) == 0xBB
			&& static_cast<unsigned char> (this->text_[2]) == 0xBF) {
			this->position_ = 3;
		}
	}

	[[nodiscard]] std::expected<VdfValue, core::Error> parse_document () {
		VdfObject root;

		this->skip_space ();

		while (!this->at_end ()) {
			auto key = this->parse_key ();
			if (!key) {
				return std::unexpected (std::move (key.error ()));
			}

			auto value = this->parse_value (0);
			if (!value) {
				return std::unexpected (std::move (value.error ()));
			}

			root.append (std::move (*key), std::move (*value));
			this->skip_space ();
		}

		return VdfValue (std::move (root));
	}

private:
	[[nodiscard]] bool at_end () const noexcept { return this->position_ >= this->text_.size (); }

	[[nodiscard]] core::Error make_error (std::string message) const {
		return core::Error::make (
			core::ErrorCode::malformed_data, "VDF line " + std::to_string (this->line_) + ": " + message);
	}

	void skip_space () {
		while (!this->at_end ()) {
			const char current = this->text_[this->position_];

			if (current == '\n') {
				++this->line_;
				++this->position_;
				continue;
			}

			if (is_space (current)) {
				++this->position_;
				continue;
			}

			if (current == '/' && this->position_ + 1 < this->text_.size ()
				&& this->text_[this->position_ + 1] == '/') {
				while (!this->at_end () && this->text_[this->position_] != '\n') {
					++this->position_;
				}
				continue;
			}

			break;
		}
	}

	[[nodiscard]] std::expected<std::string, core::Error> parse_key () {
		if (this->at_end ()) {
			return std::unexpected (this->make_error ("expected a key"));
		}

		if (this->text_[this->position_] == '{' || this->text_[this->position_] == '}') {
			return std::unexpected (this->make_error ("expected a key"));
		}

		return this->parse_token ();
	}

	[[nodiscard]] std::expected<VdfValue, core::Error> parse_value (std::size_t depth) {
		this->skip_space ();

		if (this->at_end ()) {
			return std::unexpected (this->make_error ("expected a value"));
		}

		if (this->text_[this->position_] == '}') {
			return std::unexpected (this->make_error ("unexpected '}' while parsing a value"));
		}

		if (this->text_[this->position_] == '{') {
			auto object = this->parse_object (depth + 1);
			if (!object) {
				return std::unexpected (std::move (object.error ()));
			}

			return VdfValue (std::move (*object));
		}

		auto token = this->parse_token ();
		if (!token) {
			return std::unexpected (std::move (token.error ()));
		}

		return VdfValue (std::move (*token));
	}

	[[nodiscard]] std::expected<VdfObject, core::Error> parse_object (std::size_t depth) {
		if (depth > maximum_nesting_depth) {
			return core::fail (
				core::ErrorCode::malformed_data,
				"VDF line " + std::to_string (this->line_) + ": object nesting is too deep");
		}

		const std::size_t opening_line = this->line_;
		++this->position_; // consume '{'

		VdfObject object;
		this->skip_space ();

		while (true) {
			if (this->at_end ()) {
				return core::fail (
					core::ErrorCode::malformed_data,
					"VDF line " + std::to_string (opening_line) + ": unterminated object");
			}

			if (this->text_[this->position_] == '}') {
				++this->position_;
				return object;
			}

			auto key = this->parse_key ();
			if (!key) {
				return std::unexpected (std::move (key.error ()));
			}

			auto value = this->parse_value (depth);
			if (!value) {
				return std::unexpected (std::move (value.error ()));
			}

			object.append (std::move (*key), std::move (*value));
			this->skip_space ();
		}
	}

	[[nodiscard]] std::expected<std::string, core::Error> parse_token () {
		this->skip_space ();

		if (this->at_end ()) {
			return std::unexpected (this->make_error ("expected a token"));
		}

		if (this->text_[this->position_] == '"') {
			return this->parse_quoted ();
		}

		const std::size_t start = this->position_;

		while (!this->at_end () && !is_token_terminator (this->text_[this->position_])) {
			++this->position_;
		}

		if (start == this->position_) {
			return std::unexpected (this->make_error ("expected a token"));
		}

		return std::string (this->text_.substr (start, this->position_ - start));
	}

	[[nodiscard]] std::expected<std::string, core::Error> parse_quoted () {
		const std::size_t opening_line = this->line_;
		++this->position_; // consume '"'

		std::string result;

		while (!this->at_end ()) {
			const char current = this->text_[this->position_++];

			if (current == '"') {
				return result;
			}

			if (current == '\\' && !this->at_end ()) {
				const char escaped = this->text_[this->position_++];

				switch (escaped) {
					case '\\':
						result.push_back ('\\');
						break;
					case '"':
						result.push_back ('"');
						break;
					case 'n':
						result.push_back ('\n');
						break;
					case 'r':
						result.push_back ('\r');
						break;
					case 't':
						result.push_back ('\t');
						break;
					default:
						// Valve only defines the escapes above. Keep unknown
						// sequences verbatim instead of silently dropping data.
						result.push_back ('\\');
						result.push_back (escaped);
						break;
				}

				continue;
			}

			if (current == '\n') {
				++this->line_;
			}

			result.push_back (current);
		}

		return std::unexpected (
			core::Error::make (
				core::ErrorCode::malformed_data,
				"VDF line " + std::to_string (opening_line) + ": unterminated quoted string"));
	}

	std::string_view text_;
	std::size_t position_ = 0;
	std::size_t line_ = 1;
};

} // namespace

VdfValue::VdfValue (std::string value) : value_ (std::move (value)) { }

VdfValue::VdfValue (VdfObject object) : value_ (std::make_unique<VdfObject> (std::move (object))) { }

VdfValue::~VdfValue () = default;

VdfValue::VdfValue (VdfValue&& other) noexcept = default;

VdfValue& VdfValue::operator= (VdfValue&& other) noexcept = default;

void VdfObject::append (std::string key, VdfValue value) {
	this->entries_.emplace_back (std::move (key), std::move (value));
}

const VdfValue* VdfObject::find (std::string_view key) const {
	for (const auto& [entry_key, value] : this->entries_) {
		if (core::iequals (entry_key, key)) {
			return &value;
		}
	}

	return nullptr;
}

std::expected<VdfValue, core::Error> parse_vdf (std::string_view text) {
	if (text.empty ()) {
		return core::fail (core::ErrorCode::malformed_data, "VDF document is empty");
	}

	Parser parser (text);
	return parser.parse_document ();
}

std::expected<VdfValue, core::Error> parse_vdf_file (const std::filesystem::path& path) {
	std::error_code error_code;
	const std::uintmax_t size = std::filesystem::file_size (path, error_code);

	if (error_code) {
		return core::fail (
			core::ErrorCode::io, "cannot stat '" + path.string () + "': " + error_code.message ());
	}

	if (size > maximum_file_size) {
		return core::fail (
			core::ErrorCode::out_of_range,
			"VDF file is too large: '" + path.string () + "' (" + std::to_string (size) + " bytes)");
	}

	std::ifstream stream (path, std::ios::binary);

	if (!stream) {
		return core::fail (core::ErrorCode::io, "cannot open '" + path.string () + "'");
	}

	std::string content (static_cast<std::size_t> (size), '\0');
	stream.read (content.data (), static_cast<std::streamsize> (content.size ()));

	if (!stream || static_cast<std::size_t> (stream.gcount ()) != content.size ()) {
		return core::fail (core::ErrorCode::io, "cannot read '" + path.string () + "'");
	}

	return parse_vdf (content);
}

} // namespace we::steam
