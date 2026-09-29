#include "Library.h"

#include "Steam/FileSystem/FileSystem.h"
#include "WallpaperEngine/Logging/Log.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>

using namespace WallpaperEngine::Library;
using nlohmann::json;

namespace {
std::filesystem::path baseDir () {
    const char* cacheHome = getenv ("XDG_CACHE_HOME");
    std::filesystem::path base;

    if (cacheHome != nullptr && cacheHome[0] != '\0') {
	base = cacheHome;
    } else {
	const char* home = getenv ("HOME");

	if (home == nullptr || home[0] == '\0') {
	    sLog.exception ("Cannot find home directory for the current user");
	}

	base = std::filesystem::path (home) / ".cache";
    }

    return base / "linux-wallpaperengine";
}

std::int64_t fileMtime (const std::filesystem::path& path) {
    std::error_code ec;
    const auto time = std::filesystem::last_write_time (path, ec);

    if (ec) {
	return 0;
    }

    // epoch seconds: stable across clocks, used for cache invalidation
    const auto sysTime = std::chrono::file_clock::to_sys (time);

    return static_cast<std::int64_t> (
	std::chrono::duration_cast<std::chrono::seconds> (sysTime.time_since_epoch ()).count ()
    );
}

std::string jsonString (const json& value, const std::string& key) {
    const auto it = value.find (key);

    if (it == value.end () || !it->is_string ()) {
	return "";
    }

    return it->get<std::string> ();
}
} // namespace

std::filesystem::path Library::cachePath () { return baseDir () / "library.json"; }

std::filesystem::path Library::thumbsDir () { return baseDir () / "thumbs"; }

std::filesystem::path Library::favoritesPath () {
    const char* configHome = getenv ("XDG_CONFIG_HOME");
    std::filesystem::path base;

    if (configHome != nullptr && configHome[0] != '\0') {
	base = configHome;
    } else {
	const char* home = getenv ("HOME");

	if (home == nullptr || home[0] == '\0') {
	    sLog.exception ("Cannot find home directory for the current user");
	}

	base = std::filesystem::path (home) / ".config";
    }

    return base / "we-wallpaper" / "favorites.json";
}

namespace {
// reads the sidecar favorite set, empty when missing/unreadable
std::set<std::string> readFavorites () {
    std::set<std::string> favorites;
    const auto path = Library::favoritesPath ();

    std::error_code ec;

    if (!std::filesystem::is_regular_file (path, ec)) {
	return favorites;
    }

    std::ifstream in (path);

    if (!in.is_open ()) {
	return favorites;
    }

    try {
	const json data = json::parse (in);

	if (data.is_object () && data.contains ("favorites") && data["favorites"].is_array ()) {
	    for (const auto& entry : data["favorites"]) {
		if (entry.is_string ()) {
		    favorites.insert (entry.get<std::string> ());
		}
	    }
	}
    } catch (const std::exception& e) {
	sLog.error ("Library: unreadable favorites at ", path.string (), ": ", e.what ());
    }

    return favorites;
}

bool writeFavorites (const std::set<std::string>& favorites) {
    const auto path = Library::favoritesPath ();

    std::error_code ec;
    std::filesystem::create_directories (path.parent_path (), ec);

    if (ec) {
	sLog.error ("Library: cannot create favorites directory ", path.parent_path ().string ());
	return false;
    }

    std::ofstream out (path);

    if (!out.is_open ()) {
	sLog.error ("Library: cannot write favorites at ", path.string ());
	return false;
    }

    json data = json::object ();
    data["favorites"] = json::array ();

    for (const auto& id : favorites) {
	data["favorites"].push_back (id);
    }

    out << data.dump ();
    return true;
}
} // namespace

std::string Library::toLower (std::string value) {
    std::transform (value.begin (), value.end (), value.begin (), [] (unsigned char c) { return std::tolower (c); });

    return value;
}

