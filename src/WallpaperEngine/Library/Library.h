#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace WallpaperEngine::Library {
/** Cache format version written to library.json */
constexpr int CacheVersion = 1;
/** Steam app ID of Wallpaper Engine workshop content */
constexpr int WorkshopAppID = 431960;

struct LibraryItem {
    /** Workshop item ID (workshopid field or directory name fallback) */
    std::string id;
    std::string title;
    /** scene|video|web|unknown, always lowercased */
    std::string type = "unknown";
    /** project.json "file" value (scene.json, clip.mp4, ...) */
    std::string file;
    /** project.json "preview" value (relative file name) */
    std::string preview;
    /** Absolute item directory */
    std::string path;
    std::string description;
    std::vector<std::string> tags;
    /** False when project.json is missing required type/file info */
    bool valid = false;
    /**
     * True when the item was known from a previous scan but its directory
     * is gone now (Steam unsubscribe, manual delete). The entry is kept
     * (title, favorite, ...) so the gallery can badge it instead of
     * silently dropping it. Always implies valid == false.
     */
    bool missing = false;
    /** User favorite (sidecar file, survives rescans) */
    bool favorite = false;
    /** project.json mtime, used for cache invalidation */
    std::int64_t mtime = 0;
};

class Library {
public:
    /** ~/.cache/linux-wallpaperengine/library.json (XDG_CACHE_HOME aware) */
    static std::filesystem::path cachePath ();
    /** ~/.cache/linux-wallpaperengine/thumbs (created on demand) */
    static std::filesystem::path thumbsDir ();
    /** ~/.config/we-wallpaper/favorites.json (XDG_CONFIG_HOME aware) */
    static std::filesystem::path favoritesPath ();

    /** Loads cache into items. False when missing, unreadable or stale version. */
    bool load ();
    /**
     * Scans every workshop root for the Wallpaper Engine app ID.
     * Duplicate IDs across roots resolve first-root-wins (same precedence
     * as workshopDirectory()). Returns the item count.
     */
    std::size_t scan ();
    /** Writes current items to the cache file. False on IO error. */
    bool save () const;

    /**
     * Flips the favorite flag of an item: updates the sidecar file and,
     * when the item is loaded, the in-memory entry too.
     * @return new state, std::nullopt when the id is unknown
     */
    std::optional<bool> toggleFavorite (const std::string& id);

    /**
     * Case-insensitive substring match over id/title/description/tags.
     * An empty query matches everything; type restricts to
     * scene|video|web (case-insensitive, empty means all).
     */
    [[nodiscard]] std::vector<LibraryItem> search (const std::string& query, const std::string& type) const;

    [[nodiscard]] const std::vector<LibraryItem>& items () const { return this->m_items; }

    /** JSON array of the given items, for --json output */
    static std::string toJson (const std::vector<LibraryItem>& items);

private:
    /** Parses one workshop directory. std::nullopt when it holds no project.json. */
    static std::optional<LibraryItem> parseProject (const std::filesystem::path& dir);
    static std::string toLower (std::string value);

    std::vector<LibraryItem> m_items;
};
} // namespace WallpaperEngine::Library
