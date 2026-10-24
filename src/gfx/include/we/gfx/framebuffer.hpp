#pragma once

#include <cstdint>
#include <expected>

#include "we/core/error.hpp"
#include "we/gfx/gl.hpp"
#include "we/gfx/texture.hpp"

namespace we::gfx {

enum class FramebufferFormat : std::uint8_t {
    rgba8,
    // Preferred by libmpv: its renderer works in 16-bit float internally and
    // only dithers down when forced to an 8-bit target. The dithering path
    // silently produces a black image on Mesa's llvmpipe, so video uses a
    // float target instead (also avoids banding on real GPUs).
    rgba16f,
};

// Single-colour-attachment framebuffer. Used by libmpv and by scene passes
// that need an offscreen target.
class Framebuffer {
public:
    Framebuffer () noexcept = default;
    ~Framebuffer ();

    Framebuffer (const Framebuffer&) = delete;
    Framebuffer& operator= (const Framebuffer&) = delete;
    Framebuffer (Framebuffer&& other) noexcept;
    Framebuffer& operator= (Framebuffer&& other) noexcept;

    [[nodiscard]] static std::expected<Framebuffer, core::Error>
    create (int width, int height, FramebufferFormat format = FramebufferFormat::rgba8);

    void resize (int width, int height);

    void bind () const;
    static void unbind ();

    [[nodiscard]] GLuint id () const noexcept { return this->framebuffer_; }
    [[nodiscard]] GLuint color_texture () const noexcept { return this->color_; }
    [[nodiscard]] FramebufferFormat format () const noexcept { return this->format_; }
    [[nodiscard]] int width () const noexcept { return this->width_; }
    [[nodiscard]] int height () const noexcept { return this->height_; }
    [[nodiscard]] bool valid () const noexcept { return this->framebuffer_ != 0; }

    // Non-owning texture view; refresh it after resize().
    [[nodiscard]] Texture view () const;

private:
    void destroy () noexcept;
    [[nodiscard]] bool allocate (int width, int height, FramebufferFormat format);

    GLuint framebuffer_ = 0;
    GLuint color_ = 0;
    int width_ = 0;
    int height_ = 0;
    FramebufferFormat format_ = FramebufferFormat::rgba8;
};

} // namespace we::gfx
