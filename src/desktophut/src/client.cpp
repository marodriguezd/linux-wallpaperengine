#include "we/desktophut/client.hpp"

#include "we/core/log.hpp"
#include "we/desktophut/network.hpp"
#include "we/desktophut/repository.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <format>
#include <fstream>
#include <regex>

namespace we::desktophut {
namespace {

    std::string trim (std::string_view text) {
	while (!text.empty () && std::isspace (static_cast<unsigned char> (text.front ()))) {
	    text.remove_prefix (1);
	}
	while (!text.empty () && std::isspace (static_cast<unsigned char> (text.back ()))) {
	    text.remove_suffix (1);
	}
	return std::string (text);
    }

    std::string clean_title (std::string text) {
	text = decode_html_entities (text);

	// Strip suffixes added by DesktopHut
	for (const auto& suffix :
	     { " — animated wallpaper", " – Free Live Wallpaper", " – Free Animated Desktop Background" }) {
	    if (text.ends_with (suffix)) {
		text.resize (text.size () - std::string_view (suffix).size ());
		break;
	    }
	}

	return trim (text);
    }

    std::string normalize_url (std::string_view url, std::string_view base_url) {
	if (url.empty ()) {
	    return {};
	}

	// Replace escaped slashes
	std::string result;
	result.reserve (url.size ());
	for (std::size_t i = 0; i < url.size (); ++i) {
	    if (url[i] == '\\' && i + 1 < url.size () && url[i + 1] == '/') {
		result.push_back ('/');
		++i;
	    } else {
		result.push_back (url[i]);
	    }
	}

	if (result.starts_with ("http://") || result.starts_with ("https://")) {
	    return result;
	}

	if (result.starts_with ("//")) {
	    return "https:" + result;
	}

	std::string base (base_url);
	if (base.ends_with ('/')) {
	    base.pop_back ();
	}

	if (!result.starts_with ('/')) {
	    result.insert (result.begin (), '/');
	}

	return base + result;
    }

