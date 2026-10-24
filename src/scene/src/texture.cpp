#include "we/scene/texture.hpp"

#include "we/core/log.hpp"
#include "we/core/lz4.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <filesystem>

#define BCDEC_IMPLEMENTATION
#include "bcdec.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace we::scene {
namespace {

constexpr std::uint32_t format_argb8888 = 0;
constexpr std::uint32_t format_rgb888 = 1;
constexpr std::uint32_t format_rgb565 = 2;
constexpr std::uint32_t format_dxt5 = 4;
constexpr std::uint32_t format_dxt3 = 6;
constexpr std::uint32_t format_dxt1 = 7;
constexpr std::uint32_t format_rg88 = 8;
constexpr std::uint32_t format_r8 = 9;
constexpr std::uint32_t format_bc7 = 12;
constexpr std::uint32_t format_free_image_png = 13;

class Cursor {
public:
    explicit Cursor (std::span<const std::byte> data) : data_ (data) { }

    [[nodiscard]] std::size_t remaining () const noexcept { return this->data_.size () - this->offset_; }
    [[nodiscard]] std::size_t offset () const noexcept { return this->offset_; }
    [[nodiscard]] bool good () const noexcept { return this->failed_ == false; }

    std::uint32_t u32 () {
        if (!this->require (4)) {
            return 0;
        }

        const auto* bytes = reinterpret_cast<const std::uint8_t*> (this->data_.data () + this->offset_);
        this->offset_ += 4;
        return static_cast<std::uint32_t> (bytes[0]) | (static_cast<std::uint32_t> (bytes[1]) << 8)
            | (static_cast<std::uint32_t> (bytes[2]) << 16) | (static_cast<std::uint32_t> (bytes[3]) << 24);
    }

    std::int32_t i32 () { return static_cast<std::int32_t> (this->u32 ()); }

    float f32 () {
        const auto value = this->u32 ();
        return std::bit_cast<float> (value);
    }

    std::string magic9 () {
        if (!this->require (9)) {
            return {};
        }

        std::string value (reinterpret_cast<const char*> (this->data_.data () + this->offset_), 9);
        this->offset_ += 9;
        return value;
    }

    std::string cstring () {
        std::string result;

        while (this->offset_ < this->data_.size ()) {
            const char character = static_cast<char> (this->data_[this->offset_++]);

            if (character == '\0') {
                return result;
            }

            result.push_back (character);
        }

        this->failed_ = true;
        return result;
    }

    [[nodiscard]] std::span<const std::byte> bytes (std::size_t count) {
        if (!this->require (count)) {
            return {};
        }

        const auto slice = this->data_.subspan (this->offset_, count);
        this->offset_ += count;
        return slice;
    }

    bool skip (std::size_t count) {
        if (!this->require (count)) {
            return false;
        }

        this->offset_ += count;
        return true;
    }

private:
    [[nodiscard]] bool require (std::size_t count) {
        if (this->failed_ || count > this->data_.size () - this->offset_) {
            this->failed_ = true;
            return false;
        }

        return true;
    }

