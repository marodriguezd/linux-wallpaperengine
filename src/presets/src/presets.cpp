#include "we/presets/presets.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string_view>
#include <system_error>

#include <nlohmann/json.hpp>

namespace we::presets {

namespace {

constexpr std::string_view k_app_dir = "we-wallpaper";
constexpr std::string_view k_file_name = "presets.json";

}  // namespace

bool is_valid_fps (int fps) noexcept {
    return fps >= min_fps && fps <= max_fps;
}

std::vector<Preset> builtin_presets () {
    return {
        Preset {.name = "battery", .fps = 15, .backend = "auto"},
        Preset {.name = "balanced", .fps = 30, .backend = "auto"},
        Preset {.name = "smooth", .fps = 60, .backend = "auto"},
    };
}

std::filesystem::path default_path () {
    if (const char* xdg = std::getenv ("XDG_CONFIG_HOME"); xdg != nullptr && *xdg != '\0') {
        return std::filesystem::path (xdg) / k_app_dir / k_file_name;
    }
    if (const char* home = std::getenv ("HOME"); home != nullptr && *home != '\0') {
        return std::filesystem::path (home) / ".config" / k_app_dir / k_file_name;
    }
    return std::filesystem::path (k_file_name);
}

std::expected<std::vector<Preset>, core::Error> load (const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::exists (path, ec)) {
        return std::vector<Preset> {};
    }

    std::ifstream in (path);
    if (!in) {
        return core::fail (core::ErrorCode::io, "cannot open presets file: " + path.string ());
    }

    std::stringstream buffer;
    buffer << in.rdbuf ();

    nlohmann::json doc = nlohmann::json::parse (buffer.str (), nullptr, false);
    if (doc.is_discarded () || !doc.is_object () || !doc.contains ("presets") || !doc["presets"].is_array ()) {
        return core::fail (core::ErrorCode::malformed_data, "presets file must contain a \"presets\" array: " + path.string ());
    }

    std::vector<Preset> result;
    for (const auto& entry : doc["presets"]) {
        if (!entry.is_object () || !entry.contains ("name") || !entry["name"].is_string ()) {
            return core::fail (core::ErrorCode::malformed_data, "preset entry without a string name");
        }

        Preset preset;
        preset.name = entry["name"].get<std::string> ();
        if (preset.name.empty ()) {
            return core::fail (core::ErrorCode::malformed_data, "preset name must not be empty");
        }

        if (!entry.contains ("fps") || !entry["fps"].is_number_integer ()) {
            return core::fail (core::ErrorCode::malformed_data, "preset '" + preset.name + "' needs an integer fps");
        }
        preset.fps = entry["fps"].get<int> ();
        if (!is_valid_fps (preset.fps)) {
            return core::fail (core::ErrorCode::out_of_range,
                               "preset '" + preset.name + "' has fps outside " + std::to_string (min_fps) + ".." + std::to_string (max_fps));
        }

        if (entry.contains ("backend") && entry["backend"].is_string ()) {
            preset.backend = entry["backend"].get<std::string> ();
        }
        result.push_back (std::move (preset));
    }
    return result;
}

std::expected<void, core::Error> save (const std::filesystem::path& path, const std::vector<Preset>& presets) {
    for (const auto& preset : presets) {
        if (preset.name.empty ()) {
            return core::fail (core::ErrorCode::invalid_argument, "preset name must not be empty");
        }
        if (!is_valid_fps (preset.fps)) {
            return core::fail (core::ErrorCode::out_of_range, "preset '" + preset.name + "' has an invalid fps");
        }
    }

    nlohmann::json doc;
    doc["presets"] = nlohmann::json::array ();
    for (const auto& preset : presets) {
        doc["presets"].push_back ({{"name", preset.name}, {"fps", preset.fps}, {"backend", preset.backend}});
    }

    std::error_code ec;
    if (path.has_parent_path ()) {
        std::filesystem::create_directories (path.parent_path (), ec);
        if (ec) {
            return core::fail (core::ErrorCode::io, "cannot create " + path.parent_path ().string () + ": " + ec.message ());
        }
    }

    std::ofstream out (path, std::ios::trunc);
    if (!out) {
        return core::fail (core::ErrorCode::io, "cannot write presets file: " + path.string ());
    }
    out << doc.dump (2) << '\n';
    if (!out) {
        return core::fail (core::ErrorCode::io, "failed while writing presets file: " + path.string ());
    }
    return {};
}

std::optional<Preset> find (const std::vector<Preset>& user, std::string_view name) {
    for (const auto& preset : user) {
        if (preset.name == name) {
            return preset;
        }
    }
    for (const auto& preset : builtin_presets ()) {
        if (preset.name == name) {
            return preset;
        }
    }
    return std::nullopt;
}

}  // namespace we::presets
