#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "support/harness.hpp"
#include "support/temp_dir.hpp"
#include "we/gfx/context.hpp"
#include "we/gfx/renderer.hpp"
#include "we/scene/asset_container.hpp"
#include "we/scene/project.hpp"
#include "we/scene/scene.hpp"
#include "we/scene/scene_renderer.hpp"
#include "we/scene/texture.hpp"

using namespace we;

namespace {

void append_u32 (std::vector<std::byte>& output, std::uint32_t value) {
    for (unsigned int shift = 0; shift < 32; shift += 8) {
	output.push_back (static_cast<std::byte> ((value >> shift) & 0xFFU));
    }
}

void append_raw (std::vector<std::byte>& output, std::string_view value) {
    for (const char character : value) {
	output.push_back (static_cast<std::byte> (static_cast<unsigned char> (character)));
    }
}

void append_magic (std::vector<std::byte>& output, std::string_view value) {
    append_raw (output, value);
    output.push_back (std::byte { 0 });
}

[[nodiscard]] std::vector<std::byte> make_argb_texture (
    int width, int height, std::span<const std::uint8_t> rgba, int canvas_width = 0, int canvas_height = 0
) {
    std::vector<std::byte> bytes;
    append_magic (bytes, "TEXV0005");
    append_magic (bytes, "TEXI0001");
    append_u32 (bytes, 0); // ARGB8888
    append_u32 (bytes, 0); // flags
    append_u32 (bytes, static_cast<std::uint32_t> (canvas_width > 0 ? canvas_width : width));
    append_u32 (bytes, static_cast<std::uint32_t> (canvas_height > 0 ? canvas_height : height));
    append_u32 (bytes, static_cast<std::uint32_t> (width));
    append_u32 (bytes, static_cast<std::uint32_t> (height));
    append_u32 (bytes, 0);

    append_magic (bytes, "TEXB0003");
    append_u32 (bytes, 1); // images
    append_u32 (bytes, 0); // free image format
    append_u32 (bytes, 1); // mipmaps
    append_u32 (bytes, static_cast<std::uint32_t> (width));
    append_u32 (bytes, static_cast<std::uint32_t> (height));
    append_u32 (bytes, 0); // compression: none
    append_u32 (bytes, static_cast<std::uint32_t> (rgba.size ()));
    append_u32 (bytes, static_cast<std::uint32_t> (rgba.size ()));

    for (std::size_t index = 0; index < rgba.size (); index += 4) {
	bytes.push_back (static_cast<std::byte> (rgba[index + 2]));
	bytes.push_back (static_cast<std::byte> (rgba[index + 1]));
	bytes.push_back (static_cast<std::byte> (rgba[index + 0]));
	bytes.push_back (static_cast<std::byte> (rgba[index + 3]));
    }

    return bytes;
}

[[nodiscard]] std::vector<std::byte> make_dxt1_texture () {
    std::vector<std::byte> bytes;
    append_magic (bytes, "TEXV0005");
    append_magic (bytes, "TEXI0001");
    append_u32 (bytes, 7); // DXT1
    append_u32 (bytes, 0);
    append_u32 (bytes, 4);
    append_u32 (bytes, 4);
    append_u32 (bytes, 4);
    append_u32 (bytes, 4);
    append_u32 (bytes, 0);

    append_magic (bytes, "TEXB0003");
    append_u32 (bytes, 1);
    append_u32 (bytes, 0);
    append_u32 (bytes, 1);
    append_u32 (bytes, 4);
    append_u32 (bytes, 4);
    append_u32 (bytes, 0);
    append_u32 (bytes, 8);
    append_u32 (bytes, 8);

    // color0 = red, color1 = blue, all indices 0.
    const std::array<std::uint8_t, 8> block = { 0x00, 0xF8, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00 };
    append_raw (bytes, std::string_view (reinterpret_cast<const char*> (block.data ()), block.size ()));
    return bytes;
}

[[nodiscard]] std::vector<std::byte>
make_rg88_texture (int width, int height, std::span<const std::uint8_t> rg, std::uint32_t flags = 0) {
    std::vector<std::byte> bytes;
    append_magic (bytes, "TEXV0005");
    append_magic (bytes, "TEXI0001");
    append_u32 (bytes, 8); // format_rg88
    append_u32 (bytes, flags);
    append_u32 (bytes, static_cast<std::uint32_t> (width));
    append_u32 (bytes, static_cast<std::uint32_t> (height));
    append_u32 (bytes, static_cast<std::uint32_t> (width));
    append_u32 (bytes, static_cast<std::uint32_t> (height));
    append_u32 (bytes, 0);

    append_magic (bytes, "TEXB0003");
    append_u32 (bytes, 1);
    append_u32 (bytes, 0);
    append_u32 (bytes, 1);
    append_u32 (bytes, static_cast<std::uint32_t> (width));
    append_u32 (bytes, static_cast<std::uint32_t> (height));
    append_u32 (bytes, 0);
    append_u32 (bytes, static_cast<std::uint32_t> (rg.size ()));
    append_u32 (bytes, static_cast<std::uint32_t> (rg.size ()));

    for (const auto val : rg) {
	bytes.push_back (static_cast<std::byte> (val));
    }
    return bytes;
}

void write_project (const std::filesystem::path& root, std::string_view texture_name = "textures/quad.tex") {
    we::test::write_text (
	root / "project.json", R"({"title":"Test Scene","type":"scene","file":"scene.json","workshopid":1234})"
    );

    we::test::write_text (
	root / "scene.json",
	R"({
            "camera": {"center": [0,0,0], "eye": [0,0,1], "up": [0,1,0]},
            "general": {"orthogonalprojection": {"width": 100, "height": 100}, "clearcolor": [0,0,0]},
            "objects": [{
                "id": 1,
                "name": "layer",
                "image": "models/layer.json",
                "origin": [50,50,0],
                "scale": [1,1,1],
                "angles": [0,0,0],
                "alpha": 1,
                "color": [1,1,1],
                "size": [100,100],
                "visible": true
            }]
        })"
    );

    we::test::write_text (root / "models/layer.json", R"({"material":"materials/layer.json"})");
    we::test::write_text (
	root / "materials/layer.json",
	R"({"passes":[{"shader":"genericimage","textures":[")" + std::string (texture_name)
	    + R"("],"blending":"normal"}]})"
    );
}

void write_transform_layer (
    const std::filesystem::path& root, std::string_view origin, std::string_view scale, std::string_view angles,
    std::string_view size, std::string_view texture
) {
    std::string document = R"({
        "camera": {"center": "0 0 0", "eye": "0 0 1", "up": "0 1 0"},
        "general": {"orthogonalprojection": {"width": 100, "height": 100}, "clearcolor": "1 0 0"},
        "objects": [{
            "id": 1, "name": "layer", "image": "models/layer.json",
            "origin": ")";
    document += origin;
    document += R"(", "scale": ")";
    document += scale;
    document += R"(", "angles": ")";
    document += angles;
    document += R"(", "alpha": 1, "color": "1 1 1", "size": ")";
    document += size;
    document += R"(", "visible": true
        }]
    })";

    we::test::write_text (root / "scene.json", document);
    we::test::write_text (root / "models/layer.json", R"({"material":"materials/layer.json"})");
    we::test::write_text (
	root / "materials/layer.json",
	R"({"passes":[{"shader":"genericimage","textures":[")" + std::string (texture) + R"("],"blending":"normal"}]})"
    );
}

[[nodiscard]] std::array<std::uint8_t, 4>
render_scene_pixel (const std::filesystem::path& root, int size, int x, int y) {
    auto assets = scene::AssetContainer::open (root);
    CHECK (assets.has_value ());

    auto parsed = scene::load_scene (*assets, "scene.json");
    CHECK (parsed.has_value ());

    auto context = gfx::Context::create_headless ();
    CHECK (context.has_value ());
    auto surface = context->create_pbuffer (size, size);
    CHECK (surface.has_value ());
    context->make_current (&*surface);

    auto renderer = gfx::Renderer::create ();
    CHECK (renderer.has_value ());

    auto scene_renderer = scene::SceneRenderer::build (*parsed, *assets);
    CHECK (scene_renderer.has_value ());

    renderer->begin (size, size, scene_renderer->clear_color ());
    scene_renderer->render (*renderer);
    renderer->end ();

    std::vector<std::uint8_t> output (static_cast<std::size_t> (size) * static_cast<std::size_t> (size) * 4U);
    surface->read_rgba (output, size, size);

    const std::size_t index
	= (static_cast<std::size_t> (y) * static_cast<std::size_t> (size) + static_cast<std::size_t> (x)) * 4U;
    return { output[index], output[index + 1], output[index + 2], output[index + 3] };
}