    std::span<const std::byte> data_;
    std::size_t offset_ = 0;
    bool failed_ = false;
};

std::uint16_t read_u16 (const std::uint8_t* data) {
    return static_cast<std::uint16_t> (static_cast<std::uint16_t> (data[0])
                                       | (static_cast<std::uint16_t> (data[1]) << 8));
}

void ensure_rgba (TextureData& texture) {
    if (texture.texture_width <= 0) {
        texture.texture_width = texture.width;
    }

    if (texture.texture_height <= 0) {
        texture.texture_height = texture.height;
    }
}

std::expected<std::vector<std::uint8_t>, core::Error>
decode_blocks (std::span<const std::byte> input, int width, int height, std::uint32_t format) {
    int block_bytes = 8;
    void (*decoder) (const void*, void*, int) = bcdec_bc1;

    switch (format) {
        case format_dxt1:
            block_bytes = 8;
            decoder = bcdec_bc1;
            break;
        case format_dxt3:
            block_bytes = 16;
            decoder = bcdec_bc2;
            break;
        case format_dxt5:
            block_bytes = 16;
            decoder = bcdec_bc3;
            break;
        case format_bc7:
            block_bytes = 16;
            decoder = bcdec_bc7;
            break;
        default:
            return core::fail (core::ErrorCode::unsupported, "unsupported block texture format");
    }

    const int blocks_x = (width + 3) / 4;
    const int blocks_y = (height + 3) / 4;
    const auto needed = static_cast<std::size_t> (blocks_x) * static_cast<std::size_t> (blocks_y)
        * static_cast<std::size_t> (block_bytes);

    if (input.size () < needed) {
        return core::fail (core::ErrorCode::malformed_data, "compressed texture is shorter than its block grid");
    }

    std::vector<std::uint8_t> output (static_cast<std::size_t> (width) * static_cast<std::size_t> (height) * 4U);
    std::array<std::uint8_t, 64> block {};

    for (int block_y = 0; block_y < blocks_y; ++block_y) {
        for (int block_x = 0; block_x < blocks_x; ++block_x) {
            const auto index = (static_cast<std::size_t> (block_y) * static_cast<std::size_t> (blocks_x)
                                + static_cast<std::size_t> (block_x))
                * static_cast<std::size_t> (block_bytes);
            decoder (input.data () + index, block.data (), 16);

            for (int row = 0; row < 4; ++row) {
                const int y = block_y * 4 + row;

                if (y >= height) {
                    break;
                }

                for (int column = 0; column < 4; ++column) {
                    const int x = block_x * 4 + column;

                    if (x >= width) {
                        break;
                    }

                    const auto source = (static_cast<std::size_t> (row) * 4U + static_cast<std::size_t> (column)) * 4U;
                    const auto target = (static_cast<std::size_t> (y) * static_cast<std::size_t> (width)
                                         + static_cast<std::size_t> (x))
                        * 4U;
                    std::copy_n (block.data () + source, 4, output.data () + target);
                }
            }
        }
    }

    return output;
}

std::expected<std::vector<std::uint8_t>, core::Error>
decode_uncompressed (std::span<const std::byte> input, int width, int height, std::uint32_t format) {
    const auto pixels = static_cast<std::size_t> (width) * static_cast<std::size_t> (height);
    std::vector<std::uint8_t> output (pixels * 4U);
    const auto* source = reinterpret_cast<const std::uint8_t*> (input.data ());

    switch (format) {
        case format_argb8888: {
            if (input.size () < pixels * 4U) {
                return core::fail (core::ErrorCode::malformed_data, "ARGB texture is truncated");
            }

            for (std::size_t index = 0; index < pixels; ++index) {
                output[index * 4U + 0] = source[index * 4U + 2];
                output[index * 4U + 1] = source[index * 4U + 1];
                output[index * 4U + 2] = source[index * 4U + 0];
                output[index * 4U + 3] = source[index * 4U + 3];
            }
            break;
        }
        case format_rgb888: {
            if (input.size () < pixels * 3U) {
                return core::fail (core::ErrorCode::malformed_data, "RGB texture is truncated");
            }

            for (std::size_t index = 0; index < pixels; ++index) {
                output[index * 4U + 0] = source[index * 3U + 2];
                output[index * 4U + 1] = source[index * 3U + 1];
                output[index * 4U + 2] = source[index * 3U + 0];
                output[index * 4U + 3] = 255;
            }
            break;
        }
        case format_rgb565: {
            if (input.size () < pixels * 2U) {
                return core::fail (core::ErrorCode::malformed_data, "RGB565 texture is truncated");
            }

            for (std::size_t index = 0; index < pixels; ++index) {
                const std::uint16_t value = read_u16 (source + index * 2U);
                const auto r = static_cast<std::uint8_t> (((value >> 11) & 0x1FU) * 255U / 31U);
                const auto g = static_cast<std::uint8_t> (((value >> 5) & 0x3FU) * 255U / 63U);
                const auto b = static_cast<std::uint8_t> ((value & 0x1FU) * 255U / 31U);
                output[index * 4U + 0] = r;
                output[index * 4U + 1] = g;
                output[index * 4U + 2] = b;
                output[index * 4U + 3] = 255;
            }
            break;
        }
        case format_r8: {
            if (input.size () < pixels) {
                return core::fail (core::ErrorCode::malformed_data, "R8 texture is truncated");
            }

            for (std::size_t index = 0; index < pixels; ++index) {
                output[index * 4U + 0] = source[index];
                output[index * 4U + 1] = source[index];
                output[index * 4U + 2] = source[index];
                output[index * 4U + 3] = 255;
            }
            break;
        }
        case format_rg88: {
            if (input.size () < pixels * 2U) {
                return core::fail (core::ErrorCode::malformed_data, "RG88 texture is truncated");
            }

            for (std::size_t index = 0; index < pixels; ++index) {
                output[index * 4U + 0] = source[index * 2U + 0];
                output[index * 4U + 1] = source[index * 2U + 1];
                output[index * 4U + 2] = 0;
                output[index * 4U + 3] = 255;
            }
            break;
        }
        default:
            return core::fail (
                core::ErrorCode::unsupported,
                "unsupported uncompressed texture format " + std::to_string (format)
            );
    }

    return output;
}

} // namespace

