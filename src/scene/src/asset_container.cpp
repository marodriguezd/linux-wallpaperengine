#include "we/scene/asset_container.hpp"

#include <fstream>

namespace we::scene {

std::expected<AssetContainer, core::Error> AssetContainer::open (const std::filesystem::path& location) {
    std::error_code error;
    const auto status = std::filesystem::status (location, error);

    if (error || !std::filesystem::exists (status)) {
        return core::fail (core::ErrorCode::not_found, "no such wallpaper location: " + location.string ());
    }

    AssetContainer container;

    if (std::filesystem::is_regular_file (status)) {
        if (location.extension () != ".pkg") {
            return core::fail (
                core::ErrorCode::invalid_argument, "expected a wallpaper directory or a .pkg file: " + location.string ()
            );
        }

        auto package = steam::PackageArchive::open (location);

        if (!package) {
            return std::unexpected (std::move (package.error ()));
        }

        container.root_ = location.parent_path ();
        container.package_ = std::move (*package);
        container.vfs_.add_root (container.root_);
        return container;
    }

    if (!std::filesystem::is_directory (status)) {
        return core::fail (core::ErrorCode::invalid_argument, "wallpaper location is not a directory: " + location.string ());
    }

    container.root_ = std::filesystem::absolute (location);
    container.vfs_.add_root (container.root_);

    const auto package_path = container.root_ / "scene.pkg";

    if (std::filesystem::is_regular_file (package_path, error)) {
        auto package = steam::PackageArchive::open (package_path);

        if (!package) {
            return std::unexpected (std::move (package.error ()));
        }

        container.package_ = std::move (*package);
    }

    return container;
}

void AssetContainer::add_root (std::filesystem::path root) {
    if (root.empty ()) {
        return;
    }

    if (!this->root_.empty () && this->root_ == root) {
        return;
    }

    this->vfs_.add_root (std::move (root));
}

bool AssetContainer::exists (std::string_view path) const {
    if (this->package_.has_value () && this->package_->contains (path)) {
        return true;
    }

    return this->vfs_.exists (path);
}

bool AssetContainer::is_in_package (std::string_view path) const {
    return this->package_.has_value () && this->package_->contains (path);
}

std::expected<std::vector<std::byte>, core::Error> AssetContainer::read (std::string_view path) const {
    if (this->package_.has_value ()) {
        if (const auto* entry = this->package_->find (path); entry != nullptr) {
            const auto view = this->package_->view (entry->name);

            if (!view) {
                return std::unexpected (std::move (view.error ()));
            }

            return std::vector<std::byte> (view->begin (), view->end ());
        }
    }

    auto data = this->vfs_.read (path);

    if (!data) {
        return core::fail (
            data.error ().code == steam::VfsErrorCode::not_found ? core::ErrorCode::not_found : core::ErrorCode::io,
            data.error ().message
        );
    }

    return std::move (*data);
}

std::expected<std::string, core::Error> AssetContainer::read_text (std::string_view path) const {
    auto bytes = this->read (path);

    if (!bytes) {
        return std::unexpected (std::move (bytes.error ()));
    }

    return std::string (reinterpret_cast<const char*> (bytes->data ()), bytes->size ());
}

} // namespace we::scene
