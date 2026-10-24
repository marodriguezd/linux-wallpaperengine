#include <array>
#include <cstdint>
#include <print>
#include <span>
#include <vector>

#include <cstdlib>

#include "support/harness.hpp"
#include "we/gfx/context.hpp"
#include "we/gfx/framebuffer.hpp"
#include "we/gfx/gl.hpp"
#include "we/gfx/renderer.hpp"
#include "we/gfx/texture.hpp"

using we::gfx::BlendMode;
using we::gfx::Context;
using we::gfx::Framebuffer;
using we::gfx::FramebufferFormat;
using we::gfx::Renderer;
using we::gfx::Texture;

namespace {

[[nodiscard]] std::uint8_t channel (const std::vector<std::uint8_t>& rgba, int width, int x, int y, int channel) {
    const auto index = (static_cast<std::size_t> (y) * static_cast<std::size_t> (width)
                        + static_cast<std::size_t> (x))
            * 4U
        + static_cast<std::size_t> (channel);
    return rgba[index];
}

} // namespace

TEST (gfx_headless_context_reports_a_renderer) {
    auto context = Context::create_headless ();

    if (!context) {
        std::print (stderr, "headless context error: {}\n", context.error ().message);
    }

    CHECK (context.has_value ());
    CHECK (context->valid ());
    CHECK (!context->version ().empty ());
    CHECK (context->version () != "none");
    CHECK (!context->renderer ().empty ());
}

TEST (gfx_renderer_clears_and_draws_a_quad) {
    auto context = Context::create_headless ();
    CHECK (context.has_value ());

    auto surface = context->create_pbuffer (64, 48);
    CHECK (surface.has_value ());

    context->make_current (&*surface);

    auto renderer = Renderer::create ();
    CHECK (renderer.has_value ());

    renderer->begin (64, 48, we::core::Color { 0.0F, 0.0F, 0.0F, 1.0F });
    renderer->draw_rect (we::core::Rect { 16.0F, 12.0F, 32.0F, 24.0F },
                         we::core::Color { 1.0F, 0.25F, 0.0F, 1.0F });
    renderer->end ();

    std::vector<std::uint8_t> pixels (64U * 48U * 4U);
    surface->read_rgba (pixels, 64, 48);

    CHECK_EQ (channel (pixels, 64, 32, 24, 0), std::uint8_t { 255 });
    CHECK_EQ (channel (pixels, 64, 32, 24, 1), std::uint8_t { 64 });
    CHECK_EQ (channel (pixels, 64, 32, 24, 2), std::uint8_t { 0 });
    CHECK_EQ (channel (pixels, 64, 32, 24, 3), std::uint8_t { 255 });

    // Outside the quad the clear colour is untouched.
    CHECK_EQ (channel (pixels, 64, 2, 2, 0), std::uint8_t { 0 });
    CHECK_EQ (channel (pixels, 64, 2, 2, 3), std::uint8_t { 255 });
    CHECK_EQ (renderer->draw_calls (), 1);
}

TEST (gfx_texture_upload_and_uv_sampling) {
    auto context = Context::create_headless ();
    CHECK (context.has_value ());

    auto surface = context->create_pbuffer (32, 32);
    CHECK (surface.has_value ());
    context->make_current (&*surface);

    const std::array<std::uint8_t, 16> pixels = {
        255, 0,   0,   255, 0,   255, 0,   255,
        0,   0,   255, 255, 255, 255, 255, 255,
    };

    auto texture = Texture::from_rgba8 (
        2, 2, pixels, we::gfx::TextureOptions { .mipmaps = false, .nearest = true });
    CHECK (texture.has_value ());
    CHECK_EQ (texture->width (), 2);
    CHECK_EQ (texture->height (), 2);

    auto renderer = Renderer::create ();
    CHECK (renderer.has_value ());

    renderer->begin (32, 32, we::core::Color { 0.0F, 0.0F, 0.0F, 1.0F });
    renderer->draw (*texture, we::core::Rect { 0.0F, 0.0F, 32.0F, 32.0F },
                    we::core::Rect { 0.0F, 0.0F, 1.0F, 1.0F });
    renderer->end ();

    std::vector<std::uint8_t> output (32U * 32U * 4U);
    surface->read_rgba (output, 32, 32);

    // The top-left texel is red and covers the top-left quadrant.
    CHECK_EQ (channel (output, 32, 4, 4, 0), std::uint8_t { 255 });
    CHECK_EQ (channel (output, 32, 4, 4, 1), std::uint8_t { 0 });

    // The bottom-right texel is white.
    CHECK_EQ (channel (output, 32, 28, 28, 0), std::uint8_t { 255 });
    CHECK_EQ (channel (output, 32, 28, 28, 1), std::uint8_t { 255 });
    CHECK_EQ (channel (output, 32, 28, 28, 2), std::uint8_t { 255 });
}