void write_lighting_layer (const std::filesystem::path& root) {
    we::test::write_text (root / "models/lit.json", R"({"material":"materials/lit.json"})");
    we::test::write_text (
	root / "materials/lit.json",
	R"({"passes":[{"shader":"genericimage4","combos":{"LIGHTING":1},"textures":["textures/white.tex"]}]})"
    );

    const std::array<std::uint8_t, 4> white = { 255, 255, 255, 255 };
    we::test::write_bytes (root / "textures/white.tex", make_argb_texture (1, 1, white));
}

void write_lighting_scene (const std::filesystem::path& root, std::string_view ambient, std::string_view light = {}) {
    std::string document = R"({
        "camera": {"center": [0,0,0], "eye": [0,0,1], "up": [0,1,0]},
        "general": {"orthogonalprojection": {"width": 100, "height": 100},
                    "clearcolor": [0,0,1], "ambientcolor": ")";
    document += ambient;
    document += R"("},
        "objects": [{
            "id": 1, "name": "lit", "image": "models/lit.json", "origin": [50,50,0],
            "scale": [1,1,1], "alpha": 1, "color": [1,1,1], "size": [100,100], "visible": true
        })";

    if (!light.empty ()) {
	document += ", ";
	document += light;
    }

    document += "]}";
    we::test::write_text (root / "scene.json", document);
}

void write_lit_project (const std::filesystem::path& root, std::string_view texture_name) {
    we::test::write_text (
	root / "scene.json",
	R"({
            "camera": {"center": [0,0,0], "eye": [0,0,1], "up": [0,1,0]},
            "general": {"orthogonalprojection": {"width": 100, "height": 100}, "clearcolor": [1,0,0]},
            "objects": [{
                "id": 1,
                "name": "lit",
                "image": "models/lit.json",
                "origin": [50,50,0],
                "scale": [1,1,1],
                "alpha": 1,
                "color": [0,0,0],
                "size": [100,100],
                "visible": true
            }]
        })"
    );
    we::test::write_text (root / "models/lit.json", R"({"material":"materials/lit.json"})");
    we::test::write_text (
	root / "materials/lit.json",
	R"({"passes":[{"shader":"genericimage4","combos":{"LIGHTING":1,"REFLECTION":1},"textures":[")"
	    + std::string (texture_name) + R"("]}]})"
    );
}

} // namespace

TEST (texture_decodes_argb_texb) {
    const std::array<std::uint8_t, 16> rgba = {
	255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255,
    };
    const auto bytes = make_argb_texture (2, 2, rgba);

    auto texture = scene::decode_texture (bytes);

    CHECK (texture.has_value ());
    CHECK_EQ (texture->width, 2);
    CHECK_EQ (texture->height, 2);
    CHECK_EQ (texture->rgba.size (), std::size_t { 16 });
    CHECK_EQ (texture->rgba[0], std::uint8_t { 255 });
    CHECK_EQ (texture->rgba[1], std::uint8_t { 0 });
    CHECK_EQ (texture->rgba[4], std::uint8_t { 0 });
    CHECK_EQ (texture->rgba[5], std::uint8_t { 255 });
}

TEST (texture_decodes_dxt1_blocks) {
    const auto bytes = make_dxt1_texture ();
    auto texture = scene::decode_texture (bytes);

    CHECK (texture.has_value ());
    CHECK_EQ (texture->width, 4);
    CHECK_EQ (texture->height, 4);
    CHECK (texture->rgba[0] > 200);
    CHECK (texture->rgba[1] < 40);
    CHECK (texture->rgba[2] < 40);
}

TEST (texture_decodes_rg88_standard) {
    const std::array<std::uint8_t, 2> rg = { 120, 200 };
    const auto bytes = make_rg88_texture (1, 1, rg, 0);

    auto texture = scene::decode_texture (bytes);
    CHECK (texture.has_value ());
    CHECK_EQ (texture->width, 1);
    CHECK_EQ (texture->height, 1);
    CHECK_EQ (texture->rgba.size (), std::size_t { 4 });
    CHECK_EQ (texture->rgba[0], std::uint8_t { 120 });
    CHECK_EQ (texture->rgba[1], std::uint8_t { 200 });
    CHECK_EQ (texture->rgba[2], std::uint8_t { 0 });
    CHECK_EQ (texture->rgba[3], std::uint8_t { 255 });
}

TEST (texture_decodes_rg88_alpha_priority) {
    const std::array<std::uint8_t, 2> rg = { 120, 200 };
    const auto bytes = make_rg88_texture (1, 1, rg, 524288);

    auto texture = scene::decode_texture (bytes);
    CHECK (texture.has_value ());
    CHECK_EQ (texture->width, 1);
    CHECK_EQ (texture->height, 1);
    CHECK_EQ (texture->rgba.size (), std::size_t { 4 });
    CHECK_EQ (texture->rgba[0], std::uint8_t { 120 });
    CHECK_EQ (texture->rgba[1], std::uint8_t { 120 });
    CHECK_EQ (texture->rgba[2], std::uint8_t { 120 });
    CHECK_EQ (texture->rgba[3], std::uint8_t { 200 });
}

TEST (texture_rejects_truncated_payloads) {
    auto bytes = make_argb_texture (2, 2, std::array<std::uint8_t, 16> {});
    bytes.resize (bytes.size () - 3);

    auto texture = scene::decode_texture (bytes);
    CHECK (!texture.has_value ());
    CHECK_EQ (texture.error ().code, core::ErrorCode::malformed_data);

    auto garbage = scene::decode_texture (std::array<std::byte, 4> {});
    CHECK (!garbage.has_value ());
}

TEST (project_and_scene_parse_from_extracted_directory) {
    we::test::TempDir temp;
    write_project (temp.path ());

    auto assets = scene::AssetContainer::open (temp.path ());
    CHECK (assets.has_value ());

    auto project = scene::load_project (*assets);
    CHECK (project.has_value ());
    CHECK_EQ (project->type, scene::WallpaperType::scene);
    CHECK_EQ (project->title, std::string ("Test Scene"));
    CHECK_EQ (project->workshop_id, std::string ("1234"));

    auto parsed = scene::load_scene (*assets, project->file);
    CHECK (parsed.has_value ());
    CHECK_EQ (parsed->camera.width, 100);
    CHECK_EQ (parsed->camera.height, 100);
    CHECK_EQ (parsed->images.size (), std::size_t { 1 });
    CHECK_EQ (parsed->images[0].texture_path, std::string ("textures/quad.tex"));
}

TEST (scene_parses_wallpaper_engine_string_vectors) {
    we::test::TempDir temp;

    we::test::write_text (
	temp.path () / "scene.json",
	R"({
            "camera": {"center": "960 540 -1", "eye": "958 542 0", "up": "0 1 0"},
            "general": {"orthogonalprojection": {"width": 1920, "height": 1080}},
            "objects": [{
                "id": 1, "name": "layer", "image": "models/layer.json",
                "origin": "1000 200 3", "scale": "2 3 4", "angles": "0 0 1.5",
                "size": "40 20", "parallaxDepth": "0.25 -0.5"
            }]
        })"
    );
    we::test::write_text (temp.path () / "models/layer.json", R"({"material":"materials/layer.json"})");
    we::test::write_text (
	temp.path () / "materials/layer.json",
	R"({"passes":[{"shader":"genericimage","textures":["textures/quad.tex"]}]})"
    );

    auto assets = scene::AssetContainer::open (temp.path ());
    CHECK (assets.has_value ());

    auto parsed = scene::load_scene (*assets, "scene.json");
    CHECK (parsed.has_value ());
    CHECK_EQ (parsed->camera.center.x, 960.0F);
    CHECK_EQ (parsed->camera.center.y, 540.0F);
    CHECK_EQ (parsed->camera.eye.x, 958.0F);
    CHECK_EQ (parsed->camera.eye.y, 542.0F);
    CHECK_EQ (parsed->images.size (), std::size_t { 1 });

    const auto& image = parsed->images[0];
    CHECK_EQ (image.origin.x, 1000.0F);
    CHECK_EQ (image.origin.y, 200.0F);
    CHECK_EQ (image.origin.z, 3.0F);
    CHECK_EQ (image.scale.x, 2.0F);
    CHECK_EQ (image.scale.y, 3.0F);
    CHECK_EQ (image.scale.z, 4.0F);
    CHECK_EQ (image.angles.z, 1.5F);
    CHECK_EQ (image.size.x, 40.0F);
    CHECK_EQ (image.size.y, 20.0F);
    CHECK_EQ (image.parallax_depth.x, 0.25F);
    CHECK_EQ (image.parallax_depth.y, -0.5F);
}

