#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <unistd.h>

namespace we::test {

class TempDir {
public:
	TempDir () : path_ (create ()) { }

	~TempDir () {
		std::error_code error_code;
		std::filesystem::remove_all (this->path_, error_code);
	}

	TempDir (const TempDir&) = delete;
	TempDir& operator= (const TempDir&) = delete;

	[[nodiscard]] const std::filesystem::path& path () const noexcept { return this->path_; }

	[[nodiscard]] std::filesystem::path operator/ (std::string_view child) const {
		return this->path_ / std::string (child);
	}

private:
	[[nodiscard]] static std::filesystem::path create () {
		static std::atomic<std::uint64_t> counter {0};

		const std::filesystem::path path = std::filesystem::temp_directory_path ()
			/ ("we-tests-" + std::to_string (::getpid ()) + "-" + std::to_string (counter.fetch_add (1)));

		std::error_code error_code;
		std::filesystem::remove_all (path, error_code);
		std::filesystem::create_directories (path, error_code);

		if (error_code) {
			throw std::runtime_error ("cannot create temporary directory '" + path.string () + "'");
		}

		return path;
	}

	std::filesystem::path path_;
};

inline void ensure_directory (const std::filesystem::path& path) {
	std::error_code error_code;
	std::filesystem::create_directories (path, error_code);

	if (error_code) {
		throw std::runtime_error ("cannot create directory '" + path.string () + "': " + error_code.message ());
	}
}

inline void write_text (const std::filesystem::path& path, std::string_view content) {
	if (!path.parent_path ().empty ()) {
		ensure_directory (path.parent_path ());
	}

	std::ofstream stream (path, std::ios::binary | std::ios::trunc);

	if (!stream) {
		throw std::runtime_error ("cannot open '" + path.string () + "' for writing");
	}

	stream.write (content.data (), static_cast<std::streamsize> (content.size ()));

	if (!stream) {
		throw std::runtime_error ("cannot write '" + path.string () + "'");
	}
}

inline void write_bytes (const std::filesystem::path& path, std::span<const std::byte> content) {
	if (!path.parent_path ().empty ()) {
		ensure_directory (path.parent_path ());
	}

	std::ofstream stream (path, std::ios::binary | std::ios::trunc);

	if (!stream) {
		throw std::runtime_error ("cannot open '" + path.string () + "' for writing");
	}

	stream.write (reinterpret_cast<const char*> (content.data ()), static_cast<std::streamsize> (content.size ()));

	if (!stream) {
		throw std::runtime_error ("cannot write '" + path.string () + "'");
	}
}

} // namespace we::test