std::expected<TextureData, core::Error> decode_texture (std::span<const std::byte> data) {
    Cursor cursor (data);

    const std::string header = cursor.magic9 ();

    // The on-disk magics are nine bytes: eight characters plus a NUL.
    if (!header.starts_with ("TEXV0005")) {
        return core::fail (core::ErrorCode::malformed_data, "texture does not start with TEXV0005");
    }

    if (!cursor.magic9 ().starts_with ("TEXI0001")) {
        return core::fail (core::ErrorCode::malformed_data, "texture has no TEXI0001 section");
    }

    TextureData texture;
    texture.format = cursor.u32 ();
    texture.flags = cursor.u32 ();
    texture.texture_width = static_cast<int> (cursor.u32 ());
    texture.texture_height = static_cast<int> (cursor.u32 ());
    texture.width = static_cast<int> (cursor.u32 ());
    texture.height = static_cast<int> (cursor.u32 ());
    (void)cursor.u32 ();

    if (!cursor.good ()) {
        return core::fail (core::ErrorCode::malformed_data, "texture header is truncated");
    }

    const std::string container = cursor.magic9 ();
    std::uint32_t container_version = 0;

    if (container.starts_with ("TEXB0001")) {
        container_version = 1;
    } else if (container.starts_with ("TEXB0002")) {
        container_version = 2;
    } else if (container.starts_with ("TEXB0003")) {
        container_version = 3;
    } else if (container.starts_with ("TEXB0004")) {
        container_version = 4;
    } else {
        return core::fail (core::ErrorCode::malformed_data, "unknown texture container '" + container + "'");
    }

    const std::uint32_t image_count = cursor.u32 ();

    std::uint32_t free_image_format = 0;

    if (container_version >= 3) {
        free_image_format = cursor.u32 ();
    }

    if (container_version == 4) {
        const std::uint32_t is_video = cursor.u32 ();

        if (is_video == 1) {
            return core::fail (core::ErrorCode::unsupported, "MP4 textures are not supported");
        }

        // Non-video TEXB0004 payloads use the TEXB0003 mipmap layout.
        container_version = 3;
    }

    if (image_count == 0) {
        return core::fail (core::ErrorCode::malformed_data, "texture contains no images");
    }

    bool have_first = false;
    std::vector<std::uint8_t> first_mipmap;
    std::uint32_t mip_width = 0;
    std::uint32_t mip_height = 0;

    for (std::uint32_t image = 0; image < image_count; ++image) {
        const std::uint32_t mipmap_count = cursor.u32 ();

        if (mipmap_count == 0 || mipmap_count > 64) {
            return core::fail (core::ErrorCode::malformed_data, "texture has an invalid mipmap count");
        }

        for (std::uint32_t mipmap = 0; mipmap < mipmap_count; ++mipmap) {
            if (container_version == 4) {
                (void)cursor.u32 ();
                (void)cursor.u32 ();
                (void)cursor.cstring ();
                (void)cursor.u32 ();
            }

            const std::uint32_t width = cursor.u32 ();
            const std::uint32_t height = cursor.u32 ();

            std::uint32_t current_compression = 0;
            std::int32_t uncompressed_size = 0;

            if (container_version >= 2) {
                current_compression = cursor.u32 ();
                uncompressed_size = cursor.i32 ();
            }

            const std::int32_t compressed_size = cursor.i32 ();

            if (!cursor.good ()) {
                return core::fail (core::ErrorCode::malformed_data, "texture mipmap header is truncated");
            }

            if (current_compression == 0) {
                uncompressed_size = compressed_size;
            }

            if (compressed_size < 0 || uncompressed_size < 0) {
                return core::fail (core::ErrorCode::malformed_data, "texture mipmap has a negative size");
            }

            const auto payload = cursor.bytes (static_cast<std::size_t> (compressed_size));

            if (!cursor.good ()) {
                return core::fail (core::ErrorCode::malformed_data, "texture mipmap data is truncated");
            }

            if (!have_first) {
                if (current_compression == 0) {
                    first_mipmap.assign (
                        reinterpret_cast<const std::uint8_t*> (payload.data ()),
                        reinterpret_cast<const std::uint8_t*> (payload.data ()) + payload.size ()
                    );
                } else if (current_compression == 1) {
                    auto decompressed = core::lz4::decompress_block (
                        payload, static_cast<std::size_t> (uncompressed_size)
                    );

                    if (!decompressed) {
                        return std::unexpected (std::move (decompressed.error ()));
                    }

                    first_mipmap.resize (decompressed->size ());
                    std::memcpy (first_mipmap.data (), decompressed->data (), decompressed->size ());
                } else {
                    return core::fail (core::ErrorCode::unsupported, "unknown texture compression method");
                }

                mip_width = width;
                mip_height = height;
                have_first = true;
            }
        }
    }

    if (!have_first) {
        return core::fail (core::ErrorCode::malformed_data, "texture contains no usable mipmap");
    }

    if (texture.width <= 0 || texture.height <= 0) {
        texture.width = static_cast<int> (mip_width);
        texture.height = static_cast<int> (mip_height);
    }

    ensure_rgba (texture);

    if (texture.width <= 0 || texture.height <= 0) {
        return core::fail (core::ErrorCode::malformed_data, "texture has invalid dimensions");
    }

    const auto span = std::span<const std::byte> (
        reinterpret_cast<const std::byte*> (first_mipmap.data ()), first_mipmap.size ()
    );

    // Video textures embed an MP4 stream (ISO base media 'ftyp' box at byte 4).
    if (first_mipmap.size () > 8 && std::memcmp (first_mipmap.data () + 4, "ftyp", 4) == 0) {
        texture.is_video = true;
        texture.video = std::move (first_mipmap);
        return texture;
    }

    // Free-image format 13 (FIF_PNG) stores the mipmap as an embedded PNG.
    if (free_image_format == format_free_image_png) {
        auto image = decode_image (span);

        if (!image) {
            return std::unexpected (std::move (image.error ()));
        }

        texture.width = image->width;
        texture.height = image->height;
        texture.rgba = std::move (image->rgba);
        ensure_rgba (texture);
        return texture;
    }

    auto rgba = decode_uncompressed (span, texture.width, texture.height, texture.format);

    if (!rgba && rgba.error ().code == core::ErrorCode::unsupported) {
        rgba = decode_blocks (span, texture.width, texture.height, texture.format);
    }

    if (!rgba) {
        return std::unexpected (std::move (rgba.error ()));
    }

    texture.rgba = std::move (*rgba);

    // Optional sprite-sheet / GIF animation metadata. Playback of the frames is
    // handled by the scene layer; only the timing is needed here.
    if (cursor.remaining () >= 9) {
        const std::string animation = cursor.magic9 ();

        if (animation.starts_with ("TEXS")) {
            const std::uint32_t frame_count = cursor.u32 ();

            if (animation == "TEXS0003") {
                (void)cursor.u32 ();
                (void)cursor.u32 ();
            }

            texture.animated = true;
            texture.frame_count = static_cast<int> (frame_count);

            for (std::uint32_t frame = 0; frame < frame_count; ++frame) {
                (void)cursor.u32 ();

                if (animation == "TEXS0001") {
                    (void)cursor.u32 ();
                    (void)cursor.u32 ();
                    (void)cursor.u32 ();
                    (void)cursor.u32 ();
                    (void)cursor.u32 ();
                    (void)cursor.u32 ();
                    (void)cursor.u32 ();
                } else {
                    texture.duration += cursor.f32 ();
                    (void)cursor.f32 ();
                    (void)cursor.f32 ();
                    (void)cursor.f32 ();
                    (void)cursor.f32 ();
                    (void)cursor.f32 ();
                    (void)cursor.f32 ();
                }
            }
        }
    }

    return texture;
}