    std::string url_encode (std::string_view text) {
	std::string encoded;
	encoded.reserve (text.size () * 3 / 2);

	for (const unsigned char c : text) {
	    if (std::isalnum (c) || c == '-' || c == '_' || c == '.' || c == '~') {
		encoded.push_back (static_cast<char> (c));
	    } else if (c == ' ') {
		encoded.push_back ('+');
	    } else {
		encoded += std::format ("%{:02X}", c);
	    }
	}

	return encoded;
    }

} // namespace

std::string decode_html_entities (std::string_view text) {
    std::string result;
    result.reserve (text.size ());

    for (std::size_t i = 0; i < text.size ();) {
	if (text[i] == '&') {
	    const auto semi = text.find (';', i);
	    if (semi != std::string_view::npos && semi - i < 10) {
		const auto entity = text.substr (i, semi - i + 1);
		if (entity == "&amp;") {
		    result.push_back ('&');
		} else if (entity == "&quot;") {
		    result.push_back ('"');
		} else if (entity == "&apos;" || entity == "&#039;" || entity == "&#39;") {
		    result.push_back ('\'');
		} else if (entity == "&lt;") {
		    result.push_back ('<');
		} else if (entity == "&gt;") {
		    result.push_back ('>');
		} else if (entity == "&nbsp;") {
		    result.push_back (' ');
		} else if (entity == "&#x2F;") {
		    result.push_back ('/');
		} else {
		    result.append (entity);
		}
		i = semi + 1;
		continue;
	    }
	}
	result.push_back (text[i]);
	++i;
    }

    return result;
}

std::string extract_slug (std::string_view input) {
    std::string s = trim (input);

    if (s.empty ()) {
	return {};
    }

    // Strip scheme and domain if full URL
    if (s.starts_with ("http://") || s.starts_with ("https://")) {
	const auto scheme_end = s.find ("://");
	const auto path_start = s.find ('/', scheme_end + 3);
	if (path_start != std::string::npos) {
	    s = s.substr (path_start);
	} else {
	    s.clear ();
	}
    }

    // Strip query parameters and anchors
    const auto query_pos = s.find_first_of ("?#");
    if (query_pos != std::string::npos) {
	s = s.substr (0, query_pos);
    }

    // Strip leading slashes
    while (s.starts_with ('/')) {
	s.erase (0, 1);
    }

    // Strip trailing slashes
    while (s.ends_with ('/')) {
	s.pop_back ();
    }

    // If starts with "desktophut-", strip prefix
    if (s.starts_with ("desktophut-")) {
	s.erase (0, 11);
    }

    // If path was something like category/... or search/..., take last part
    const auto last_slash = s.rfind ('/');
    if (last_slash != std::string::npos) {
	s = s.substr (last_slash + 1);
    }

    return s;
}

std::string build_search_url (const SearchOptions& options, std::string_view base_url) {
    std::string base (base_url);
    if (base.ends_with ('/')) {
	base.pop_back ();
    }

    if (!options.query.empty ()) {
	std::string query_enc = url_encode (options.query);
	std::string url = base + "/search/" + query_enc;
	if (options.page > 1) {
	    url += std::format ("?page={}", options.page);
	}
	return url;
    }

    if (!options.category.empty ()) {
	std::string cat (options.category);
	if (!cat.contains ('-') && !cat.ends_with ("-live-wallpapers")) {
	    cat += "-live-wallpapers";
	}
	std::string url = base + "/category/" + cat;
	if (options.page > 1) {
	    url += std::format ("?page={}", options.page);
	}
	return url;
    }

    if (options.page > 1) {
	return std::format ("{}/?page={}", base, options.page);
    }

    return base;
}

std::vector<DesktopHutCard> parse_cards (std::string_view html, std::string_view base_url) {
    std::vector<DesktopHutCard> cards;

    // Pattern to match wallpaper-card links:
    // <a ... class="...wallpaper-card..." ...> ... </a>
    // We use a robust regex or state machine.
    static const std::regex card_regex (
	R"(<a\s+[^>]*class=["'][^"']*wallpaper-card[^"']*["'][^>]*>([\s\S]*?)</a>)", std::regex::optimize
    );

    // Attribute extraction regexes
    static const std::regex href_regex (R"(href=["']([^"']+)["'])", std::regex::optimize);
    static const std::regex title_tag_regex (
	R"(<h3[^>]*class=["'][^"']*card-title[^"']*["'][^>]*>([\s\S]*?)</h3>)", std::regex::optimize
    );
    static const std::regex title_attr_regex (R"(title=["']([^"']+)["'])", std::regex::optimize);
    static const std::regex res_regex (
	R"(<span[^>]*class=["'][^"']*badge-res[^"']*["'][^>]*>([\s\S]*?)</span>)", std::regex::optimize
    );
    static const std::regex img_regex (R"(<img[^>]*\s+(?:data-src|src)=["']([^"']+)["'])", std::regex::optimize);
    static const std::regex video_regex (R"(<video[^>]*\s+(?:data-src|src)=["']([^"']+)["'])", std::regex::optimize);

    std::string html_str (html);
    auto it = std::sregex_iterator (html_str.begin (), html_str.end (), card_regex);
    const auto end = std::sregex_iterator ();

    for (; it != end; ++it) {
	const std::smatch& card_match = *it;
	const std::string card_html = card_match.str ();

	// Extract href
	std::string href;
	std::smatch match;
	if (std::regex_search (card_html, match, href_regex)) {
	    href = match.str (1);
	}

	const std::string slug = extract_slug (href);
	if (slug.empty ()) {
	    continue;
	}

	// Extract title
	std::string title;
	if (std::regex_search (card_html, match, title_tag_regex)) {
	    title = match.str (1);
	} else if (std::regex_search (card_html, match, title_attr_regex)) {
	    title = match.str (1);
	}
	title = clean_title (title);
	if (title.empty ()) {
	    title = slug;
	}

	// Extract resolution
	std::string resolution = "HD";
	if (std::regex_search (card_html, match, res_regex)) {
	    resolution = trim (decode_html_entities (match.str (1)));
	}

	// Extract thumbnail URL
	std::string thumbnail_url;
	if (std::regex_search (card_html, match, img_regex)) {
	    thumbnail_url = normalize_url (match.str (1), base_url);
	}

	// Extract preview video URL
	std::string preview_video_url;
	if (std::regex_search (card_html, match, video_regex)) {
	    preview_video_url = normalize_url (match.str (1), base_url);
	}

	std::string page_url = normalize_url (slug, base_url);

	cards.push_back (
	    DesktopHutCard {
		.id = slug,
		.title = std::move (title),
		.resolution = std::move (resolution),
		.thumbnail_url = std::move (thumbnail_url),
		.preview_video_url = std::move (preview_video_url),
		.page_url = std::move (page_url),
	    }
	);
    }

    return cards;
}

