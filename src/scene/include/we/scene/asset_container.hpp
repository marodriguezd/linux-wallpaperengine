#pragma once

#include <cstddef>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "we/core/error.hpp"
#include "we/steam/package_archive.hpp"
#include "we/steam/vfs.hpp"

namespace we::scene {

// Reads a Wallpaper Engine project's files. Content stored inside scene.pkg is
// served zero-copy from the archive; extracted files are resolved through the
// case-insensitive VFS so Windows paths keep working.
class AssetContainer {
public:
    AssetContainer () = default;

    AssetContainer (const AssetContainer&) = delete;
    AssetContainer& operator= (const AssetContainer&) = delete;
    AssetContainer (AssetContainer&&) noexcept = default;
    AssetContainer& operator= (AssetContainer&&) noexcept = default;

    // Accepts a project directory, a `.pkg` file, or a directory containing
    // `scene.pkg` plus `project.json`.
    [[nodiscard]] static std::expected<AssetContainer, core::Error> open (const std::filesystem::path& location);

    void add_root (std::filesystem::path root);

    [[nodiscard]] bool exists (std::string_view path) const;
    [[nodiscard]] bool is_in_package (std::string_view path) const;

    [[nodiscard]] std::expected<std::vector<std::byte>, core::Error> read (std::string_view path) const;
    [[nodiscard]] std::expected<std::string, core::Error> read_text (std::string_view path) const;

    [[nodiscard]] const std::filesystem::path& root () const noexcept { return this->root_; }
    [[nodiscard]] const std::optional<steam::PackageArchive>& package () const noexcept { return this->package_; }

private:
    std::filesystem::path root_;
    std::optional<steam::PackageArchive> package_;
    steam::CaseInsensitiveVfs vfs_;
};

} // namespace we::scene
