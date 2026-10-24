#pragma once

#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

#include "we/core/error.hpp"
#include "we/gfx/texture.hpp"
#include "we/scene/asset_container.hpp"

namespace we::scene {

struct TextureData {
    // Dimensions of the stored bitmap (TEX header "image" width/height).
    int width = 0;
    int height = 0;
    // Canvas the bitmap was cropped from (TEX header "texture" width/height).
    // The payload only contains the cropped image, so this is metadata used to
    // report the original canvas; rendering samples the whole uploaded texture.
    int texture_width = 0;
    int texture_height = 0;
    std::uint32_t format = 0;
    std::uint32_t flags = 0;
    bool animated = false;
    int frame_count = 0;
    float duration = 0.0F;
    std::vector<std::uint8_t> rgba;
    // Set when the mipmap is an embedded MP4 stream instead of pixels.
    bool is_video = false;
    std::vector<std::uint8_t> video;
};

// Decodes a Wallpaper Engine `.tex` payload (TEXV/TEXI/TEXB containers).
[[nodiscard]] std::expected<TextureData, core::Error> decode_texture (std::span<const std::byte> data);

// Decodes PNG/JPEG/WebP images using stb_image (preview images and free-image
// textures).
[[nodiscard]] std::expected<TextureData, core::Error> decode_image (std::span<const std::byte> data);

[[nodiscard]] std::expected<gfx::Texture, core::Error> upload_texture (const TextureData& texture);

[[nodiscard]] std::expected<TextureData, core::Error> load_texture_data (const AssetContainer& assets,
                                                                         std::string_view path);

} // namespace we::scene
