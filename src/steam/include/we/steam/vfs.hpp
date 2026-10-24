#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace we::steam {

enum class VfsErrorCode : std::uint8_t {
	not_found,
	not_a_file,
	not_a_directory,
	invalid_path,
	io,
	out_of_range,
};

[[nodiscard]] std::string_view to_string (VfsErrorCode code) noexcept;

struct VfsError {
	VfsErrorCode code = VfsErrorCode::not_found;
	std::string message;
};

struct VfsEntry {
	std::string name;
	bool directory = false;
	std::uint64_t size = 0;
};

// Virtual filesystem over a prioritized list of roots.
//
// Wallpaper Engine content uses Windows semantics: paths are case-insensitive,
// separators are `\`, and absolute paths carry a drive letter. This class
// normalizes those paths, resolves them component by component against cached
// directory indexes and, for drive-letter paths, falls back to the longest
// matching suffix under any root.
//
// Not thread-safe: the directory cache is mutable. Use one instance per thread
// or synchronize externally.
class CaseInsensitiveVfs {
public:
	CaseInsensitiveVfs () = default;
	explicit CaseInsensitiveVfs (std::vector<std::filesystem::path> roots);

	void add_root (std::filesystem::path root);

	[[nodiscard]] std::span<const std::filesystem::path> roots () const noexcept { return this->roots_; }

	// Converts a logical path into a physical one. Drive letters and leading
	// separators are stripped, `.`/`..` are resolved lexically and `..` can
	// never escape the root.
	[[nodiscard]] std::expected<std::filesystem::path, VfsError> resolve (std::string_view path) const;

	[[nodiscard]] bool exists (std::string_view path) const;

	[[nodiscard]] std::expected<std::vector<VfsEntry>, VfsError> list (std::string_view path) const;

	[[nodiscard]] std::expected<std::vector<std::byte>, VfsError> read (std::string_view path) const;

	[[nodiscard]] std::expected<std::string, VfsError> read_text (std::string_view path) const;

	// Drops cached directory listings. Call after installing new content.
	void clear_cache ();

	// Exposed for diagnostics and tests.
	[[nodiscard]] static std::string normalize (std::string_view path);

private:
	struct DirectoryIndex;

	struct ParsedPath {
		std::string normalized;
		std::vector<std::string> components;
		bool windowish = false;
	};

	[[nodiscard]] static ParsedPath parse (std::string_view path);

	[[nodiscard]] const DirectoryIndex* index_for (const std::filesystem::path& directory) const;

	[[nodiscard]] std::optional<std::filesystem::path> resolve_from (
		const std::filesystem::path& root, std::span<const std::string> components) const;

	std::vector<std::filesystem::path> roots_;
	mutable std::unordered_map<std::string, std::shared_ptr<const DirectoryIndex>> cache_;
};

} // namespace we::steam
