#pragma once

#include <X11/Xlib.h>

#include <cstdint>

namespace WallpaperEngine::Render::Drivers::Detectors {
/**
 * X11 input-idle time via MIT-SCREEN-SAVER, loaded with dlopen so systems
 * without libXss keep building and running (idle reads as 0 = feature inert).
 */
class X11IdleDetector {
public:
    X11IdleDetector ();
    ~X11IdleDetector ();

    X11IdleDetector (const X11IdleDetector&) = delete;
    X11IdleDetector& operator= (const X11IdleDetector&) = delete;

    /** Milliseconds since the last input event, 0 when unavailable */
    [[nodiscard]] std::uint64_t idleMilliseconds ();

private:
    using AllocInfoFn = void* (*)();
    using QueryInfoFn = int (*) (Display*, Drawable, void*);

    Display* m_display = nullptr;
    void* m_xss = nullptr;
    void* m_info = nullptr;
    AllocInfoFn m_allocInfo = nullptr;
    QueryInfoFn m_queryInfo = nullptr;
};
} // namespace WallpaperEngine::Render::Drivers::Detectors
