#include "we/gfx/context.hpp"

#include "we/core/log.hpp"
#include "we/gfx/gl.hpp"

#include <EGL/eglext.h>

#include <algorithm>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace we::gfx {
namespace {

// EGL displays are process-global: several Context instances (the renderer,
// a video player, a test) may share one EGLDisplay. Terminating it while other
// contexts are alive would invalidate them, so the display is reference
// counted and only terminated once the last user is gone. This also lets the
// driver release its per-display caches instead of leaking them at exit.
std::mutex display_mutex;
std::unordered_map<EGLDisplay, std::size_t> display_users;

void retain_display (EGLDisplay display) {
    const std::lock_guard lock (display_mutex);
    ++display_users[display];
}

void release_display (EGLDisplay display) noexcept {
    bool terminate = false;

    {
        const std::lock_guard lock (display_mutex);
        const auto entry = display_users.find (display);

        if (entry == display_users.end ()) {
            return;
        }

        if (--entry->second == 0) {
            display_users.erase (entry);
            terminate = true;
        }
    }

    if (terminate) {
        eglReleaseThread ();
        eglTerminate (display);
    }
}

std::string egl_error_text (const char* operation) {
    const EGLint code = eglGetError ();
    const char* name = "EGL_UNKNOWN";

    switch (code) {
        case EGL_SUCCESS:
            name = "EGL_SUCCESS";
            break;
        case EGL_NOT_INITIALIZED:
            name = "EGL_NOT_INITIALIZED";
            break;
        case EGL_BAD_ACCESS:
            name = "EGL_BAD_ACCESS";
            break;
        case EGL_BAD_ALLOC:
            name = "EGL_BAD_ALLOC";
            break;
        case EGL_BAD_ATTRIBUTE:
            name = "EGL_BAD_ATTRIBUTE";
            break;
        case EGL_BAD_CONFIG:
            name = "EGL_BAD_CONFIG";
            break;
        case EGL_BAD_CONTEXT:
            name = "EGL_BAD_CONTEXT";
            break;
        case EGL_BAD_CURRENT_SURFACE:
            name = "EGL_BAD_CURRENT_SURFACE";
            break;
        case EGL_BAD_DISPLAY:
            name = "EGL_BAD_DISPLAY";
            break;
        case EGL_BAD_MATCH:
            name = "EGL_BAD_MATCH";
            break;
            case EGL_BAD_NATIVE_WINDOW:
            name = "EGL_BAD_NATIVE_WINDOW";
            break;
        case EGL_BAD_PARAMETER:
            name = "EGL_BAD_PARAMETER";
            break;
        case EGL_BAD_SURFACE:
            name = "EGL_BAD_SURFACE";
            break;
        case EGL_CONTEXT_LOST:
            name = "EGL_CONTEXT_LOST";
            break;
        default:
            break;
    }

    return std::string (operation) + " failed (" + name + ")";
}

struct DisplayHandle {
    EGLDisplay display = EGL_NO_DISPLAY;
    bool initialized = false;
};

DisplayHandle open_platform_display (EGLenum platform, void* native_display) {
    DisplayHandle handle;
    using GetPlatformDisplay = EGLDisplay (*) (EGLenum, void*, const EGLint*);
    const auto get_platform_display
        = reinterpret_cast<GetPlatformDisplay> (eglGetProcAddress ("eglGetPlatformDisplay"));

    if (get_platform_display != nullptr) {
        handle.display = get_platform_display (platform, native_display, nullptr);
    } else {
        const auto get_platform_display_ext
            = reinterpret_cast<GetPlatformDisplay> (eglGetProcAddress ("eglGetPlatformDisplayEXT"));

        if (get_platform_display_ext != nullptr) {
            handle.display = get_platform_display_ext (platform, native_display, nullptr);
        } else {
            handle.display = eglGetDisplay (reinterpret_cast<EGLNativeDisplayType> (native_display));
        }
    }

    if (handle.display == EGL_NO_DISPLAY) {
        return handle;
    }

    EGLint major = 0;
    EGLint minor = 0;

    if (eglInitialize (handle.display, &major, &minor) == EGL_TRUE) {
        handle.initialized = true;
        core::log_debug ("EGL ", major, ".", minor, " initialized");
    }

    return handle;
}

std::expected<EGLConfig, core::Error> choose_config (EGLDisplay display, const ContextConfig& config,
                                                      bool windowed) {
    EGLint surface_type = windowed ? (EGL_WINDOW_BIT | EGL_PBUFFER_BIT) : EGL_PBUFFER_BIT;

    std::vector<EGLint> attributes = {
        EGL_SURFACE_TYPE,
        surface_type,
        EGL_RENDERABLE_TYPE,
        EGL_OPENGL_BIT,
        EGL_RED_SIZE,
        8,
        EGL_GREEN_SIZE,
        8,
        EGL_BLUE_SIZE,
        8,
        EGL_ALPHA_SIZE,
        8,
        EGL_NONE,
    };

    if (config.depth) {
        const EGLint depth_attributes[] = { EGL_DEPTH_SIZE, 24, EGL_NONE };
        attributes.insert (attributes.end () - 1, depth_attributes, depth_attributes + 3);
    }

    if (config.stencil) {
        const EGLint stencil_attributes[] = { EGL_STENCIL_SIZE, 8, EGL_NONE };
        attributes.insert (attributes.end () - 1, stencil_attributes, stencil_attributes + 3);
    }

    EGLConfig selected = nullptr;
    EGLint count = 0;

    if (eglChooseConfig (display, attributes.data (), &selected, 1, &count) != EGL_TRUE || count == 0) {
        return core::fail (core::ErrorCode::unsupported, egl_error_text ("eglChooseConfig (OpenGL RGBA8)"));
    }

    return selected;
}

std::expected<EGLContext, core::Error> create_context (EGLDisplay display, EGLConfig config,
                                                       const ContextConfig& options) {
    eglBindAPI (EGL_OPENGL_API);

    std::vector<EGLint> attributes = { EGL_CONTEXT_MAJOR_VERSION,
                                       3,
                                       EGL_CONTEXT_MINOR_VERSION,
                                       3,
                                       EGL_CONTEXT_OPENGL_PROFILE_MASK,
                                       EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
                                       EGL_NONE };

    if (options.debug) {
        const EGLint debug_attributes[] = { EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR, EGL_NONE };
        attributes.insert (attributes.end () - 1, debug_attributes, debug_attributes + 3);
    }

    EGLContext context = eglCreateContext (display, config, EGL_NO_CONTEXT, attributes.data ());

    if (context == EGL_NO_CONTEXT) {
        // Some drivers only expose a 3.2+ core profile. Retry without an
        // explicit version before giving up.
        const EGLint fallback[] = { EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE };
        context = eglCreateContext (display, config, EGL_NO_CONTEXT, fallback);
    }

    if (context == EGL_NO_CONTEXT) {
        const EGLint compatibility[] = { EGL_NONE };
        context = eglCreateContext (display, config, EGL_NO_CONTEXT, compatibility);
    }

    if (context == EGL_NO_CONTEXT) {
        return core::fail (core::ErrorCode::unsupported, egl_error_text ("eglCreateContext"));
    }

    return context;
}

} // namespace

