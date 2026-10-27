#pragma once

#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "we/core/error.hpp"

namespace we::desktophut {

struct DesktopHutCard {
    std::string id; // slug (e.g. "neon-city-corvette-live-wallpaper-eeye")
    std::string title; // Wallpaper title
    std::string resolution; // e.g. "4096x2304", "4K", "HD"
    std::string thumbnail_url; // Preview image URL (webp/jpg/png)
    std::string preview_video_url; // Short MP4 preview video
    std::string page_url; // Full page URL
};

struct DesktopHutDetail {
    std::string id; // slug
    std::string title;
    std::string resolution;
    std::string video_url; // Full original MP4 download URL
    std::string thumbnail_url; // Preview image URL
    std::string page_url;
};

struct SearchOptions {
    std::string query;
    std::string category;
    int page = 1;
};

// Pure HTML parsing / URL helper functions (testable without network)
[[nodiscard]] std::string decode_html_entities (std::string_view text);
[[nodiscard]] std::string extract_slug (std::string_view input);
[[nodiscard]] std::string
build_search_url (const SearchOptions& options, std::string_view base_url = "https://www.desktophut.com");
[[nodiscard]] std::vector<DesktopHutCard>
parse_cards (std::string_view html, std::string_view base_url = "https://www.desktophut.com");
[[nodiscard]] std::expected<DesktopHutDetail, core::Error> parse_detail_page (
    std::string_view html, std::string_view fallback_slug = "", std::string_view base_url = "https://www.desktophut.com"
);

class DesktopHutClient {
public:
    explicit DesktopHutClient (std::string base_url = "https://www.desktophut.com");

    [[nodiscard]] const std::string& base_url () const noexcept { return this->base_url_; }

    [[nodiscard]] std::expected<std::vector<DesktopHutCard>, core::Error> search (const SearchOptions& options);

    [[nodiscard]] std::expected<DesktopHutDetail, core::Error> fetch_details (std::string_view slug_or_url);

    [[nodiscard]] std::expected<std::filesystem::path, core::Error> download_wallpaper (
	std::string_view slug_or_url, std::optional<std::string_view> custom_id = std::nullopt,
	const std::filesystem::path& destination_dir = {}
    );

private:
    std::string base_url_;
};

} // namespace we::desktophut