std::expected<DesktopHutDetail, core::Error>
parse_detail_page (std::string_view html, std::string_view fallback_slug, std::string_view base_url) {
    if (html.empty ()) {
	return core::fail (core::ErrorCode::malformed_data, "empty HTML for detail page");
    }

    std::string html_str (html);

    // 1. Video URL resolution:
    // Try primaryDownloadBtn, contentUrl in JSON-LD, data-wallpaper-url, /files/...mp4, /dh-file/...
    std::string video_url;
    std::smatch match;

    // Pattern 1: id="primaryDownloadBtn" with href
    static const std::regex primary_btn_regex1 (
	R"(id=["']primaryDownloadBtn["'][^>]*href=["']([^"']+)["'])", std::regex::optimize
    );
    static const std::regex primary_btn_regex2 (
	R"(href=["']([^"']+)["'][^>]*id=["']primaryDownloadBtn["'])", std::regex::optimize
    );

    // Pattern 2: contentUrl inside JSON-LD
    static const std::regex content_url_regex (
	R"raw("(?:contentUrl|contentURL)":\s*"([^"]+)")raw", std::regex::optimize
    );

    // Pattern 3: data-wallpaper-url
    static const std::regex data_wp_url_regex (R"(data-wallpaper-url=["']([^"']+)["'])", std::regex::optimize);

    // Pattern 4: files/....mp4 or dh-file/...
    static const std::regex files_mp4_regex (
	R"(href=["']([^"']*(?:/files/|/dh-file/)[^"']+)["'])", std::regex::optimize
    );

    // Pattern 5: source tag with video/mp4
    static const std::regex source_video_regex (R"(<source\s+[^>]*src=["']([^"']+\.mp4)["'])", std::regex::optimize);

    if (std::regex_search (html_str, match, primary_btn_regex1)
	|| std::regex_search (html_str, match, primary_btn_regex2)) {
	video_url = match.str (1);
    } else if (std::regex_search (html_str, match, content_url_regex)) {
	video_url = match.str (1);
    } else if (std::regex_search (html_str, match, data_wp_url_regex)) {
	video_url = match.str (1);
    } else if (std::regex_search (html_str, match, files_mp4_regex)) {
	video_url = match.str (1);
    } else if (std::regex_search (html_str, match, source_video_regex)) {
	video_url = match.str (1);
    }

    if (video_url.empty ()) {
	return core::fail (core::ErrorCode::not_found, "cannot find video download URL in DesktopHut page");
    }

    video_url = normalize_url (video_url, base_url);

    // 2. Title extraction:
    std::string title;
    static const std::regex h1_regex (R"(<h1[^>]*>([\s\S]*?)</h1>)", std::regex::optimize);
    static const std::regex og_title_regex (
	R"(<meta\s+property=["']og:title["']\s+content=["']([^"']+)["'])", std::regex::optimize
    );
    static const std::regex jsonld_name_regex (R"raw("(?:name|headline)":\s*"([^"]+)")raw", std::regex::optimize);

    if (std::regex_search (html_str, match, h1_regex)) {
	title = match.str (1);
    } else if (std::regex_search (html_str, match, jsonld_name_regex)) {
	title = match.str (1);
    } else if (std::regex_search (html_str, match, og_title_regex)) {
	title = match.str (1);
    }
    title = clean_title (title);
    if (title.empty ()) {
	title = std::string (fallback_slug);
    }

    // 3. Thumbnail URL:
    std::string thumbnail_url;
    static const std::regex og_image_regex (
	R"(<meta\s+property=["']og:image["']\s+content=["']([^"']+)["'])", std::regex::optimize
    );
    static const std::regex poster_regex (R"(poster=["']([^"']+)["'])", std::regex::optimize);

    if (std::regex_search (html_str, match, og_image_regex)) {
	thumbnail_url = normalize_url (match.str (1), base_url);
    } else if (std::regex_search (html_str, match, poster_regex)) {
	thumbnail_url = normalize_url (match.str (1), base_url);
    }

    // 4. Resolution:
    std::string resolution = "HD";
    static const std::regex desc_res_regex (R"((\d{3,4}x\d{3,4}))", std::regex::optimize);
    if (std::regex_search (html_str, match, desc_res_regex)) {
	resolution = match.str (1);
    }

    std::string slug = std::string (fallback_slug);
    if (slug.empty ()) {
	static const std::regex canonical_regex (
	    R"(<link\s+rel=["']canonical["']\s+href=["']([^"']+)["'])", std::regex::optimize
	);
	if (std::regex_search (html_str, match, canonical_regex)) {
	    slug = extract_slug (match.str (1));
	}
    }

    return DesktopHutDetail {
	.id = slug,
	.title = std::move (title),
	.resolution = std::move (resolution),
	.video_url = std::move (video_url),
	.thumbnail_url = std::move (thumbnail_url),
	.page_url = normalize_url (slug, base_url),
    };
}