TEST (scene_renderer_draws_image_layers_headless) {
    we::test::TempDir temp;
    write_project (temp.path ());

    const std::array<std::uint8_t, 16> pixels = {
	255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255,
    };
    we::test::write_bytes (temp / "textures/quad.tex", make_argb_texture (2, 2, pixels));

    auto assets = scene::AssetContainer::open (temp.path ());
    CHECK (assets.has_value ());

    auto project = scene::load_project (*assets);
    CHECK (project.has_value ());

    auto parsed = scene::load_scene (*assets, project->file);
    CHECK (parsed.has_value ());

    auto context = gfx::Context::create_headless ();
    CHECK (context.has_value ());

    auto surface = context->create_pbuffer (100, 100);
    CHECK (surface.has_value ());
    context->make_current (&*surface);

    auto renderer = gfx::Renderer::create ();
    CHECK (renderer.has_value ());

    auto scene_renderer = scene::SceneRenderer::build (*parsed, *assets);
    CHECK (scene_renderer.has_value ());
    CHECK_EQ (scene_renderer->layer_count (), std::size_t { 1 });

    renderer->begin (100, 100, scene_renderer->clear_color ());
    scene_renderer->render (*renderer);
    renderer->end ();

    std::vector<std::uint8_t> output (100U * 100U * 4U);
    surface->read_rgba (output, 100, 100);

    const auto at = [&output] (int x, int y, int channel) {
	return output
	    [(static_cast<std::size_t> (y) * 100U + static_cast<std::size_t> (x)) * 4U
	     + static_cast<std::size_t> (channel)];
    };

    // Top-left quadrant is red.
    CHECK (at (25, 25, 0) > 200);
    CHECK (at (25, 25, 1) < 60);

    // Bottom-right quadrant is white.
    CHECK (at (75, 75, 0) > 200);
    CHECK (at (75, 75, 1) > 200);
    CHECK (at (75, 75, 2) > 200);
}

TEST (scene_layer_origin_uses_scene_coordinates) {
    we::test::TempDir temp;
    write_transform_layer (temp.path (), "60 30 0", "1 1 1", "0 0 0", "20 20", "textures/white.tex");

    const std::array<std::uint8_t, 4> white = { 255, 255, 255, 255 };
    we::test::write_bytes (temp / "textures/white.tex", make_argb_texture (1, 1, white));

    // 20x20 quad centred on scene coordinate (60, 30): scene y grows downward.
    const auto inside = render_scene_pixel (temp.path (), 100, 60, 30);
    CHECK (inside[0] > 200);
    CHECK (inside[1] > 200);
    CHECK (inside[2] > 200);

    const auto left_of_quad = render_scene_pixel (temp.path (), 100, 30, 30);
    CHECK (left_of_quad[0] > 200);
    CHECK (left_of_quad[1] < 60);
    CHECK (left_of_quad[2] < 60);

    const auto below_quad = render_scene_pixel (temp.path (), 100, 60, 60);
    CHECK (below_quad[1] < 60);
    CHECK (below_quad[2] < 60);
}

TEST (scene_layer_scale_multiplies_quad) {
    we::test::TempDir temp;
    write_transform_layer (temp.path (), "50 50 0", "2 1.5 1", "0 0 0", "40 20", "textures/white.tex");

    const std::array<std::uint8_t, 4> white = { 255, 255, 255, 255 };
    we::test::write_bytes (temp / "textures/white.tex", make_argb_texture (1, 1, white));

    // 40x20 scaled by (2, 1.5) covers x in [10, 90] and y in [35, 65].
    const auto left = render_scene_pixel (temp.path (), 100, 15, 50);
    CHECK (left[1] > 200);

    const auto outside_left = render_scene_pixel (temp.path (), 100, 5, 50);
    CHECK (outside_left[1] < 60);

    const auto top = render_scene_pixel (temp.path (), 100, 50, 37);
    CHECK (top[1] > 200);

    const auto outside_top = render_scene_pixel (temp.path (), 100, 50, 33);
    CHECK (outside_top[1] < 60);
}

TEST (scene_layer_angles_rotate_quad_clockwise) {
    we::test::TempDir temp;
    write_transform_layer (temp.path (), "50 50 0", "1 1 1", "0 0 1.5708", "40 10", "textures/quad.tex");

    // Left half of the texture is red and its right half is green, so the
    // rotated quad shows which texture edge ends up on top.
    std::array<std::uint8_t, 32> pixels {};

    for (std::size_t index = 0; index < 4; ++index) {
	const bool green = index >= 2;
	pixels[index * 4 + 0] = green ? 0 : 255;
	pixels[index * 4 + 1] = green ? 255 : 0;
	pixels[index * 4 + 2] = 0;
	pixels[index * 4 + 3] = 255;
	pixels[16 + index * 4 + 0] = green ? 0 : 255;
	pixels[16 + index * 4 + 1] = green ? 255 : 0;
	pixels[16 + index * 4 + 2] = 0;
	pixels[16 + index * 4 + 3] = 255;
    }

    we::test::write_bytes (temp / "textures/quad.tex", make_argb_texture (4, 2, pixels));

    // angles are radians: 1.5708 turns the 40x10 quad 90 degrees clockwise in
    // scene space, so the texture's left (red) edge ends up at the top.
    const auto top = render_scene_pixel (temp.path (), 100, 50, 35);
    CHECK (top[0] > 200);
    CHECK (top[1] < 60);
    CHECK (top[2] < 60);

    const auto bottom = render_scene_pixel (temp.path (), 100, 50, 65);
    CHECK (bottom[0] < 60);
    CHECK (bottom[1] > 200);
    CHECK (bottom[2] < 60);

    // The unrotated rectangle would cover (25, 50); after the rotation it does not.
    const auto outside = render_scene_pixel (temp.path (), 100, 25, 50);
    CHECK (outside[1] < 60);
    CHECK (outside[2] < 60);
    CHECK (outside[0] > 200);
}

TEST (scene_automatic_projection_uses_rotated_quad_bounds) {
    const auto write_auto_scene = [] (const std::filesystem::path& root, std::string_view angles) {
	std::string document = R"({
            "camera": {"center": "0 0 0", "eye": "0 0 1", "up": "0 1 0"},
            "general": {"orthogonalprojection": {"width": 0, "height": 0, "auto": true}},
            "objects": [{
                "id": 1, "name": "layer", "image": "models/layer.json",
                "origin": "70 30 0", "scale": "1 1 1", "angles": ")";
	document += angles;
	document += R"(", "size": "40 20"
            }]
        })";

	we::test::write_text (root / "scene.json", document);
	we::test::write_text (root / "models/layer.json", R"({"material":"materials/layer.json"})");
	we::test::write_text (
	    root / "materials/layer.json", R"({"passes":[{"shader":"genericimage","textures":["textures/quad.tex"]}]})"
	);
    };

    we::test::TempDir unrotated;
    write_auto_scene (unrotated.path (), "0 0 0");
    auto assets = scene::AssetContainer::open (unrotated.path ());
    CHECK (assets.has_value ());
    auto parsed = scene::load_scene (*assets, "scene.json");
    CHECK (parsed.has_value ());
    CHECK_EQ (parsed->camera.width, 40);
    CHECK_EQ (parsed->camera.height, 20);

    we::test::TempDir rotated;
    write_auto_scene (rotated.path (), "0 0 1.5708");
    auto rotated_assets = scene::AssetContainer::open (rotated.path ());
    CHECK (rotated_assets.has_value ());
    auto rotated_scene = scene::load_scene (*rotated_assets, "scene.json");
    CHECK (rotated_scene.has_value ());
    CHECK_EQ (rotated_scene->camera.width, 20);
    CHECK_EQ (rotated_scene->camera.height, 40);
}

TEST (scene_layer_samples_cropped_texture_without_uv_scaling) {
    we::test::TempDir temp;
    write_transform_layer (temp.path (), "50 50 0", "1 1 1", "0 0 0", "100 100", "textures/quad.tex");

    // 2x2 bitmap stored in a 4x4 TEX canvas, as Wallpaper Engine does for
    // auto-cropped models. The header canvas must not shrink the sampled UVs.
    const std::array<std::uint8_t, 16> pixels = {
	255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255,
    };
    we::test::write_bytes (temp / "textures/quad.tex", make_argb_texture (2, 2, pixels, 4, 4));

    const auto top_left = render_scene_pixel (temp.path (), 100, 25, 25);
    CHECK (top_left[0] > 200);
    CHECK (top_left[1] < 60);

    const auto top_right = render_scene_pixel (temp.path (), 100, 75, 25);
    CHECK (top_right[0] < 60);
    CHECK (top_right[1] > 200);

    const auto bottom_left = render_scene_pixel (temp.path (), 100, 25, 75);
    CHECK (bottom_left[0] < 60);
    CHECK (bottom_left[2] > 200);

    const auto bottom_right = render_scene_pixel (temp.path (), 100, 75, 75);
    CHECK (bottom_right[0] > 200);
    CHECK (bottom_right[1] > 200);
    CHECK (bottom_right[2] > 200);
}