std::expected<TextureData, core::Error> decode_image (std::span<const std::byte> data) {
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load_from_memory (
        reinterpret_cast<const stbi_uc*> (data.data ()), static_cast<int> (data.size ()), &width, &height, &channels, 4
    );

    if (pixels == nullptr) {
        return core::fail (core::ErrorCode::malformed_data, "unsupported or corrupt image file");
    }

    TextureData texture;
    texture.width = width;
    texture.height = height;
    texture.texture_width = width;
    texture.texture_height = height;
    texture.rgba.assign (pixels, pixels + static_cast<std::size_t> (width) * static_cast<std::size_t> (height) * 4U);
    stbi_image_free (pixels);

    return texture;
}

std::expected<gfx::Texture, core::Error> upload_texture (const TextureData& texture) {
    gfx::TextureOptions options;
    options.mipmaps = true;
    options.clamp_to_edge = (texture.flags & (2U | 8U)) != 0;
    options.nearest = (texture.flags & 1U) != 0;

    return gfx::Texture::from_rgba8 (
        texture.width, texture.height, texture.rgba, options
    );
}

std::expected<TextureData, core::Error> load_texture_data (const AssetContainer& assets, std::string_view path) {
    // Scene materials name textures bare (e.g. "adiyogi-lord-shiva-..."); the
    // original engine resolves them to materials/<name>.tex (AssetLocator::texture).
    std::string resolved (path);

    if (!path.ends_with (".tex") && !std::filesystem::path (path).has_extension ()) {
        resolved = "materials/" + std::string (path) + ".tex";
    }

    auto bytes = assets.read (resolved);

    if (!bytes) {
        return std::unexpected (std::move (bytes.error ()));
    }

    const auto data = std::span<const std::byte> (bytes->data (), bytes->size ());

    if (resolved.ends_with (".tex")) {
        return decode_texture (data);
    }

    return decode_image (data);
}

} // namespace we::scene
