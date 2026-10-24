#include "we/gfx/texture.hpp"

#include <algorithm>
#include <vector>

namespace we::gfx {

Texture::~Texture () { this->destroy (); }

Texture::Texture (Texture&& other) noexcept :
    id_ (other.id_), width_ (other.width_), height_ (other.height_), owned_ (other.owned_) {
    other.id_ = 0;
    other.width_ = 0;
    other.height_ = 0;
    other.owned_ = true;
}

Texture& Texture::operator= (Texture&& other) noexcept {
    if (this != &other) {
        this->destroy ();
        this->id_ = other.id_;
        this->width_ = other.width_;
        this->height_ = other.height_;
        this->owned_ = other.owned_;
        other.id_ = 0;
        other.width_ = 0;
        other.height_ = 0;
        other.owned_ = true;
    }

    return *this;
}

void Texture::destroy () noexcept {
    if (this->id_ != 0 && this->owned_) {
        glDeleteTextures (1, &this->id_);
    }

    this->id_ = 0;
}

Texture Texture::view (GLuint id, int width, int height) noexcept {
    Texture result;
    result.id_ = id;
    result.width_ = width;
    result.height_ = height;
    result.owned_ = false;
    return result;
}

std::expected<Texture, core::Error> Texture::from_rgba8 (int width, int height,
                                                         std::span<const std::uint8_t> pixels,
                                                         const TextureOptions& options) {
    if (width <= 0 || height <= 0) {
        return core::fail (core::ErrorCode::invalid_argument, "texture dimensions must be positive");
    }

    const auto expected_size = static_cast<std::size_t> (width) * static_cast<std::size_t> (height) * 4U;

    if (pixels.size () < expected_size) {
        return core::fail (core::ErrorCode::invalid_argument, "texture pixel buffer is too small");
    }

    std::vector<std::uint8_t> flipped;

    if (options.flip_vertical) {
        flipped.resize (expected_size);
        const std::size_t row_bytes = static_cast<std::size_t> (width) * 4U;

        for (int row = 0; row < height; ++row) {
            const auto* source = pixels.data () + static_cast<std::size_t> (height - 1 - row) * row_bytes;
            auto* target = flipped.data () + static_cast<std::size_t> (row) * row_bytes;
            std::copy_n (source, row_bytes, target);
        }

        pixels = flipped;
    }

    Texture result;
    result.width_ = width;
    result.height_ = height;
    glGenTextures (1, &result.id_);
    glBindTexture (GL_TEXTURE_2D, result.id_);
    glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data ());

    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, options.clamp_to_edge ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, options.clamp_to_edge ? GL_CLAMP_TO_EDGE : GL_REPEAT);

    if (options.nearest) {
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    } else {
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                         options.mipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    }

    if (options.mipmaps) {
        glGenerateMipmap (GL_TEXTURE_2D);
    }

    glBindTexture (GL_TEXTURE_2D, 0);
    glPixelStorei (GL_UNPACK_ALIGNMENT, 4);

    if (!check_gl ("Texture::from_rgba8")) {
        return core::fail (core::ErrorCode::io, "OpenGL rejected the texture upload");
    }

    return result;
}

void Texture::bind (int unit) const {
    glActiveTexture (static_cast<GLenum> (GL_TEXTURE0 + static_cast<GLenum> (unit)));
    glBindTexture (GL_TEXTURE_2D, this->id_);
}

} // namespace we::gfx