TEST (scene_lit_material_keeps_scene_colour) {
    we::test::TempDir temp;
    write_lit_project (temp.path (), "textures/white.tex");

    const std::array<std::uint8_t, 4> white = { 255, 255, 255, 255 };
    we::test::write_bytes (temp / "textures/white.tex", make_argb_texture (1, 1, white));

    auto assets = scene::AssetContainer::open (temp.path ());
    CHECK (assets.has_value ());

    auto parsed = scene::load_scene (*assets, "scene.json");
    CHECK (parsed.has_value ());
    CHECK_EQ (parsed->images.size (), std::size_t { 1 });
    CHECK (parsed->images[0].lit_material);
    CHECK_EQ (parsed->images[0].color.r, 0.0F);

    auto context = gfx::Context::create_headless ();
    CHECK (context.has_value ());
    auto surface = context->create_pbuffer (100, 100);
    CHECK (surface.has_value ());
    context->make_current (&*surface);

    auto renderer = gfx::Renderer::create ();
    CHECK (renderer.has_value ());

    auto scene_renderer = scene::SceneRenderer::build (*parsed, *assets);
    CHECK (scene_renderer.has_value ());
    CHECK_EQ (scene_renderer->layer_count (), std::size_t { 1 });

    renderer->begin (100, 100, scene_renderer->clear_color ());
    scene_renderer->render (*renderer);
    renderer->end ();

    std::vector<std::uint8_t> output (100U * 100U * 4U);
    surface->read_rgba (output, 100, 100);

    // The white albedo is tinted with the object colour, so the lit layer is
    // black; the red clear colour would show through if the layer were dropped.
    CHECK_EQ (output[0], std::uint8_t { 0 });
    CHECK_EQ (output[1], std::uint8_t { 0 });
    CHECK_EQ (output[2], std::uint8_t { 0 });
}

TEST (scene_skips_internal_render_targets) {
    we::test::TempDir temp;
    write_lit_project (temp.path (), "_rt_FullFrameBuffer");

    auto assets = scene::AssetContainer::open (temp.path ());
    CHECK (assets.has_value ());

    auto parsed = scene::load_scene (*assets, "scene.json");
    CHECK (parsed.has_value ());
    CHECK (parsed->images.empty ());
    CHECK_EQ (parsed->skipped_objects, 1);
}

TEST (scene_reflective_material_samples_frame) {
    we::test::TempDir temp;

    we::test::write_text (
	temp / "scene.json",
	R"({
            "camera": {"center": [0,0,0], "eye": [0,0,1], "up": [0,1,0]},
            "general": {"orthogonalprojection": {"width": 200, "height": 200}, "clearcolor": [0,0,1]},
            "objects": [
                {
                    "id": 1, "name": "background", "image": "models/background.json",
                    "origin": [100,100,0], "scale": [1,1,1], "size": [200,200], "color": [1,1,1]
                },
                {
                    "id": 2, "name": "glass", "image": "models/glass.json",
                    "origin": [100,100,0], "scale": [1,1,1], "size": [100,100], "color": [0,0,0]
                }
            ]
        })"
    );
    we::test::write_text (temp / "models/background.json", R"({"material":"materials/background.json"})");
    we::test::write_text (
	temp / "materials/background.json",
	R"({"passes":[{"shader":"genericimage","textures":["textures/green.tex"]}]})"
    );
    we::test::write_text (temp / "models/glass.json", R"({"material":"materials/glass.json"})");
    we::test::write_text (
	temp / "materials/glass.json",
	R"({"passes":[{"shader":"genericimage4","combos":{"LIGHTING":1,"REFLECTION":1},)"
	R"("constantshadervalues":{"metallic":1,"reflectivity":1,"roughness":0},)"
	R"("textures":["textures/white.tex","textures/normal.tex"]}]})"
    );

    const std::array<std::uint8_t, 4> green = { 0, 255, 0, 255 };
    const std::array<std::uint8_t, 4> white = { 255, 255, 255, 255 };
    const std::array<std::uint8_t, 4> normal = { 233, 128, 0, 255 };
    we::test::write_bytes (temp / "textures/green.tex", make_argb_texture (1, 1, green));
    we::test::write_bytes (temp / "textures/white.tex", make_argb_texture (1, 1, white));
    we::test::write_bytes (temp / "textures/normal.tex", make_argb_texture (1, 1, normal));

    auto assets = scene::AssetContainer::open (temp.path ());
    CHECK (assets.has_value ());

    auto parsed = scene::load_scene (*assets, "scene.json");
    CHECK (parsed.has_value ());
    CHECK_EQ (parsed->images.size (), std::size_t { 2 });
    CHECK (parsed->images[1].reflective);
    CHECK_EQ (parsed->images[1].normal_texture_path, std::string ("textures/normal.tex"));
    CHECK_EQ (parsed->images[1].roughness, 0.0F);
    CHECK_EQ (parsed->images[1].metallic, 1.0F);

    auto context = gfx::Context::create_headless ();
    CHECK (context.has_value ());
    auto surface = context->create_pbuffer (200, 200);
    CHECK (surface.has_value ());
    context->make_current (&*surface);

    auto renderer = gfx::Renderer::create ();
    CHECK (renderer.has_value ());

    auto scene_renderer = scene::SceneRenderer::build (*parsed, *assets);
    CHECK (scene_renderer.has_value ());
    CHECK_EQ (scene_renderer->layer_count (), std::size_t { 2 });

    renderer->begin (200, 200, scene_renderer->clear_color ());
    scene_renderer->render (*renderer);
    renderer->end ();

    std::vector<std::uint8_t> output (200U * 200U * 4U);
    surface->read_rgba (output, 200, 200);

    const auto at = [&output] (int x, int y, int channel) {
	return output
	    [(static_cast<std::size_t> (y) * 200U + static_cast<std::size_t> (x)) * 4U
	     + static_cast<std::size_t> (channel)];
    };

    // The background is green. The glass has a black albedo and a tilted normal
    // (nz around 0.56), so REFLECTION adds roughly a quarter of that green.
    CHECK (at (10, 10, 1) > 200);
    CHECK (at (100, 100, 0) < 20);
    CHECK (at (100, 100, 1) > 35);
    CHECK (at (100, 100, 1) < 95);
    CHECK (at (100, 100, 2) < 20);
}

TEST (scene_parses_point_lights_and_ambient) {
    we::test::TempDir temp;

    we::test::write_text (
	temp / "scene.json",
	R"({
            "camera": {"center": [0,0,0], "eye": [0,0,1], "up": [0,1,0]},
            "general": {"orthogonalprojection": {"width": 100, "height": 100},
                        "ambientcolor": "0.25 0.5 0.75"},
            "objects": [
                {"id": 7, "light": "lpoint", "origin": "10 20 30", "color": "0.5 0.25 0.125",
                 "intensity": 2, "radius": 300, "exponent": 1.5, "density": 2},
                {"id": 8, "light": "lspot", "origin": [0,0,0]},
                {"id": 9, "particle": "particles/rain.json"}
            ]
        })"
    );

    auto assets = scene::AssetContainer::open (temp.path ());
    CHECK (assets.has_value ());

    auto parsed = scene::load_scene (*assets, "scene.json");
    CHECK (parsed.has_value ());
    CHECK (parsed->images.empty ());
    CHECK_EQ (parsed->lights.size (), std::size_t { 1 });
    CHECK_EQ (parsed->lights[0].id, 7);
    CHECK_EQ (parsed->lights[0].origin.x, 10.0F);
    CHECK_EQ (parsed->lights[0].origin.y, 20.0F);
    CHECK_EQ (parsed->lights[0].origin.z, 30.0F);
    CHECK_EQ (parsed->lights[0].color.r, 0.5F);
    CHECK_EQ (parsed->lights[0].color.g, 0.25F);
    CHECK_EQ (parsed->lights[0].color.b, 0.125F);
    CHECK_EQ (parsed->lights[0].intensity, 2.0F);
    CHECK_EQ (parsed->lights[0].radius, 300.0F);
    CHECK_EQ (parsed->lights[0].exponent, 1.5F);
    CHECK_EQ (parsed->lights[0].density, 2.0F);
    CHECK_EQ (parsed->ambient.x, 0.25F);
    CHECK_EQ (parsed->ambient.y, 0.5F);
    CHECK_EQ (parsed->ambient.z, 0.75F);
    // The spot light is not rendered yet, but the particle object is parsed.
    CHECK_EQ (parsed->skipped_objects, 1);
    CHECK_EQ (parsed->particles.size (), std::size_t { 1 });
    CHECK_EQ (parsed->particles[0].id, 9);
    CHECK_EQ (parsed->particles[0].file, "particles/rain.json");
}

