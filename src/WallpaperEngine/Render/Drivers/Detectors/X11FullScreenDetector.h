#pragma once

#include <glm/vec4.hpp>
#include <string>
#include <vector>

#include "FullScreenDetector.h"
#include "WallpaperEngine/Render/Drivers/VideoDriver.h"
#include <X11/Xlib.h>

namespace WallpaperEngine::Render::Drivers {
class GLFWOpenGLDriver;

namespace Detectors {
    class X11FullScreenDetector final : public FullScreenDetector {
    public:
	X11FullScreenDetector (Application::ApplicationContext& appContext, VideoDriver& driver);
	~X11FullScreenDetector () override;

	[[nodiscard]] bool anythingFullscreen () const override;
	void reset () override;

    private:
	void initialize ();
	void stop ();
	/**
	 * EWMH check: true if the window advertises _NET_WM_STATE_FULLSCREEN.
	 * Authoritative for real fullscreen clients (games, browsers, DWM fullscreen),
	 * unlike the geometry comparison below which also matches maximized/tiled windows.
	 */
	[[nodiscard]] bool hasFullscreenState (Window window) const;

	Display* m_display = nullptr;
	Window m_root;
	Atom m_netWmState = None;
	Atom m_netWmStateFullscreen = None;
	std::map<std::string, glm::ivec4> m_screens = {};
	VideoDriver& m_driver;
    };
} // namespace Detectors
} // namespace WallpaperEngine::Render::Drivers