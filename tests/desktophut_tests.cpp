#include "support/harness.hpp"
#include "support/temp_dir.hpp"
#include "we/desktophut/client.hpp"
#include "we/desktophut/repository.hpp"
#include "we/steam/steam_locator.hpp"

#include <filesystem>
#include <string>

using we::desktophut::build_search_url;
using we::desktophut::decode_html_entities;
using we::desktophut::delete_desktophut_wallpaper;
using we::desktophut::delete_wallpaper;
using we::desktophut::DesktopHutCard;
using we::desktophut::DesktopHutDetail;
using we::desktophut::extract_slug;
using we::desktophut::is_steam_wallpaper;
using we::desktophut::list_desktophut_wallpapers;
using we::desktophut::list_local_wallpapers;
using we::desktophut::parse_cards;
using we::desktophut::parse_detail_page;
using we::desktophut::resolve_wallpaper;
using we::desktophut::SearchOptions;
using we::desktophut::steam_deletion_refusal_message;
using we::steam::SteamDiscovery;
using we::steam::SteamInstallKind;
using we::steam::SteamLibrary;

TEST (desktophut_extract_slug_and_urls) {
    // Full URLs
    CHECK_EQ (
	extract_slug ("https://www.desktophut.com/neon-city-corvette-live-wallpaper-eeye"),
	"neon-city-corvette-live-wallpaper-eeye"
    );
    CHECK_EQ (
	extract_slug ("https://www.desktophut.com/neon-city-corvette-live-wallpaper-eeye?page=2#preview"),
	"neon-city-corvette-live-wallpaper-eeye"
    );

    // Relative paths
    CHECK_EQ (extract_slug ("/neon-city-corvette-live-wallpaper-eeye"), "neon-city-corvette-live-wallpaper-eeye");
    CHECK_EQ (extract_slug ("neon-city-corvette-live-wallpaper-eeye/"), "neon-city-corvette-live-wallpaper-eeye");

    // With desktophut- prefix
    CHECK_EQ (
	extract_slug ("desktophut-neon-city-corvette-live-wallpaper-eeye"), "neon-city-corvette-live-wallpaper-eeye"
    );

    // Search URL generation
    CHECK_EQ (
	build_search_url (SearchOptions { .query = "cyberpunk city", .category = {}, .page = 1 }),
	"https://www.desktophut.com/search/cyberpunk+city"
    );
    CHECK_EQ (
	build_search_url (SearchOptions { .query = "anime", .category = {}, .page = 3 }),
	"https://www.desktophut.com/search/anime?page=3"
    );
    CHECK_EQ (
	build_search_url (SearchOptions { .query = {}, .category = "anime", .page = 2 }),
	"https://www.desktophut.com/category/anime-live-wallpapers?page=2"
    );
    CHECK_EQ (
	build_search_url (SearchOptions { .query = {}, .category = {}, .page = 2 }),
	"https://www.desktophut.com/?page=2"
    );

    // Entity decoding
    CHECK_EQ (
	decode_html_entities ("Gojo &amp; Geto &quot;Special&quot; &#039;Battle&#039;"),
	"Gojo & Geto \"Special\" 'Battle'"
    );
}