TEST (scene_parses_pbr_mask_material) {
    we::test::TempDir temp;

    we::test::write_text (
	temp / "scene.json",
	R"({
            "camera": {"center": [0,0,0], "eye": [0,0,1], "up": [0,1,0]},
            "general": {"orthogonalprojection": {"width": 100, "height": 100}},
            "objects": [{
                "id": 1, "name": "pbr", "image": "models/pbr.json", "origin": [50,50,0],
                "scale": [1,1,1], "size": [100,100], "color": [1,1,1]
            }]
        })"
    );
    we::test::write_text (temp / "models/pbr.json", R"({"material":"materials/pbr.json"})");
    we::test::write_text (
	temp / "materials/pbr.json",
	R"({"passes":[{"shader":"genericimage4",)"
	R"("combos":{"LIGHTING":1,"REFLECTION":1,"PBRMASKS":1,)"
	R"("METALLIC_MAP":1,"ROUGHNESS_MAP":1,"REFLECTION_MAP":1,"EMISSIVE_MAP":1},)"
	R"("constantshadervalues":{"metallic":0.2,"roughness":0.4,"reflectivity":0.6,)"
	R"("speculartint":"0 0 1","emissivecolor":"1 0.5 0","emissivebrightness":3},)"
	R"("textures":["textures/albedo.tex","textures/normal.tex","textures/mask.tex"]}]})"
    );

    auto assets = scene::AssetContainer::open (temp.path ());
    CHECK (assets.has_value ());

    auto parsed = scene::load_scene (*assets, "scene.json");
    CHECK (parsed.has_value ());
    CHECK_EQ (parsed->images.size (), std::size_t { 1 });

    const auto& image = parsed->images[0];
    CHECK (image.lit_material);
    CHECK (image.lighting);
    CHECK (image.reflective);
    CHECK_EQ (image.texture_path, std::string ("textures/albedo.tex"));
    CHECK_EQ (image.normal_texture_path, std::string ("textures/normal.tex"));
    CHECK_EQ (image.pbr_mask_texture_path, std::string ("textures/mask.tex"));
    CHECK (image.metallic_map);
    CHECK (image.roughness_map);
    CHECK (image.reflection_map);
    CHECK (image.emissive_map);
    CHECK_EQ (image.specular_tint.x, 0.0F);
    CHECK_EQ (image.specular_tint.y, 0.0F);
    CHECK_EQ (image.specular_tint.z, 1.0F);
    CHECK_EQ (image.emissive_color.x, 1.0F);
    CHECK_EQ (image.emissive_color.y, 0.5F);
    CHECK_EQ (image.emissive_brightness, 3.0F);
    CHECK_EQ (image.reflectivity, 0.6F);
}

TEST (scene_point_light_lights_lit_material) {
    we::test::TempDir temp;
    write_lighting_layer (temp.path ());
    write_lighting_scene (
	temp.path (), "0 0 0",
	R"({"id": 2, "light": "lpoint", "origin": "50 50 100", "color": "1 1 1",)"
	R"("intensity": 1, "radius": 1000, "exponent": 1})"
    );

    const auto pixel = render_scene_pixel (temp.path (), 100, 50, 50);

    // The white albedo is multiplied by the object colour and then lit. Without
    // the LIGHTING term the quad would stay pure white, and without any light
    // it would be black, so the mid-grey value proves the light contributed.
    CHECK (pixel[0] > 40);
    CHECK (pixel[0] < 160);
    CHECK_EQ (pixel[0], pixel[1]);
    CHECK_EQ (pixel[0], pixel[2]);
    CHECK_EQ (pixel[3], std::uint8_t { 255 });
}

TEST (scene_point_light_radius_limits_reach) {
    we::test::TempDir lit;
    write_lighting_layer (lit.path ());
    write_lighting_scene (
	lit.path (), "0 0 0",
	R"({"id": 2, "light": "lpoint", "origin": "50 50 500", "color": "1 1 1",)"
	R"("intensity": 1, "radius": 2000, "exponent": 1})"
    );

    const auto bright = render_scene_pixel (lit.path (), 100, 50, 50);
    CHECK (bright[0] > 30);
    // The lit layer also covers the blue clear colour.
    CHECK (bright[2] >= bright[0] - 2);
    CHECK (bright[2] <= bright[0] + 2);

    we::test::TempDir dark;
    write_lighting_layer (dark.path ());
    write_lighting_scene (
	dark.path (), "0 0 0",
	R"({"id": 2, "light": "lpoint", "origin": "50 50 500", "color": "1 1 1",)"
	R"("intensity": 1, "radius": 100, "exponent": 1})"
    );

    // The light cannot reach the layer, so it stays black over the blue clear.
    const auto unlit = render_scene_pixel (dark.path (), 100, 50, 50);
    CHECK (unlit[0] < 10);
    CHECK (unlit[1] < 10);
    CHECK (unlit[2] < 10);
}

TEST (scene_ambient_lights_lit_material) {
    we::test::TempDir temp;
    write_lighting_layer (temp.path ());
    write_lighting_scene (temp.path (), "0.5 0 0");

    const auto pixel = render_scene_pixel (temp.path (), 100, 50, 50);

    // ambient = g_LightAmbientColor * color.rgb, with the white albedo tinted
    // by the white object colour.
    CHECK (pixel[0] > 100);
    CHECK (pixel[0] < 150);
    CHECK (pixel[1] < 10);
    CHECK (pixel[2] < 10);
}

TEST (scene_pbr_emissive_mask_lights_material) {
    we::test::TempDir temp;

    we::test::write_text (
	temp / "scene.json",
	R"({
            "camera": {"center": [0,0,0], "eye": [0,0,1], "up": [0,1,0]},
            "general": {"orthogonalprojection": {"width": 100, "height": 100},
                        "clearcolor": [0,0,1], "ambientcolor": "0 0 0"},
            "objects": [{
                "id": 1, "name": "glow", "image": "models/glow.json", "origin": [50,50,0],
                "scale": [1,1,1], "size": [100,100], "color": [1,1,1]
            }]
        })"
    );
    we::test::write_text (temp / "models/glow.json", R"({"material":"materials/glow.json"})");
    we::test::write_text (
	temp / "materials/glow.json",
	R"({"passes":[{"shader":"genericimage4",)"
	R"("combos":{"LIGHTING":1,"PBRMASKS":1,"EMISSIVE_MAP":1},)"
	R"("constantshadervalues":{"emissivecolor":"0 1 0","emissivebrightness":1},)"
	R"("textures":["textures/white.tex","","textures/mask.tex"]}]})"
    );

    const std::array<std::uint8_t, 4> white = { 255, 255, 255, 255 };
    we::test::write_bytes (temp / "textures/white.tex", make_argb_texture (1, 1, white));
    we::test::write_bytes (temp / "textures/mask.tex", make_argb_texture (1, 1, white));

    const auto pixel = render_scene_pixel (temp.path (), 100, 50, 50);

    // EMISSIVE_MAP maxes the lighting with emissivecolor * albedo * mask.a, so
    // only the green component survives.
    CHECK (pixel[0] < 20);
    CHECK (pixel[1] > 200);
    CHECK (pixel[2] < 20);
}

