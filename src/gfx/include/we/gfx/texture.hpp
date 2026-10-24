#pragma once

#include <cstdint>
#include <expected>
#include <span>

#include "we/core/error.hpp"
#include "we/gfx/gl.hpp"

namespace we::gfx {

struct TextureOptions {
    bool mipmaps = true;
    bool clamp_to_edge = true;
    bool nearest = false;
    bool flip_vertical = false;
};

class Texture {
public:
    Texture () noexcept = default;
    ~Texture ();

    Texture (const Texture&) = delete;
    Texture& operator= (const Texture&) = delete;
    Texture (Texture&& other) noexcept;
    Texture& operator= (Texture&& other) noexcept;

    [[nodiscard]] static std::expected<Texture, core::Error> from_rgba8 (int width, int height,
                                                                         std::span<const std::uint8_t> pixels,
                                                                         const TextureOptions& options = {});

    // Non-owning view of a texture created elsewhere (framebuffers, mpv).
    [[nodiscard]] static Texture view (GLuint id, int width, int height) noexcept;

    [[nodiscard]] bool valid () const noexcept { return this->id_ != 0; }
    [[nodiscard]] bool owned () const noexcept { return this->owned_; }
    [[nodiscard]] GLuint id () const noexcept { return this->id_; }
    [[nodiscard]] int width () const noexcept { return this->width_; }
    [[nodiscard]] int height () const noexcept { return this->height_; }

    void bind (int unit = 0) const;

private:
    void destroy () noexcept;

    GLuint id_ = 0;
    int width_ = 0;
    int height_ = 0;
    bool owned_ = true;
};

} // namespace we::gfx