TEST (desktophut_parse_cards) {
    constexpr std::string_view sample_html = R"html(
<!DOCTYPE html>
<html>
<body>
    <div class="grid">
        <!-- Wallpaper Card 1 -->
        <a href="/neon-city-corvette-live-wallpaper-eeye" class="wallpaper-card" title="Neon City Corvette Live Wallpaper — animated wallpaper">
            <img src="https://www.desktophut.com/images/corvette.webp" width="480" height="270" alt="Corvette preview">
            <video class="card-preview-video" muted loop data-src="https://www.desktophut.com/previews/corvette-preview.mp4"></video>
            <div class="card-badges">
                <div><span class="badge-res">4096x2304</span></div>
            </div>
            <div class="card-overlay">
                <h3 class="card-title">Neon City Corvette Live Wallpaper</h3>
            </div>
        </a>

        <!-- Wallpaper Card 2 -->
        <a href="/cyber-samurai-szta" class="wallpaper-card" title="Cyber Samurai &amp; Dragon – Free Live Wallpaper">
            <img src="/images/samurai.webp" width="480" height="270" alt="Samurai">
            <video class="card-preview-video" muted loop data-src="/previews/samurai-preview.mp4"></video>
            <div class="card-badges">
                <div><span class="badge-res">1920x1080</span></div>
            </div>
            <div class="card-overlay">
                <h3 class="card-title">Cyber Samurai &amp; Dragon</h3>
            </div>
        </a>
    </div>
</body>
</html>
)html";

    const auto cards = parse_cards (sample_html);
    CHECK_EQ (cards.size (), std::size_t { 2 });

    // Card 1
    CHECK_EQ (cards[0].id, "neon-city-corvette-live-wallpaper-eeye");
    CHECK_EQ (cards[0].title, "Neon City Corvette Live Wallpaper");
    CHECK_EQ (cards[0].resolution, "4096x2304");
    CHECK_EQ (cards[0].thumbnail_url, "https://www.desktophut.com/images/corvette.webp");
    CHECK_EQ (cards[0].preview_video_url, "https://www.desktophut.com/previews/corvette-preview.mp4");
    CHECK_EQ (cards[0].page_url, "https://www.desktophut.com/neon-city-corvette-live-wallpaper-eeye");

    // Card 2
    CHECK_EQ (cards[1].id, "cyber-samurai-szta");
    CHECK_EQ (cards[1].title, "Cyber Samurai & Dragon");
    CHECK_EQ (cards[1].resolution, "1920x1080");
    CHECK_EQ (cards[1].thumbnail_url, "https://www.desktophut.com/images/samurai.webp");
    CHECK_EQ (cards[1].preview_video_url, "https://www.desktophut.com/previews/samurai-preview.mp4");
    CHECK_EQ (cards[1].page_url, "https://www.desktophut.com/cyber-samurai-szta");
}

TEST (desktophut_parse_detail_page) {
    constexpr std::string_view detail_html = R"html(
<!DOCTYPE html>
<html>
<head>
    <meta property="og:title" content="Neon City Corvette Live Wallpaper – Free Animated Desktop Background">
    <meta property="og:image" content="https://www.desktophut.com/images/corvette.webp">
</head>
<body>
    <h1 style="color:#fff;">Neon City Corvette Live Wallpaper</h1>
    <a href="https://www.desktophut.com/files/corvette-full.mp4" id="primaryDownloadBtn" download>Download Original</a>
</body>
</html>
)html";

    const auto detail = parse_detail_page (detail_html, "neon-city-corvette");
    CHECK_TRUE (detail.has_value ());
    CHECK_EQ (detail->title, "Neon City Corvette Live Wallpaper");
    CHECK_EQ (detail->video_url, "https://www.desktophut.com/files/corvette-full.mp4");
    CHECK_EQ (detail->thumbnail_url, "https://www.desktophut.com/images/corvette.webp");
}

TEST (desktophut_parse_detail_jsonld_fallback) {
    constexpr std::string_view jsonld_html = R"html(
<!DOCTYPE html>
<html>
<head>
    <script type="application/ld+json">
    {
        "@type": "VideoObject",
        "name": "Neon Sunset",
        "contentUrl": "https:\/\/www.desktophut.com\/files\/sunset.mp4"
    }
    </script>
    <meta property="og:image" content="https://www.desktophut.com/images/sunset.webp">
</head>
<body>
    <h1>Neon Sunset Live Wallpaper</h1>
</body>
</html>
)html";

    const auto detail = parse_detail_page (jsonld_html, "neon-sunset");
    CHECK_TRUE (detail.has_value ());
    CHECK_EQ (detail->title, "Neon Sunset Live Wallpaper");
    CHECK_EQ (detail->video_url, "https://www.desktophut.com/files/sunset.mp4");
    CHECK_EQ (detail->thumbnail_url, "https://www.desktophut.com/images/sunset.webp");
}

TEST (desktophut_parse_detail_missing_video_fails) {
    constexpr std::string_view no_video_html = R"html(
<!DOCTYPE html>
<html>
<body>
    <h1>No Video Here</h1>
</body>
</html>
)html";

    const auto detail = parse_detail_page (no_video_html, "no-video");
    CHECK_TRUE (!detail.has_value ());
}