TEST (scene_renderer_reads_textures_from_scene_pkg) {
    we::test::TempDir temp;
    write_project (temp.path (), "textures/quad.tex");

    const std::array<std::uint8_t, 4> pixel = { 0, 255, 0, 255 };
    const auto texture = make_argb_texture (1, 1, pixel);

    // Build a minimal PKGV archive with the texture and scene files.
    struct Entry {
	std::string name;
	std::vector<std::byte> data;
    };

    std::vector<Entry> entries;
    entries.push_back ({ "textures/quad.tex", texture });
    entries.push_back ({ "models/layer.json", [] {
			    std::vector<std::byte> value;
			    append_raw (value, R"({"material":"materials/layer.json"})");
			    return value;
			}() });
    entries.push_back (
	{ "materials/layer.json", [] {
	     std::vector<std::byte> value;
	     append_raw (value, R"({"passes":[{"shader":"genericimage","textures":["textures/quad.tex"]}]})");
	     return value;
	 }() }
    );
    entries.push_back (
	{ "scene.json", [] {
	     std::vector<std::byte> value;
	     append_raw (
		 value,
		 R"({"camera":{"center":[0,0,0],"eye":[0,0,1],"up":[0,1,0]},)"
		 R"("general":{"orthogonalprojection":{"width":10,"height":10}},)"
		 R"("objects":[{"id":1,"name":"l","image":"models/layer.json","origin":[5,5,0],"size":[10,10]}]})"
	     );
	     return value;
	 }() }
    );

    std::vector<std::byte> package;
    append_u32 (package, 8);
    append_raw (package, "PKGV0001");
    append_u32 (package, static_cast<std::uint32_t> (entries.size ()));

    std::uint32_t offset = 0;

    for (const auto& entry : entries) {
	append_u32 (package, static_cast<std::uint32_t> (entry.name.size ()));
	append_raw (package, entry.name);
	append_u32 (package, offset);
	append_u32 (package, static_cast<std::uint32_t> (entry.data.size ()));
	offset += static_cast<std::uint32_t> (entry.data.size ());
    }

    for (const auto& entry : entries) {
	package.insert (package.end (), entry.data.begin (), entry.data.end ());
    }

    we::test::write_bytes (temp / "scene.pkg", package);

    // Remove the extracted texture to prove it comes from the package.
    std::filesystem::remove (temp / "textures/quad.tex");

    auto assets = scene::AssetContainer::open (temp.path ());
    CHECK (assets.has_value ());
    CHECK (assets->is_in_package ("textures/quad.tex"));

    auto project = scene::load_project (*assets);
    CHECK (project.has_value ());

    auto parsed = scene::load_scene (*assets, project->file);
    CHECK (parsed.has_value ());

    auto context = gfx::Context::create_headless ();
    CHECK (context.has_value ());
    auto surface = context->create_pbuffer (10, 10);
    CHECK (surface.has_value ());
    context->make_current (&*surface);

    auto renderer = gfx::Renderer::create ();
    CHECK (renderer.has_value ());
    auto scene_renderer = scene::SceneRenderer::build (*parsed, *assets);
    CHECK (scene_renderer.has_value ());
    CHECK_EQ (scene_renderer->layer_count (), std::size_t { 1 });

    renderer->begin (10, 10, scene_renderer->clear_color ());
    scene_renderer->render (*renderer);
    renderer->end ();

    std::vector<std::uint8_t> output (10U * 10U * 4U);
    surface->read_rgba (output, 10, 10);
    CHECK (output[1] > 200);
    CHECK (output[0] < 60);
}

namespace {

// TEXB0003 container whose only mipmap is an MP4 stream, the layout of a video wallpaper.
[[nodiscard]] std::vector<std::byte> make_video_texture (int width, int height, const std::vector<std::byte>& mp4) {
    std::vector<std::byte> bytes;
    append_magic (bytes, "TEXV0005");
    append_magic (bytes, "TEXI0001");
    append_u32 (bytes, 0);
    append_u32 (bytes, 0);
    append_u32 (bytes, static_cast<std::uint32_t> (width));
    append_u32 (bytes, static_cast<std::uint32_t> (height));
    append_u32 (bytes, static_cast<std::uint32_t> (width));
    append_u32 (bytes, static_cast<std::uint32_t> (height));
    append_u32 (bytes, 0);

    append_magic (bytes, "TEXB0003");
    append_u32 (bytes, 1);
    append_u32 (bytes, 0);
    append_u32 (bytes, 1);
    append_u32 (bytes, static_cast<std::uint32_t> (width));
    append_u32 (bytes, static_cast<std::uint32_t> (height));
    append_u32 (bytes, 0);
    append_u32 (bytes, static_cast<std::uint32_t> (mp4.size ()));
    append_u32 (bytes, static_cast<std::uint32_t> (mp4.size ()));
    bytes.insert (bytes.end (), mp4.begin (), mp4.end ());
    return bytes;
}

} // namespace

TEST (video_layer_reports_frame_changes_from_mp4_texture) {
    std::ifstream fixture (std::filesystem::path (WE_TEST_FIXTURES_DIR) / "testsrc.mp4", std::ios::binary);
    CHECK (fixture.good ());
    const std::string raw { std::istreambuf_iterator<char> (fixture), std::istreambuf_iterator<char> () };
    std::vector<std::byte> mp4;
    mp4.reserve (raw.size ());

    for (const char character : raw) {
	mp4.push_back (static_cast<std::byte> (static_cast<unsigned char> (character)));
    }
    CHECK (!mp4.empty ());

    we::test::TempDir temp;
    write_project (temp.path (), "textures/video.tex");
    we::test::write_bytes (temp / "textures/video.tex", make_video_texture (64, 48, mp4));

    auto assets = scene::AssetContainer::open (temp.path ());
    CHECK (assets.has_value ());
    auto project = scene::load_project (*assets);
    CHECK (project.has_value ());
    auto parsed = scene::load_scene (*assets, project->file);
    CHECK (parsed.has_value ());

    auto context = gfx::Context::create_headless ();
    CHECK (context.has_value ());
    auto surface = context->create_pbuffer (100, 100);
    CHECK (surface.has_value ());
    context->make_current (&*surface);

    auto scene_renderer = scene::SceneRenderer::build (*parsed, *assets);
    CHECK (scene_renderer.has_value ());
    CHECK_EQ (scene_renderer->layer_count (), std::size_t { 1 });

    // mpv decodes asynchronously, so poll until the first frame arrives.
    bool changed = false;
    for (int attempt = 0; attempt < 200 && !changed; ++attempt) {
	changed = scene_renderer->update_videos ();
	if (!changed) {
	    std::this_thread::sleep_for (std::chrono::milliseconds (10));
	}
    }
    CHECK (changed);

    // Once a frame is uploaded, the layer keeps its decoded size.
    scene_renderer->report_video_swap ();
}

TEST (scene_post_processing_layer_count) {
    we::test::TempDir temp;
    write_project (temp.path ());
    we::test::write_bytes (
	temp / "textures/quad.tex", make_argb_texture (1, 1, std::array<std::uint8_t, 4> { 255, 255, 255, 255 })
    );

    we::test::write_text (
	temp / "scene.json",
	R"({
            "camera": {"center": [0,0,0], "eye": [0,0,1], "up": [0,1,0]},
            "general": {"orthogonalprojection": {"width": 100, "height": 100}, "clearcolor": [0,0,0]},
            "objects": [
                {
                    "id": 1, "name": "bg", "image": "models/layer.json",
                    "origin": [50,50,0], "scale": [1,1,1], "size": [100,100], "visible": true
                },
                {
                    "id": 2, "name": "post", "image": "models/post.json",
                    "origin": [50,50,0], "scale": [1,1,1], "size": [100,100], "visible": true,
                    "effects": [{
                        "file": "effects/test/effect.json",
                        "passes": [{
                            "constantshadervalues": { "ui_strength": 0.5 }
                        }]
                    }]
                }
            ]
        })"
    );
    we::test::write_text (temp / "models/post.json", R"({"material":"materials/post.json","fullscreen":true})");
    we::test::write_text (
	temp / "materials/post.json", R"({"passes":[{"shader":"passthrough","textures":["_rt_FullFrameBuffer"]}]})"
    );
    we::test::write_text (
	temp / "effects/test/effect.json", R"({"passes":[{"material":"materials/effects/fx.json"}]})"
    );
    we::test::write_text (temp / "materials/effects/fx.json", R"({"passes":[{"shader":"effects/fx"}]})");
    we::test::write_text (
	temp / "shaders/effects/fx.vert",
	"attribute vec3 a_Position;\nattribute vec2 a_TexCoord;\nvarying vec2 v_TexCoord;\nvoid main() { gl_Position = "
	"vec4(a_Position, 1.0); v_TexCoord = a_TexCoord; }\n"
    );
    we::test::write_text (
	temp / "shaders/effects/fx.frag",
	"varying vec2 v_TexCoord;\nuniform float u_Strength; // {\"material\":\"ui_strength\",\"default\":1.0}\nvoid "
	"main() { gl_FragColor = texSample2D(g_Texture0, v_TexCoord) * u_Strength; }\n"
    );

    auto assets = scene::AssetContainer::open (temp.path ());
    CHECK (assets.has_value ());
    auto parsed = scene::load_scene (*assets, "scene.json");
    CHECK (parsed.has_value ());
    CHECK_EQ (parsed->images.size (), std::size_t { 2 });

    auto context = gfx::Context::create_headless ();
    CHECK (context.has_value ());
    auto surface = context->create_pbuffer (100, 100);
    CHECK (surface.has_value ());
    context->make_current (&*surface);

    auto scene_renderer = scene::SceneRenderer::build (*parsed, *assets);
    CHECK (scene_renderer.has_value ());
    CHECK_EQ (scene_renderer->layer_count (), std::size_t { 2 });
    CHECK (scene_renderer->uses_offscreen ());
}