DesktopHutClient::DesktopHutClient (std::string base_url) : base_url_ (std::move (base_url)) { }

std::expected<std::vector<DesktopHutCard>, core::Error> DesktopHutClient::search (const SearchOptions& options) {
    const std::string url = build_search_url (options, this->base_url_);
    core::log_info ("DesktopHut searching URL: ", url);

    const auto response = http_get (url);
    if (!response) {
	return std::unexpected (response.error ());
    }

    return parse_cards (*response, this->base_url_);
}

std::expected<DesktopHutDetail, core::Error> DesktopHutClient::fetch_details (std::string_view slug_or_url) {
    const std::string slug = extract_slug (slug_or_url);
    if (slug.empty ()) {
	return core::fail (core::ErrorCode::invalid_argument, "invalid slug or url");
    }

    const std::string url = normalize_url (slug, this->base_url_);
    core::log_info ("DesktopHut fetching details from: ", url);

    const auto response = http_get (url);
    if (!response) {
	return std::unexpected (response.error ());
    }

    return parse_detail_page (*response, slug, this->base_url_);
}

std::expected<std::filesystem::path, core::Error> DesktopHutClient::download_wallpaper (
    std::string_view slug_or_url, std::optional<std::string_view> custom_id,
    const std::filesystem::path& destination_dir
) {
    const std::string slug = extract_slug (slug_or_url);
    const std::string id = custom_id.has_value () && !custom_id->empty () ? std::string (*custom_id) : slug;

    if (id.empty ()) {
	return core::fail (core::ErrorCode::invalid_argument, "cannot determine wallpaper ID");
    }

    const std::filesystem::path target_dir
	= destination_dir.empty () ? desktophut_wallpapers_dir () / id : destination_dir;

    std::error_code ec;
    std::filesystem::create_directories (target_dir, ec);
    if (ec) {
	return core::fail (
	    core::ErrorCode::io, std::format ("cannot create directory '{}': {}", target_dir.string (), ec.message ())
	);
    }

    std::string video_url;
    std::string thumbnail_url;
    std::string title = id;

    // Check if slug_or_url is already a direct MP4 link
    if (slug_or_url.ends_with (".mp4")) {
	video_url = std::string (slug_or_url);
    } else {
	auto details = fetch_details (slug_or_url);
	if (!details) {
	    return std::unexpected (details.error ());
	}

	video_url = details->video_url;
	thumbnail_url = details->thumbnail_url;
	if (!details->title.empty ()) {
	    title = details->title;
	}
    }

    // 1. Download video: wallpaper.mp4
    const std::filesystem::path video_file = target_dir / "wallpaper.mp4";
    core::log_info ("downloading video from ", video_url, " to ", video_file.string ());
    const auto video_dl = http_download (video_url, video_file);
    if (!video_dl) {
	return std::unexpected (video_dl.error ());
    }

    // 2. Download thumbnail / preview: preview.jpg
    const std::string preview_filename = "preview.jpg";
    const std::filesystem::path preview_file = target_dir / preview_filename;
    if (!thumbnail_url.empty ()) {
	core::log_info ("downloading thumbnail from ", thumbnail_url, " to ", preview_file.string ());
	const auto thumb_dl = http_download (thumbnail_url, preview_file);
	if (!thumb_dl) {
	    core::log_warn ("failed to download preview image: ", thumb_dl.error ().message);
	}
    }

    // 3. Generate standard project.json:
    // {
    //   "title": "<title>",
    //   "type": "video",
    //   "file": "wallpaper.mp4",
    //   "preview": "preview.jpg"
    // }
    nlohmann::json project_doc;
    project_doc["title"] = title;
    project_doc["type"] = "video";
    project_doc["file"] = "wallpaper.mp4";
    project_doc["preview"] = preview_filename;

    const std::filesystem::path project_file = target_dir / "project.json";
    std::ofstream project_stream (project_file, std::ios::trunc);
    if (!project_stream) {
	return core::fail (core::ErrorCode::io, std::format ("cannot open '{}' for writing", project_file.string ()));
    }

    project_stream << project_doc.dump (2) << "\n";
    if (!project_stream) {
	return core::fail (core::ErrorCode::io, std::format ("failed writing '{}'", project_file.string ()));
    }

    return target_dir;
}

} // namespace we::desktophut