TEST (local_wallpaper_repository_listing) {
    we::test::TempDir temp;
    const auto root = temp.path () / "wallpapers";
    const auto dh_dir = root / "desktophut";

    // Wallpaper 1: desktophut item
    we::test::ensure_directory (dh_dir / "cyber-car");
    we::test::write_text (
	dh_dir / "cyber-car" / "project.json",
	R"({ "title": "Cyber Car", "type": "video", "file": "wallpaper.mp4", "preview": "preview.jpg" })"
    );

    // Wallpaper 2: another desktophut item
    we::test::ensure_directory (dh_dir / "neon-city");
    we::test::write_text (
	dh_dir / "neon-city" / "project.json",
	R"({ "title": "Neon City", "type": "video", "file": "wallpaper.mp4", "preview": "preview.jpg" })"
    );

    // Test list_desktophut_wallpapers
    const auto dh_items = list_desktophut_wallpapers (dh_dir);
    CHECK_EQ (dh_items.size (), std::size_t { 2 });
    CHECK_EQ (dh_items[0].title, "Cyber Car");
    CHECK_EQ (dh_items[0].source, "desktophut");
    CHECK_EQ (dh_items[0].type, "video");
    CHECK_EQ (dh_items[1].title, "Neon City");

    // Test list_local_wallpapers
    const auto local_items = list_local_wallpapers (root);
    CHECK_EQ (local_items.size (), std::size_t { 2 });
}

TEST (wallpaper_engine_safety_guard_blocks_steam_deletion) {
    we::test::TempDir temp;
    const auto steam_root = temp.path () / "Steam";
    const auto steamapps = steam_root / "steamapps";
    const auto workshop_dir = steamapps / "workshop" / "content" / "431960";
    const auto item_dir = workshop_dir / "1234567";

    we::test::ensure_directory (item_dir);
    we::test::write_text (
	item_dir / "project.json",
	R"({ "title": "Steam Wallpaper", "type": "scene", "file": "scene.pkg", "workshopid": "1234567" })"
    );

    SteamLibrary library;
    library.root = steam_root;
    library.steamapps = steamapps;
    library.workshop_content_dir = workshop_dir;
    library.kind = SteamInstallKind::native;

    SteamDiscovery discovery;
    discovery.libraries.push_back (library);

    // 1. is_steam_wallpaper check
    CHECK_TRUE (is_steam_wallpaper ("1234567", discovery));
    CHECK_TRUE (is_steam_wallpaper (item_dir.string (), discovery));
    CHECK_TRUE (is_steam_wallpaper ("431960", discovery));

    // 2. delete_wallpaper MUST fail with the exact Spanish message
    const auto result_id = delete_wallpaper ("1234567", discovery);
    CHECK_TRUE (!result_id.has_value ());
    CHECK_EQ (result_id.error ().message, std::string (steam_deletion_refusal_message));

    const auto result_path = delete_wallpaper (item_dir.string (), discovery);
    CHECK_TRUE (!result_path.has_value ());
    CHECK_EQ (result_path.error ().message, std::string (steam_deletion_refusal_message));

    // 3. delete_desktophut_wallpaper on steam item MUST also fail
    const auto result_dh = delete_desktophut_wallpaper ("1234567", discovery);
    CHECK_TRUE (!result_dh.has_value ());
    CHECK_EQ (result_dh.error ().message, std::string (steam_deletion_refusal_message));

    // 4. Verify directory on disk was PROTECTED and STILL EXISTS!
    std::error_code ec;
    CHECK_TRUE (std::filesystem::exists (item_dir, ec));
}

TEST (local_wallpaper_deletion_succeeds) {
    we::test::TempDir temp;
    const auto local_dir = temp.path () / "desktophut" / "temp-wallpaper";
    we::test::ensure_directory (local_dir);
    we::test::write_text (
	local_dir / "project.json", R"({ "title": "Temporary Wallpaper", "type": "video", "file": "wallpaper.mp4" })"
    );
    we::test::write_text (local_dir / "wallpaper.mp4", "fake video content");

    SteamDiscovery discovery;

    // Verify exists before delete
    std::error_code ec;
    CHECK_TRUE (std::filesystem::exists (local_dir, ec));

    // Delete by path
    const auto result = delete_wallpaper (local_dir.string (), discovery);
    CHECK_TRUE (result.has_value ());

    // Verify deleted from disk
    CHECK_TRUE (!std::filesystem::exists (local_dir, ec));

    // Deleting again fails with not_found
    const auto second_attempt = delete_wallpaper (local_dir.string (), discovery);
    CHECK_TRUE (!second_attempt.has_value ());
}