std::optional<LibraryItem> Library::parseProject (const std::filesystem::path& dir) {
    const auto projectPath = dir / "project.json";

    std::error_code ec;

    if (!std::filesystem::is_regular_file (projectPath, ec)) {
	return std::nullopt;
    }

    LibraryItem item;
    item.id = dir.filename ().string ();
    item.title = item.id;
    item.path = dir.string ();
    item.mtime = fileMtime (projectPath);

    std::ifstream file (projectPath);

    if (!file.is_open ()) {
	return item;
    }

    json data;

    try {
	data = json::parse (file);
    } catch (const std::exception& e) {
	sLog.error ("Library: skipping unparseable project.json in ", dir.string (), ": ", e.what ());
	return item;
    }

    if (!data.is_object ()) {
	return item;
    }

    item.title = jsonString (data, "title");
    item.file = jsonString (data, "file");
    item.preview = jsonString (data, "preview");
    item.description = jsonString (data, "description");
    item.type = toLower (jsonString (data, "type"));

    if (item.title.empty ()) {
	item.title = item.id;
    }

    if (item.type != "scene" && item.type != "video" && item.type != "web") {
	item.type = "unknown";
    }

    const auto workshopId = data.find ("workshopid");

    if (workshopId != data.end ()) {
	if (workshopId->is_number ()) {
	    item.id = std::to_string (workshopId->get<std::int64_t> ());
	} else if (workshopId->is_string () && !workshopId->get<std::string> ().empty ()) {
	    item.id = workshopId->get<std::string> ();
	}
    }

    const auto tags = data.find ("tags");

    if (tags != data.end () && tags->is_array ()) {
	for (const auto& tag : *tags) {
	    if (tag.is_string ()) {
		item.tags.push_back (tag.get<std::string> ());
	    }
	}
    }

    item.valid = item.type != "unknown" && !item.file.empty ();

    // ghosts out: a project.json pointing at a missing file must not list
    if (item.valid) {
	std::error_code existsEc;
	item.valid = std::filesystem::is_regular_file (dir / item.file, existsEc);
    }

    return item;
}

std::size_t Library::scan () {
    std::map<std::string, LibraryItem> merged;

    for (const auto& root : Steam::FileSystem::workshopRoots (WorkshopAppID)) {
	std::error_code ec;

	for (const auto& entry : std::filesystem::directory_iterator (root, ec)) {
	    if (ec) {
		break;
	    }

	    std::error_code dirEc;

	    if (!entry.is_directory (dirEc)) {
		continue;
	    }

	    auto item = parseProject (entry.path ());

	    if (!item.has_value ()) {
		continue;
	    }

	    // first root wins, same precedence as workshopDirectory()
	    merged.try_emplace (item->id, std::move (*item));
	}
    }

    this->m_items.clear ();
    this->m_items.reserve (merged.size ());

    const auto favorites = readFavorites ();

    for (auto& [id, item] : merged) {
	item.favorite = favorites.count (id) > 0;
	this->m_items.push_back (std::move (item));
    }

    std::sort (this->m_items.begin (), this->m_items.end (), [] (const LibraryItem& a, const LibraryItem& b) {
	return a.id < b.id;
    });

    return this->m_items.size ();
}

bool Library::save () const {
    const auto path = cachePath ();

    std::error_code ec;
    std::filesystem::create_directories (path.parent_path (), ec);

    if (ec) {
	sLog.error ("Library: cannot create cache directory ", path.parent_path ().string ());
	return false;
    }

    json items = json::array ();

    for (const auto& item : this->m_items) {
	items.push_back (
	    {
		{ "id", item.id },
		{ "title", item.title },
		{ "type", item.type },
		{ "file", item.file },
		{ "preview", item.preview },
		{ "path", item.path },
		{ "description", item.description },
		{ "tags", item.tags },
		{ "valid", item.valid },
		{ "favorite", item.favorite },
		{ "mtime", item.mtime },
	    }
	);
    }

    std::ofstream out (path);

    if (!out.is_open ()) {
	sLog.error ("Library: cannot write cache at ", path.string ());
	return false;
    }

    out << json ({ { "version", CacheVersion }, { "items", std::move (items) } }).dump ();
    return true;
}

