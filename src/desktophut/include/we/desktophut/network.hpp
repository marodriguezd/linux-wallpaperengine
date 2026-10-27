#pragma once

#include <chrono>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

#include "we/core/error.hpp"

namespace we::desktophut {

// Executes curl to fetch HTTP GET body as a string.
[[nodiscard]] std::expected<std::string, core::Error>
http_get (std::string_view url, std::chrono::seconds timeout = std::chrono::seconds (30));

// Executes curl to download file to a destination path safely (via temp file).
[[nodiscard]] std::expected<void, core::Error> http_download (
    std::string_view url, const std::filesystem::path& destination,
    std::chrono::seconds timeout = std::chrono::seconds (300)
);

} // namespace we::desktophut