TEST (scene_post_processing_chromatic_aberration) {
    we::test::TempDir temp;
    we::test::write_text (
	temp / "scene.json",
	R"({
            "camera": {"center": [0,0,0], "eye": [0,0,1], "up": [0,1,0]},
            "general": {"orthogonalprojection": {"width": 100, "height": 100}, "clearcolor": [0,0,0]},
            "objects": [
                {
                    "id": 1, "name": "white_box", "image": "models/box.json",
                    "origin": [50,50,0], "scale": [1,1,1], "size": [40,40], "visible": true
                },
                {
                    "id": 2, "name": "chroma", "image": "models/post.json",
                    "origin": [50,50,0], "scale": [1,1,1], "size": [100,100], "visible": true,
                    "effects": [{
                        "file": "effects/chroma/effect.json",
                        "passes": [{
                            "constantshadervalues": { "ui_offset": "0.15 0" }
                        }]
                    }]
                }
            ]
        })"
    );
    we::test::write_text (temp / "models/box.json", R"({"material":"materials/box.json"})");
    we::test::write_text (
	temp / "materials/box.json", R"({"passes":[{"shader":"genericimage","textures":["textures/white.tex"]}]})"
    );
    const std::array<std::uint8_t, 4> white = { 255, 255, 255, 255 };
    we::test::write_bytes (temp / "textures/white.tex", make_argb_texture (1, 1, white));

    we::test::write_text (temp / "models/post.json", R"({"material":"materials/post.json","fullscreen":true})");
    we::test::write_text (
	temp / "materials/post.json", R"({"passes":[{"shader":"passthrough","textures":["_rt_FullFrameBuffer"]}]})"
    );
    we::test::write_text (
	temp / "effects/chroma/effect.json", R"({"passes":[{"material":"materials/effects/chroma.json"}]})"
    );
    we::test::write_text (temp / "materials/effects/chroma.json", R"({"passes":[{"shader":"effects/chroma"}]})");
    we::test::write_text (
	temp / "shaders/effects/chroma.vert",
	"attribute vec3 a_Position;\nattribute vec2 a_TexCoord;\nvarying vec2 v_TexCoord;\nvoid main() { gl_Position = "
	"vec4(a_Position, 1.0); v_TexCoord = a_TexCoord; }\n"
    );
    we::test::write_text (
	temp / "shaders/effects/chroma.frag",
	"varying vec2 v_TexCoord;\n"
	"uniform vec2 u_Offset; // {\"material\":\"ui_offset\",\"default\":\"0.1 0\"}\n"
	"void main() {\n"
	"    vec4 s_red = texSample2D(g_Texture0, v_TexCoord + u_Offset);\n"
	"    vec4 s_blue = texSample2D(g_Texture0, v_TexCoord - u_Offset);\n"
	"    vec4 base = texSample2D(g_Texture0, v_TexCoord);\n"
	"    gl_FragColor = vec4(s_red.r, base.g, s_blue.b, 1.0);\n"
	"}\n"
    );

    auto assets = scene::AssetContainer::open (temp.path ());
    CHECK (assets.has_value ());
    auto parsed = scene::load_scene (*assets, "scene.json");
    CHECK (parsed.has_value ());

    auto context = gfx::Context::create_headless ();
    CHECK (context.has_value ());
    auto surface = context->create_pbuffer (100, 100);
    CHECK (surface.has_value ());
    context->make_current (&*surface);

    auto renderer = gfx::Renderer::create ();
    CHECK (renderer.has_value ());
    auto scene_renderer = scene::SceneRenderer::build (*parsed, *assets);
    CHECK (scene_renderer.has_value ());

    renderer->begin (100, 100, scene_renderer->clear_color ());
    scene_renderer->render (*renderer);
    renderer->end ();

    std::vector<std::uint8_t> output (100U * 100U * 4U);
    surface->read_rgba (output, 100, 100);

    const auto at = [&output] (int x, int y, int channel) {
	return output
	    [(static_cast<std::size_t> (y) * 100U + static_cast<std::size_t> (x)) * 4U
	     + static_cast<std::size_t> (channel)];
    };

    // Center (50, 50) is inside the white quad: R, G, B are all high.
    CHECK (at (50, 50, 0) > 200);
    CHECK (at (50, 50, 1) > 200);
    CHECK (at (50, 50, 2) > 200);

    // Left fringe: x=20, y=50. Unshifted box starts at x=30.
    // Red samples at x + 0.15*100 = 35 (inside white box) -> Red > 200.
    // Blue samples at x - 0.15*100 = 5 (in black bg) -> Blue < 30.
    // Green is unshifted base (at 20 in black bg) -> Green < 30.
    CHECK (at (20, 50, 0) > 200);
    CHECK (at (20, 50, 1) < 30);
    CHECK (at (20, 50, 2) < 30);

    // Right fringe: x=80, y=50.
    // Red samples at 80 + 15 = 95 (black bg) -> Red < 30.
    // Blue samples at 80 - 15 = 65 (inside white box) -> Blue > 200.
    CHECK (at (80, 50, 0) < 30);
    CHECK (at (80, 50, 2) > 200);
}

TEST (scene_post_processing_blur) {
    we::test::TempDir temp;
    we::test::write_text (
	temp / "scene.json",
	R"({
            "camera": {"center": [0,0,0], "eye": [0,0,1], "up": [0,1,0]},
            "general": {"orthogonalprojection": {"width": 100, "height": 100}, "clearcolor": [0,0,0]},
            "objects": [
                {
                    "id": 1, "name": "right_white", "image": "models/white.json",
                    "origin": [75,50,0], "scale": [1,1,1], "size": [50,100], "visible": true
                },
                {
                    "id": 2, "name": "blur_layer", "image": "models/post.json",
                    "origin": [50,50,0], "scale": [1,1,1], "size": [100,100], "visible": true,
                    "effects": [{
                        "file": "effects/blur/effect.json",
                        "passes": [{ "constantshadervalues": { "ui_radius": 0.05 } }]
                    }]
                }
            ]
        })"
    );
    we::test::write_text (temp / "models/white.json", R"({"material":"materials/white.json"})");
    we::test::write_text (
	temp / "materials/white.json", R"({"passes":[{"shader":"genericimage","textures":["textures/white.tex"]}]})"
    );
    const std::array<std::uint8_t, 4> white = { 255, 255, 255, 255 };
    we::test::write_bytes (temp / "textures/white.tex", make_argb_texture (1, 1, white));

    we::test::write_text (temp / "models/post.json", R"({"material":"materials/post.json","fullscreen":true})");
    we::test::write_text (
	temp / "materials/post.json", R"({"passes":[{"shader":"passthrough","textures":["_rt_FullFrameBuffer"]}]})"
    );
    we::test::write_text (
	temp / "effects/blur/effect.json", R"({"passes":[{"material":"materials/effects/blur.json"}]})"
    );
    we::test::write_text (temp / "materials/effects/blur.json", R"({"passes":[{"shader":"effects/blur"}]})");
    we::test::write_text (
	temp / "shaders/effects/blur.vert",
	"attribute vec3 a_Position;\nattribute vec2 a_TexCoord;\nvarying vec2 v_TexCoord;\nvoid main() { gl_Position = "
	"vec4(a_Position, 1.0); v_TexCoord = a_TexCoord; }\n"
    );
    we::test::write_text (
	temp / "shaders/effects/blur.frag",
	"varying vec2 v_TexCoord;\n"
	"uniform float u_Radius; // {\"material\":\"ui_radius\",\"default\":0.05}\n"
	"void main() {\n"
	"    vec4 c0 = texSample2D(g_Texture0, v_TexCoord + vec2(-u_Radius, 0.0));\n"
	"    vec4 c1 = texSample2D(g_Texture0, v_TexCoord);\n"
	"    vec4 c2 = texSample2D(g_Texture0, v_TexCoord + vec2(u_Radius, 0.0));\n"
	"    gl_FragColor = 0.25 * c0 + 0.5 * c1 + 0.25 * c2;\n"
	"}\n"
    );

    auto assets = scene::AssetContainer::open (temp.path ());
    CHECK (assets.has_value ());
    auto parsed = scene::load_scene (*assets, "scene.json");
    CHECK (parsed.has_value ());

    auto context = gfx::Context::create_headless ();
    CHECK (context.has_value ());
    auto surface = context->create_pbuffer (100, 100);
    CHECK (surface.has_value ());
    context->make_current (&*surface);

    auto renderer = gfx::Renderer::create ();
    CHECK (renderer.has_value ());
    auto scene_renderer = scene::SceneRenderer::build (*parsed, *assets);
    CHECK (scene_renderer.has_value ());

    renderer->begin (100, 100, scene_renderer->clear_color ());
    scene_renderer->render (*renderer);
    renderer->end ();

    std::vector<std::uint8_t> output (100U * 100U * 4U);
    surface->read_rgba (output, 100, 100);

    const auto at = [&output] (int x, int y, int channel) {
	return output
	    [(static_cast<std::size_t> (y) * 100U + static_cast<std::size_t> (x)) * 4U
	     + static_cast<std::size_t> (channel)];
    };

    // Far left (x=10): deep black
    CHECK (at (10, 50, 0) < 10);
    // Far right (x=90): bright white
    CHECK (at (90, 50, 0) > 240);

    // Boundary at x=50 averages black and white (three-tap blur produces ~191 at 50 and ~64 at 49)
    CHECK (at (50, 50, 0) > 150);
    CHECK (at (50, 50, 0) < 220);
    CHECK (at (49, 50, 0) > 40);
    CHECK (at (49, 50, 0) < 100);

    // Light diffused into black region (x=48)
    CHECK (at (48, 50, 0) > 20);
    // Darkness diffused into white region (x=52)
    CHECK (at (52, 50, 0) < 235);
}

