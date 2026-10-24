#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "we/core/error.hpp"

namespace we::presets {

// A named playback profile. `fps` is the frame rate on AC power; the battery
// rate is derived by the engine, so presets stay simple.
struct Preset {
    std::string name;
    int fps = 30;
    std::string backend = "auto";
};

// Limits shared by CLI and GUI, so both reject the same values.
inline constexpr int min_fps = 1;
inline constexpr int max_fps = 1000;

[[nodiscard]] bool is_valid_fps (int fps) noexcept;

// Built-in presets, always available and never written to disk.
[[nodiscard]] std::vector<Preset> builtin_presets ();

// Default location: $XDG_CONFIG_HOME/we-wallpaper/presets.json, falling back
// to ~/.config/we-wallpaper/presets.json.
[[nodiscard]] std::filesystem::path default_path ();

// Loads user presets from `path`. A missing file yields an empty list. Entries
// with an invalid FPS or an empty name are rejected as malformed data.
[[nodiscard]] std::expected<std::vector<Preset>, core::Error> load (const std::filesystem::path& path);

// Writes `presets` to `path`, creating parent directories when needed.
[[nodiscard]] std::expected<void, core::Error> save (const std::filesystem::path& path,
                                                     const std::vector<Preset>& presets);

// Looks up a preset by name among `builtin` and `user`; user entries win.
[[nodiscard]] std::optional<Preset> find (const std::vector<Preset>& user, std::string_view name);

} // namespace we::presets
