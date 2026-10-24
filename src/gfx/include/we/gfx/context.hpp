#pragma once

#include <EGL/egl.h>

#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string_view>

#include "we/core/error.hpp"

namespace we::gfx {

// A GL context plus the EGL display it belongs to. The context is always
// created together with a 1x1 pbuffer so it can be made current before any
// window exists (texture upload, shader warm-up, headless rendering).
class Surface;

struct ContextConfig {
    bool depth = true;
    bool stencil = false;
    bool debug = false;
};

class Context {
public:
    Context () noexcept = default;
    ~Context ();

    Context (const Context&) = delete;
    Context& operator= (const Context&) = delete;
    Context (Context&& other) noexcept;
    Context& operator= (Context&& other) noexcept;

    // `native_display` is an X11 `Display*`.
    [[nodiscard]] static std::expected<Context, core::Error>
    create_x11 (void* native_display, const ContextConfig& config = {});

    // `native_display` is a `wl_display*`.
    [[nodiscard]] static std::expected<Context, core::Error>
    create_wayland (void* native_display, const ContextConfig& config = {});

    // EGL surfaceless / device platform, used by tests and `--dry-run`.
    [[nodiscard]] static std::expected<Context, core::Error> create_headless (const ContextConfig& config = {});

    // `native_window` is an X11 `Window` or a `wl_egl_window*`.
    [[nodiscard]] std::expected<Surface, core::Error>
    create_window_surface (void* native_window, int width, int height) const;

    [[nodiscard]] std::expected<Surface, core::Error> create_pbuffer (int width, int height) const;

    // Passing nullptr binds the internal 1x1 pbuffer.
    void make_current (const Surface* surface) const;
    void release_current () const;

    void set_swap_interval (int interval) const;

    [[nodiscard]] bool valid () const noexcept { return this->impl_ != nullptr; }
    [[nodiscard]] EGLDisplay egl_display () const noexcept;
    [[nodiscard]] EGLConfig egl_config () const noexcept;

    [[nodiscard]] std::string_view renderer () const;
    [[nodiscard]] std::string_view version () const;

private:
    struct Impl;
    explicit Context (std::unique_ptr<Impl> impl) noexcept;

    [[nodiscard]] static std::expected<Context, core::Error>
    create_with (EGLDisplay display, EGLConfig config, bool headless, const ContextConfig& options);

    std::unique_ptr<Impl> impl_;
};

class Surface {
public:
    Surface () noexcept = default;
    ~Surface ();

    Surface (const Surface&) = delete;
    Surface& operator= (const Surface&) = delete;
    Surface (Surface&& other) noexcept;
    Surface& operator= (Surface&& other) noexcept;

    [[nodiscard]] bool valid () const noexcept { return this->surface_ != EGL_NO_SURFACE; }
    [[nodiscard]] EGLSurface handle () const noexcept { return this->surface_; }
    [[nodiscard]] int width () const noexcept { return this->width_; }
    [[nodiscard]] int height () const noexcept { return this->height_; }
    [[nodiscard]] bool offscreen () const noexcept { return this->offscreen_; }

    void resize (int width, int height) noexcept;

    void swap () const;

    // Reads the current draw buffer into tightly packed RGBA8 rows, top row
    // first. Only meaningful while this surface is current.
    void read_rgba (std::span<std::uint8_t> pixels, int width, int height) const;

private:
    friend class Context;

    Surface (EGLDisplay display, EGLSurface surface, int width, int height, bool offscreen) noexcept;

    void destroy () noexcept;

    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLSurface surface_ = EGL_NO_SURFACE;
    int width_ = 0;
    int height_ = 0;
    bool offscreen_ = false;
};

} // namespace we::gfx
