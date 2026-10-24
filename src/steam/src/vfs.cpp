#include "we/steam/vfs.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <ranges>
#include <system_error>
#include <unordered_set>
#include <utility>

#include "we/core/text.hpp"

namespace we::steam {
namespace {

constexpr std::uintmax_t maximum_read_size = 2ULL * 1024ULL * 1024ULL * 1024ULL;

[[nodiscard]] VfsError vfs_error (VfsErrorCode code, std::string message) {
	return VfsError {.code = code, .message = std::move (message)};
}

} // namespace

struct CaseInsensitiveVfs::DirectoryIndex {
	std::unordered_set<std::string> names;
	std::unordered_map<std::string, std::string> by_lower;

	[[nodiscard]] std::optional<std::string> lookup (std::string_view component) const {
		// Exact spelling wins when a directory contains both `Foo` and `foo`.
		if (const auto exact = this->names.find (std::string (component)); exact != this->names.end ()) {
			return *exact;
		}

		const auto folded = this->by_lower.find (core::to_lower_ascii (component));

		if (folded == this->by_lower.end ()) {
			return std::nullopt;
		}

		return folded->second;
	}
};

std::string_view to_string (VfsErrorCode code) noexcept {
	switch (code) {
		case VfsErrorCode::not_found:
			return "not_found";
		case VfsErrorCode::not_a_file:
			return "not_a_file";
		case VfsErrorCode::not_a_directory:
			return "not_a_directory";
		case VfsErrorCode::invalid_path:
			return "invalid_path";
		case VfsErrorCode::io:
			return "io";
		case VfsErrorCode::out_of_range:
			return "out_of_range";
	}

	return "unknown";
}

CaseInsensitiveVfs::CaseInsensitiveVfs (std::vector<std::filesystem::path> roots) : roots_ (std::move (roots)) { }

void CaseInsensitiveVfs::add_root (std::filesystem::path root) { this->roots_.push_back (std::move (root)); }

void CaseInsensitiveVfs::clear_cache () { this->cache_.clear (); }

CaseInsensitiveVfs::ParsedPath CaseInsensitiveVfs::parse (std::string_view path) {
	ParsedPath result;

	std::size_t position = 0;

	if (path.size () >= 2 && std::isalpha (static_cast<unsigned char> (path[0])) && path[1] == ':') {
		result.windowish = true;
		position = 2;
	}

	std::string current;

	const auto flush = [&result, &current] {
		if (current.empty () || current == ".") {
			current.clear ();
			return;
		}

		if (current == "..") {
			if (!result.components.empty ()) {
				result.components.pop_back ();
			}

			current.clear ();
			return;
		}

		result.components.push_back (std::move (current));
		current.clear ();
	};

	for (; position < path.size (); ++position) {
		const char value = path[position];

		if (value == '/' || value == '\\') {
			flush ();
			continue;
		}

		current.push_back (value);
	}

	flush ();

	for (const auto& component : result.components) {
		if (!result.normalized.empty ()) {
			result.normalized.push_back ('/');
		}

		result.normalized += component;
	}

	return result;
}

std::string CaseInsensitiveVfs::normalize (std::string_view path) { return parse (path).normalized; }

const CaseInsensitiveVfs::DirectoryIndex* CaseInsensitiveVfs::index_for (
	const std::filesystem::path& directory) const {
	const std::string key = directory.string ();

	if (const auto cached = this->cache_.find (key); cached != this->cache_.end ()) {
		return cached->second.get ();
	}

	auto index = std::make_shared<DirectoryIndex> ();
	std::error_code error_code;

	std::filesystem::directory_iterator iterator (
		directory, std::filesystem::directory_options::skip_permission_denied, error_code);

	if (!error_code) {
		const std::filesystem::directory_iterator end;

		while (iterator != end) {
			std::string name = iterator->path ().filename ().string ();

			if (!name.empty ()) {
				index->by_lower.emplace (core::to_lower_ascii (name), name);
				index->names.insert (std::move (name));
			}

			iterator.increment (error_code);

			if (error_code) {
				break;
			}
		}
	}

	const auto position = this->cache_.emplace (key, std::move (index)).first;
	return position->second.get ();
}

std::optional<std::filesystem::path> CaseInsensitiveVfs::resolve_from (
	const std::filesystem::path& root, std::span<const std::string> components) const {
	std::filesystem::path current = root;

	for (std::size_t index = 0; index < components.size (); ++index) {
		const DirectoryIndex* directory = this->index_for (current);

		if (directory == nullptr) {
			return std::nullopt;
		}

		const auto match = directory->lookup (components[index]);

		if (!match) {
			return std::nullopt;
		}

		current /= *match;

		if (index + 1 < components.size ()) {
			std::error_code error_code;

			if (!std::filesystem::is_directory (current, error_code)) {
				return std::nullopt;
			}
		}
	}

	return current;
}

std::expected<std::filesystem::path, VfsError> CaseInsensitiveVfs::resolve (std::string_view path) const {
	const ParsedPath parsed = parse (path);

	if (parsed.components.empty ()) {
		return std::unexpected (vfs_error (VfsErrorCode::invalid_path, "empty path"));
	}

	for (const auto& root : this->roots_) {
		if (auto resolved = this->resolve_from (root, parsed.components)) {
			return std::move (*resolved);
		}
	}

	// Absolute Wallpaper Engine paths look like `C:\Program Files (x86)\...`.
	// Strip leading components until the remainder exists under a known root.
	if (parsed.windowish) {
		const std::span<const std::string> all (parsed.components);

		for (std::size_t skipped = 1; skipped + 1 < parsed.components.size (); ++skipped) {
			const auto suffix = all.subspan (skipped);

			for (const auto& root : this->roots_) {
				if (auto resolved = this->resolve_from (root, suffix)) {
					return std::move (*resolved);
				}
			}
		}
	}

	return std::unexpected (
		vfs_error (VfsErrorCode::not_found, "cannot resolve '" + std::string (path) + "'"));
}

bool CaseInsensitiveVfs::exists (std::string_view path) const { return this->resolve (path).has_value (); }

std::expected<std::vector<VfsEntry>, VfsError> CaseInsensitiveVfs::list (std::string_view path) const {
	auto resolved = this->resolve (path);

	if (!resolved) {
		return std::unexpected (std::move (resolved.error ()));
	}

	std::error_code error_code;

	if (!std::filesystem::is_directory (*resolved, error_code)) {
		return std::unexpected (
			vfs_error (VfsErrorCode::not_a_directory, "'" + std::string (path) + "' is not a directory"));
	}

	std::vector<VfsEntry> entries;

	std::filesystem::directory_iterator iterator (
		*resolved, std::filesystem::directory_options::skip_permission_denied, error_code);

	if (error_code) {
		return std::unexpected (vfs_error (
			VfsErrorCode::io, "cannot list '" + resolved->string () + "': " + error_code.message ()));
	}

	const std::filesystem::directory_iterator end;

	while (iterator != end) {
		VfsEntry entry;
		entry.name = iterator->path ().filename ().string ();
		entry.directory = iterator->is_directory (error_code);

		if (!entry.directory && iterator->is_regular_file (error_code)) {
			entry.size = iterator->file_size (error_code);
		}

		entries.push_back (std::move (entry));
		iterator.increment (error_code);

		if (error_code) {
			break;
		}
	}

	std::ranges::sort (entries, {}, &VfsEntry::name);

	return entries;
}

std::expected<std::vector<std::byte>, VfsError> CaseInsensitiveVfs::read (std::string_view path) const {
	auto resolved = this->resolve (path);

	if (!resolved) {
		return std::unexpected (std::move (resolved.error ()));
	}

	std::error_code error_code;

	if (!std::filesystem::is_regular_file (*resolved, error_code)) {
		return std::unexpected (
			vfs_error (VfsErrorCode::not_a_file, "'" + std::string (path) + "' is not a regular file"));
	}

	const std::uintmax_t size = std::filesystem::file_size (*resolved, error_code);

	if (error_code) {
		return std::unexpected (vfs_error (
			VfsErrorCode::io, "cannot stat '" + resolved->string () + "': " + error_code.message ()));
	}

	if (size > maximum_read_size) {
		return std::unexpected (vfs_error (
			VfsErrorCode::out_of_range, "'" + resolved->string () + "' is too large to read into memory"));
	}

	std::ifstream stream (*resolved, std::ios::binary);

	if (!stream) {
		return std::unexpected (vfs_error (VfsErrorCode::io, "cannot open '" + resolved->string () + "'"));
	}

	std::vector<std::byte> data (static_cast<std::size_t> (size));

	if (!data.empty ()) {
		stream.read (reinterpret_cast<char*> (data.data ()), static_cast<std::streamsize> (data.size ()));

		if (!stream || static_cast<std::size_t> (stream.gcount ()) != data.size ()) {
			return std::unexpected (vfs_error (VfsErrorCode::io, "cannot read '" + resolved->string () + "'"));
		}
	}

	return data;
}

std::expected<std::string, VfsError> CaseInsensitiveVfs::read_text (std::string_view path) const {
	auto data = this->read (path);

	if (!data) {
		return std::unexpected (std::move (data.error ()));
	}

	if (data->empty ()) {
		return std::string {};
	}

	return std::string (reinterpret_cast<const char*> (data->data ()), data->size ());
}

} // namespace we::steam