bool Library::load () {
    const auto path = cachePath ();

    std::error_code ec;

    if (!std::filesystem::is_regular_file (path, ec)) {
	return false;
    }

    std::ifstream in (path);

    if (!in.is_open ()) {
	return false;
    }

    json data;

    try {
	data = json::parse (in);
    } catch (const std::exception& e) {
	sLog.error ("Library: unreadable cache at ", path.string (), ": ", e.what ());
	return false;
    }

    if (!data.is_object () || data.value ("version", 0) != CacheVersion || !data.contains ("items")
	|| !data["items"].is_array ()) {
	return false;
    }

    std::vector<LibraryItem> items;

    for (const auto& entry : data["items"]) {
	if (!entry.is_object ()) {
	    continue;
	}

	LibraryItem item;
	item.id = entry.value ("id", "");
	item.title = entry.value ("title", item.id);
	item.type = entry.value ("type", std::string ("unknown"));
	item.file = entry.value ("file", "");
	item.preview = entry.value ("preview", "");
	item.path = entry.value ("path", "");
	item.description = entry.value ("description", "");
	item.valid = entry.value ("valid", false);
	item.favorite = entry.value ("favorite", false);
	item.mtime = entry.value ("mtime", std::int64_t (0));

	const auto tags = entry.find ("tags");

	if (tags != entry.end () && tags->is_array ()) {
	    for (const auto& tag : *tags) {
		if (tag.is_string ()) {
		    item.tags.push_back (tag.get<std::string> ());
		}
	    }
	}

	if (item.id.empty ()) {
	    continue;
	}

	items.push_back (std::move (item));
    }

    this->m_items = std::move (items);

    // sidecar wins over whatever the cache file stored
    const auto favorites = readFavorites ();

    for (auto& item : this->m_items) {
	item.favorite = favorites.count (item.id) > 0;
    }

    return true;
}

std::vector<LibraryItem> Library::search (const std::string& query, const std::string& type) const {
    const std::string needle = toLower (query);
    const std::string typeFilter = toLower (type);
    std::vector<LibraryItem> results;

    for (const auto& item : this->m_items) {
	if (!typeFilter.empty () && toLower (item.type) != typeFilter) {
	    continue;
	}

	if (!needle.empty ()) {
	    std::string haystack = toLower (item.id + "\n" + item.title + "\n" + item.description);

	    for (const auto& tag : item.tags) {
		haystack += "\n" + toLower (tag);
	    }

	    if (haystack.find (needle) == std::string::npos) {
		continue;
	    }
	}

	results.push_back (item);
    }

    return results;
}

std::string Library::toJson (const std::vector<LibraryItem>& items) {
    json result = json::array ();

    for (const auto& item : items) {
	result.push_back (
	    {
		{ "id", item.id },
		{ "title", item.title },
		{ "type", item.type },
		{ "file", item.file },
		{ "preview", item.preview },
		{ "path", item.path },
		{ "description", item.description },
		{ "tags", item.tags },
		{ "valid", item.valid },
		{ "favorite", item.favorite },
	    }
	);
    }

    return result.dump ();
}

std::optional<bool> Library::toggleFavorite (const std::string& id) {
    bool known = false;

    for (const auto& item : this->m_items) {
	if (item.id == id) {
	    known = true;
	    break;
	}
    }

    if (!known) {
	return std::nullopt;
    }

    auto favorites = readFavorites ();
    const bool nowFavorite = favorites.count (id) == 0;

    if (nowFavorite) {
	favorites.insert (id);
    } else {
	favorites.erase (id);
    }

    if (!writeFavorites (favorites)) {
	return std::nullopt;
    }

    for (auto& item : this->m_items) {
	if (item.id == id) {
	    item.favorite = nowFavorite;
	}
    }

    return nowFavorite;
}