TEST (scene_reflection_with_post_processing) {
    we::test::TempDir temp;

    we::test::write_text (
	temp / "scene.json",
	R"({
            "camera": {"center": [0,0,0], "eye": [0,0,1], "up": [0,1,0]},
            "general": {"orthogonalprojection": {"width": 200, "height": 200}},
            "objects": [
                {
                    "id": 1, "name": "background", "image": "models/background.json",
                    "origin": [100,100,0], "scale": [1,1,1], "size": [200,200]
                },
                {
                    "id": 2, "name": "glass", "image": "models/glass.json",
                    "origin": [100,100,0], "scale": [1,1,1], "size": [100,100], "color": [0,0,0]
                },
                {
                    "id": 3, "name": "post", "image": "models/post.json",
                    "origin": [100,100,0], "scale": [1,1,1], "size": [200,200],
                    "effects": [{
                        "file": "effects/fx/effect.json",
                        "passes": [{ "constantshadervalues": { "ui_tint": 1.0 } }]
                    }]
                }
            ]
        })"
    );
    we::test::write_text (temp / "models/background.json", R"({"material":"materials/background.json"})");
    we::test::write_text (
	temp / "materials/background.json",
	R"({"passes":[{"shader":"genericimage","textures":["textures/green.tex"]}]})"
    );
    we::test::write_text (temp / "models/glass.json", R"({"material":"materials/glass.json"})");
    we::test::write_text (
	temp / "materials/glass.json",
	R"({"passes":[{"shader":"genericimage4","combos":{"LIGHTING":1,"REFLECTION":1},)"
	R"("constantshadervalues":{"metallic":1,"reflectivity":1,"roughness":0},)"
	R"("textures":["textures/white.tex","textures/normal.tex"]}]})"
    );

    const std::array<std::uint8_t, 4> green = { 0, 255, 0, 255 };
    const std::array<std::uint8_t, 4> white = { 255, 255, 255, 255 };
    const std::array<std::uint8_t, 4> normal = { 233, 128, 0, 255 };
    we::test::write_bytes (temp / "textures/green.tex", make_argb_texture (1, 1, green));
    we::test::write_bytes (temp / "textures/white.tex", make_argb_texture (1, 1, white));
    we::test::write_bytes (temp / "textures/normal.tex", make_argb_texture (1, 1, normal));

    we::test::write_text (temp / "models/post.json", R"({"material":"materials/post.json","fullscreen":true})");
    we::test::write_text (
	temp / "materials/post.json", R"({"passes":[{"shader":"passthrough","textures":["_rt_FullFrameBuffer"]}]})"
    );
    we::test::write_text (temp / "effects/fx/effect.json", R"({"passes":[{"material":"materials/effects/fx.json"}]})");
    we::test::write_text (temp / "materials/effects/fx.json", R"({"passes":[{"shader":"effects/fx"}]})");
    we::test::write_text (
	temp / "shaders/effects/fx.vert",
	"attribute vec3 a_Position;\nattribute vec2 a_TexCoord;\nvarying vec2 v_TexCoord;\nvoid main() { gl_Position = "
	"vec4(a_Position, 1.0); v_TexCoord = a_TexCoord; }\n"
    );
    we::test::write_text (
	temp / "shaders/effects/fx.frag",
	"varying vec2 v_TexCoord;\n"
	"uniform float u_Tint; // {\"material\":\"ui_tint\",\"default\":1.0}\n"
	"void main() {\n"
	"    gl_FragColor = texSample2D(g_Texture0, v_TexCoord) * u_Tint;\n"
	"}\n"
    );

    auto assets = scene::AssetContainer::open (temp.path ());
    CHECK (assets.has_value ());

    auto parsed = scene::load_scene (*assets, "scene.json");
    CHECK (parsed.has_value ());
    CHECK_EQ (parsed->images.size (), std::size_t { 3 });
    CHECK (parsed->images[1].reflective);

    auto context = gfx::Context::create_headless ();
    CHECK (context.has_value ());
    auto surface = context->create_pbuffer (200, 200);
    CHECK (surface.has_value ());
    context->make_current (&*surface);

    auto renderer = gfx::Renderer::create ();
    CHECK (renderer.has_value ());

    auto scene_renderer = scene::SceneRenderer::build (*parsed, *assets);
    CHECK (scene_renderer.has_value ());
    CHECK_EQ (scene_renderer->layer_count (), std::size_t { 3 });
    CHECK (scene_renderer->uses_offscreen ());

    renderer->begin (200, 200, scene_renderer->clear_color ());
    scene_renderer->render (*renderer);
    renderer->end ();

    std::vector<std::uint8_t> output (200U * 200U * 4U);
    surface->read_rgba (output, 200, 200);

    const auto at = [&output] (int x, int y, int channel) {
	return output
	    [(static_cast<std::size_t> (y) * 200U + static_cast<std::size_t> (x)) * 4U
	     + static_cast<std::size_t> (channel)];
    };

    // The background is green.
    CHECK (at (10, 10, 1) > 200);
    // Reflective glass reflects the background captured from offscreen scene FBO
    CHECK (at (100, 100, 0) < 20);
    CHECK (at (100, 100, 1) > 35);
    CHECK (at (100, 100, 1) < 95);
    CHECK (at (100, 100, 2) < 20);
}

TEST (scene_particle_system_renders_particles) {
    we::test::TempDir temp;

    we::test::write_text (
	temp / "scene.json",
	R"({
            "camera": {"center": [0,0,0], "eye": [0,0,1], "up": [0,1,0]},
            "general": {"orthogonalprojection": {"width": 100, "height": 100}, "clearcolor": "0 0 0"},
            "objects": [
                {
                    "id": 1, "name": "rain", "particle": "particles/rain.json",
                    "origin": [50,50,0], "scale": [1,1,1]
                }
            ]
        })"
    );

    we::test::write_text (
	temp / "particles/rain.json",
	R"({
            "material": "materials/particle.json",
            "count": 10,
            "emitter": [
                { "name": "boxrandom", "rate": 100, "distancemin": [-10,-10,0], "distancemax": [10,10,0] }
            ],
            "initializer": [
                { "name": "lifetime", "min": 2.0, "max": 2.0 },
                { "name": "size", "min": 10.0, "max": 10.0 },
                { "name": "color", "min": [1.0, 1.0, 1.0], "max": [1.0, 1.0, 1.0] },
                { "name": "alpha", "min": 1.0, "max": 1.0 }
            ]
        })"
    );

    auto assets = scene::AssetContainer::open (temp.path ());
    CHECK (assets.has_value ());

    auto parsed = scene::load_scene (*assets, "scene.json");
    CHECK (parsed.has_value ());
    CHECK_EQ (parsed->particles.size (), std::size_t { 1 });

    auto context = gfx::Context::create_headless ();
    CHECK (context.has_value ());
    auto surface = context->create_pbuffer (100, 100);
    CHECK (surface.has_value ());
    context->make_current (&*surface);

    auto renderer = gfx::Renderer::create ();
    CHECK (renderer.has_value ());

    auto scene_renderer = scene::SceneRenderer::build (*parsed, *assets);
    CHECK (scene_renderer.has_value ());
    CHECK_EQ (scene_renderer->layer_count (), std::size_t { 1 });

    renderer->begin (100, 100, scene_renderer->clear_color ());
    // First render initializes start time
    scene_renderer->render (*renderer);
    renderer->end ();

    std::this_thread::sleep_for (std::chrono::milliseconds (50));

    renderer->begin (100, 100, scene_renderer->clear_color ());
    // Second render advances dt and spawns/updates particles
    scene_renderer->render (*renderer);
    renderer->end ();

    std::vector<std::uint8_t> output (100U * 100U * 4U);
    surface->read_rgba (output, 100, 100);

    int non_zero_pixels = 0;
    for (std::size_t i = 0; i < output.size (); i += 4) {
	if (output[i] > 0 || output[i + 1] > 0 || output[i + 2] > 0) {
	    non_zero_pixels++;
	}
    }
    CHECK (non_zero_pixels > 0);
}
