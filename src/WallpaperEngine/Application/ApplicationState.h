#pragma once

#include "ApplicationContext.h"

namespace WallpaperEngine::Application {
/**
 * Represents current application state
 */
class ApplicationState {
public:
    struct {
	bool keepRunning;
    } general {};

    struct {
	/** Set by WallpaperApplication while running on battery (battery feature enabled) */
	bool batteryActive = false;
	/** FPS cap from the active playlist (-1 = none), applied in effectiveMaximumFPS */
	int playlistFps = -1;
    } render {};

    struct {
	bool enabled;
	int volume;
    } audio {};

    struct {
	bool enabled;
    } mouse {};
};
} // namespace WallpaperEngine::Application