struct Context::Impl {
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLContext context = EGL_NO_CONTEXT;
    EGLConfig config = nullptr;
    EGLSurface scratch = EGL_NO_SURFACE;
    bool headless = false;
    std::string renderer = "unknown";
    std::string version = "unknown";

    ~Impl () {
        if (this->display == EGL_NO_DISPLAY) {
            return;
        }

        eglMakeCurrent (this->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

        if (this->scratch != EGL_NO_SURFACE) {
            eglDestroySurface (this->display, this->scratch);
        }

        if (this->context != EGL_NO_CONTEXT) {
            eglDestroyContext (this->display, this->context);
        }

        release_display (this->display);
    }
};

Context::Context (std::unique_ptr<Impl> impl) noexcept : impl_ (std::move (impl)) { }

Context::~Context () = default;
Context::Context (Context&& other) noexcept = default;
Context& Context::operator= (Context&& other) noexcept = default;

std::expected<Context, core::Error>
Context::create_with (EGLDisplay display, EGLConfig config, bool headless, const ContextConfig& options) {
    retain_display (display);

    auto context = create_context (display, config, options);

    if (!context) {
        release_display (display);
        return std::unexpected (std::move (context.error ()));
    }

    auto impl = std::make_unique<Context::Impl> ();
    impl->display = display;
    impl->context = *context;
    impl->config = config;
    impl->headless = headless;

    const EGLint pbuffer_attributes[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
    impl->scratch = eglCreatePbufferSurface (display, config, pbuffer_attributes);

    if (impl->scratch == EGL_NO_SURFACE) {
        eglDestroyContext (display, impl->context);
        release_display (display);
        return core::fail (core::ErrorCode::unsupported, egl_error_text ("eglCreatePbufferSurface (scratch)"));
    }

    if (eglMakeCurrent (display, impl->scratch, impl->scratch, impl->context) != EGL_TRUE) {
        eglDestroySurface (display, impl->scratch);
        eglDestroyContext (display, impl->context);
        release_display (display);
        return core::fail (core::ErrorCode::unsupported, egl_error_text ("eglMakeCurrent (scratch)"));
    }

    const auto* version = reinterpret_cast<const char*> (glGetString (GL_VERSION));
    const auto* renderer = reinterpret_cast<const char*> (glGetString (GL_RENDERER));
    impl->version = version != nullptr ? version : "unknown";
    impl->renderer = renderer != nullptr ? renderer : "unknown";
    core::log_debug ("OpenGL ", impl->version, " / ", impl->renderer);

    eglMakeCurrent (display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    return Context (std::move (impl));
}

std::expected<Context, core::Error> Context::create_x11 (void* native_display, const ContextConfig& config) {
    const auto handle = open_platform_display (EGL_PLATFORM_X11_KHR, native_display);

    if (!handle.initialized) {
        return core::fail (core::ErrorCode::unsupported, egl_error_text ("EGL X11 display initialization"));
    }

    auto selected = choose_config (handle.display, config, true);

    if (!selected) {
        return std::unexpected (std::move (selected.error ()));
    }

    return Context::create_with (handle.display, *selected, false, config);
}

std::expected<Context, core::Error> Context::create_wayland (void* native_display, const ContextConfig& config) {
    const auto handle = open_platform_display (EGL_PLATFORM_WAYLAND_KHR, native_display);

    if (!handle.initialized) {
        return core::fail (core::ErrorCode::unsupported, egl_error_text ("EGL Wayland display initialization"));
    }

    auto selected = choose_config (handle.display, config, true);

    if (!selected) {
        return std::unexpected (std::move (selected.error ()));
    }

    return Context::create_with (handle.display, *selected, false, config);
}

std::expected<Context, core::Error> Context::create_headless (const ContextConfig& config) {
    const auto handle = open_platform_display (EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY);

    if (!handle.initialized) {
        return core::fail (core::ErrorCode::unsupported, egl_error_text ("EGL surfaceless display initialization"));
    }

    auto selected = choose_config (handle.display, config, false);

    if (!selected) {
        return std::unexpected (std::move (selected.error ()));
    }

    return Context::create_with (handle.display, *selected, true, config);
}

std::expected<Surface, core::Error>
Context::create_window_surface (void* native_window, int width, int height) const {
    if (!this->valid ()) {
        return core::fail (core::ErrorCode::invalid_argument, "cannot create a surface without a GL context");
    }

    const EGLNativeWindowType native = reinterpret_cast<EGLNativeWindowType> (native_window);
    EGLSurface surface = eglCreateWindowSurface (this->impl_->display, this->impl_->config, native, nullptr);

    if (surface == EGL_NO_SURFACE) {
        return core::fail (core::ErrorCode::unsupported, egl_error_text ("eglCreateWindowSurface"));
    }

    return Surface (this->impl_->display, surface, width, height, false);
}

std::expected<Surface, core::Error> Context::create_pbuffer (int width, int height) const {
    if (!this->valid ()) {
        return core::fail (core::ErrorCode::invalid_argument, "cannot create a surface without a GL context");
    }

    const EGLint attributes[] = { EGL_WIDTH, width, EGL_HEIGHT, height, EGL_NONE };
    EGLSurface surface = eglCreatePbufferSurface (this->impl_->display, this->impl_->config, attributes);

    if (surface == EGL_NO_SURFACE) {
        return core::fail (core::ErrorCode::unsupported, egl_error_text ("eglCreatePbufferSurface"));
    }

    return Surface (this->impl_->display, surface, width, height, true);
}

void Context::make_current (const Surface* surface) const {
    if (!this->valid ()) {
        return;
    }

    const EGLSurface draw = surface != nullptr ? surface->surface_ : this->impl_->scratch;

    if (eglMakeCurrent (this->impl_->display, draw, draw, this->impl_->context) != EGL_TRUE) {
        core::log_error (egl_error_text ("eglMakeCurrent"));
    }
}

void Context::release_current () const {
    if (!this->valid ()) {
        return;
    }

    eglMakeCurrent (this->impl_->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

void Context::set_swap_interval (int interval) const {
    if (this->valid ()) {
        eglSwapInterval (this->impl_->display, interval);
    }
}

EGLDisplay Context::egl_display () const noexcept {
    return this->valid () ? this->impl_->display : EGL_NO_DISPLAY;
}

EGLConfig Context::egl_config () const noexcept {
    return this->valid () ? this->impl_->config : nullptr;
}

std::string_view Context::renderer () const {
    if (!this->valid ()) {
        return "none";
    }

    return this->impl_->renderer;
}

std::string_view Context::version () const {
    if (!this->valid ()) {
        return "none";
    }

    return this->impl_->version;
}

Surface::Surface (EGLDisplay display, EGLSurface surface, int width, int height, bool offscreen) noexcept :
    display_ (display), surface_ (surface), width_ (width), height_ (height), offscreen_ (offscreen) { }

Surface::~Surface () { this->destroy (); }

Surface::Surface (Surface&& other) noexcept :
    display_ (other.display_), surface_ (other.surface_), width_ (other.width_), height_ (other.height_),
    offscreen_ (other.offscreen_) {
    other.display_ = EGL_NO_DISPLAY;
    other.surface_ = EGL_NO_SURFACE;
    other.width_ = 0;
    other.height_ = 0;
}

Surface& Surface::operator= (Surface&& other) noexcept {
    if (this != &other) {
        this->destroy ();
        this->display_ = other.display_;
        this->surface_ = other.surface_;
        this->width_ = other.width_;
        this->height_ = other.height_;
        this->offscreen_ = other.offscreen_;
        other.display_ = EGL_NO_DISPLAY;
        other.surface_ = EGL_NO_SURFACE;
        other.width_ = 0;
        other.height_ = 0;
    }

    return *this;
}

void Surface::destroy () noexcept {
    if (this->surface_ != EGL_NO_SURFACE && this->display_ != EGL_NO_DISPLAY) {
        eglDestroySurface (this->display_, this->surface_);
    }

    this->surface_ = EGL_NO_SURFACE;
    this->display_ = EGL_NO_DISPLAY;
}

void Surface::resize (int width, int height) noexcept {
    this->width_ = std::max (width, 0);
    this->height_ = std::max (height, 0);
}

void Surface::swap () const {
    if (this->surface_ != EGL_NO_SURFACE) {
        eglSwapBuffers (this->display_, this->surface_);
    }
}

void Surface::read_rgba (std::span<std::uint8_t> pixels, int width, int height) const {
    if (pixels.size () < static_cast<std::size_t> (width) * static_cast<std::size_t> (height) * 4U) {
        return;
    }

    glPixelStorei (GL_PACK_ALIGNMENT, 1);
    glReadPixels (0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data ());
    glPixelStorei (GL_PACK_ALIGNMENT, 4);

    // OpenGL returns the bottom row first; flip into the top-down layout used
    // by X11 images and image writers.
    const std::size_t row_bytes = static_cast<std::size_t> (width) * 4U;
    std::vector<std::uint8_t> scratch (row_bytes);

    for (int row = 0; row < height / 2; ++row) {
        auto* top = pixels.data () + static_cast<std::size_t> (row) * row_bytes;
        auto* bottom = pixels.data () + static_cast<std::size_t> (height - 1 - row) * row_bytes;
        std::memcpy (scratch.data (), top, row_bytes);
        std::memcpy (top, bottom, row_bytes);
        std::memcpy (bottom, scratch.data (), row_bytes);
    }
}

} // namespace we::gfx