TEST (gfx_additive_blend_adds_colour) {
    auto context = Context::create_headless ();
    CHECK (context.has_value ());

    auto surface = context->create_pbuffer (16, 16);
    CHECK (surface.has_value ());

    context->make_current (&*surface);

    auto renderer = Renderer::create ();
    CHECK (renderer.has_value ());

    renderer->begin (16, 16, we::core::Color { 0.0F, 0.0F, 0.0F, 1.0F });
    renderer->draw_rect (we::core::Rect { 0.0F, 0.0F, 16.0F, 16.0F },
                         we::core::Color { 0.5F, 0.0F, 0.0F, 1.0F }, BlendMode::additive);
    renderer->draw_rect (we::core::Rect { 0.0F, 0.0F, 16.0F, 16.0F },
                         we::core::Color { 0.5F, 0.0F, 0.0F, 1.0F }, BlendMode::additive);
    renderer->end ();

    std::vector<std::uint8_t> pixels (16U * 16U * 4U);
    surface->read_rgba (pixels, 16, 16);

    CHECK (channel (pixels, 16, 8, 8, 0) >= 250);
}

TEST (gfx_float_framebuffer_renders_and_resizes) {
    auto context = Context::create_headless ();
    CHECK (context.has_value ());

    auto surface = context->create_pbuffer (32, 16);
    CHECK (surface.has_value ());
    context->make_current (&*surface);

    auto framebuffer = Framebuffer::create (32, 16, FramebufferFormat::rgba16f);
    CHECK (framebuffer.has_value ());
    CHECK_EQ (framebuffer->format (), FramebufferFormat::rgba16f);
    CHECK_EQ (framebuffer->width (), 32);

    framebuffer->bind ();
    glViewport (0, 0, 32, 16);
    glClearColor (0.25F, 0.5F, 0.75F, 1.0F);
    glClear (GL_COLOR_BUFFER_BIT);
    glReadBuffer (GL_COLOR_ATTACHMENT0);

    std::vector<std::uint8_t> pixels (32U * 16U * 4U);
    glReadPixels (0, 0, 32, 16, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data ());
    Framebuffer::unbind ();

    CHECK (std::abs (static_cast<int> (channel (pixels, 32, 4, 4, 0)) - 64) <= 1);
    CHECK (std::abs (static_cast<int> (channel (pixels, 32, 4, 4, 1)) - 128) <= 1);
    CHECK (std::abs (static_cast<int> (channel (pixels, 32, 4, 4, 2)) - 191) <= 1);

    framebuffer->resize (64, 32);

    CHECK_EQ (framebuffer->width (), 64);
    CHECK_EQ (framebuffer->format (), FramebufferFormat::rgba16f);
    CHECK (framebuffer->valid ());
}

TEST (gfx_invalid_shader_is_reported) {
    auto context = Context::create_headless ();
    CHECK (context.has_value ());

    auto surface = context->create_pbuffer (1, 1);
    CHECK (surface.has_value ());
    context->make_current (&*surface);

    auto shader = we::gfx::Shader::build ("#version 330 core\nvoid main() { gl_Position = vec4(1.0); }",
                                         "this is not glsl", "broken");
    CHECK (!shader.has_value ());
    CHECK_EQ (shader.error ().code, we::core::ErrorCode::unsupported);
    CHECK (shader.error ().message.find ("shader compilation failed") != std::string::npos);
}
