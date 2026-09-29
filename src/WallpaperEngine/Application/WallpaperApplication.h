#pragma once

#include <atomic>
#include <chrono>
#include <random>

#include "WallpaperEngine/Application/ApplicationContext.h"
#include "WallpaperEngine/Assets/AssetLocator.h"

#include "WallpaperEngine/Render/CWallpaper.h"
#include "WallpaperEngine/Render/Drivers/Detectors/FullScreenDetector.h"
#include "WallpaperEngine/Render/Drivers/Detectors/X11IdleDetector.h"
#include "WallpaperEngine/Render/Drivers/GLFWOpenGLDriver.h"
#include "WallpaperEngine/Render/Drivers/Output/GLFWWindowOutput.h"
#include "WallpaperEngine/Render/RenderContext.h"

#include "WallpaperEngine/Audio/Drivers/SDLAudioDriver.h"

#include "WallpaperEngine/Input/InputContext.h"
#include "WallpaperEngine/WebBrowser/WebBrowserContext.h"

#include "WallpaperEngine/Data/Model/Types.h"
#include "WallpaperEngine/Media/DBusMediaSource.h"
#include "WallpaperEngine/Media/MPRISServer.h"
#include "WallpaperEngine/Media/MediaSource.h"
#include "WallpaperEngine/System/BatteryState.h"

#include <set>

namespace WallpaperEngine::Application {

using namespace WallpaperEngine::Assets;
using namespace WallpaperEngine::Data::Model;
/**
 * Small wrapper class over the actual wallpaper's main application skeleton
 */
class WallpaperApplication {
public:
    explicit WallpaperApplication (ApplicationContext& context);

    /**
     * Prepares the application for rendering.
     */
    void setup ();
    /**
     * Renders a frame of the application.
     */
    void render ();
    /**
     * Cleans up all the resources used by the application.
     */
    static void cleanup ();
    /**
     * Shows the application until it's closed
     */
    void show ();
    /**
     * Handles a OS signal sent to this PID
     *
     * @param signal
     */
    void signal (int signal);
    /**
     * Requests a playlist skip consumed by the render thread (+1 next, -1 prev).
     * Async-signal-safe: only touches an atomic flag (call from SIGUSR1/SIGUSR2).
     */
    void requestPlaylistSkip (int direction);
    /**
     * @return Maps screens to loaded backgrounds
     */
    [[nodiscard]] const std::map<std::string, ProjectUniquePtr>& getBackgrounds () const;
    /**
     * @return The current application context
     */
    [[nodiscard]] ApplicationContext& getContext () const;
    /**
     * Renders a frame
     */
    void update (Render::Drivers::Output::OutputViewport* viewport);
    /**
     * Gets the output
     */
    [[nodiscard]] const WallpaperEngine::Render::Drivers::Output::Output& getOutput () const;
    /**
     * Sets the destination framebuffer for rendering. If not called, the default framebuffer will be used.
     */
    void setDestinationFramebuffer (GLuint framebuffer);

    /**
     * Gets the currently set destination framebuffer for rendering. If not set, returns 0 (the default framebuffer).
     */
    [[nodiscard]] GLuint getDestinationFramebuffer () const;

private:
    /**
     * Sets up an asset locator for the given background
     *
     * @param bg
     */
    AssetLocatorUniquePtr setupAssetLocator (const std::string& bg) const;
    /**
     * Initializes subsystems required for application operation
     */
    void initializeSubsystems ();

    /**
     * Loads projects based off the settings
     */
    void loadBackgrounds ();
    /**
     * Loads the given project
     *
     * @param bg
     * @return
     */
    [[nodiscard]] ProjectUniquePtr loadBackground (const std::string& bg);
    /**
     * Prepares all background's values and updates their properties if required
     */
    void setupProperties ();
    /**
     * Updates the properties for the given background based on the current context
     *
     * @param project
     */
    void setupPropertiesForProject (const Project& project);
    /**
     * Prepares CEF browser to be used
     */
    void setupBrowser ();
    /**
     * Prepares desktop environment-related things (like render, window, fullscreen detector, etc)
     */
    void setupOutput ();
    /**
     * Prepares all audio-related things (like detector, output, etc)
     */
    void setupAudio ();
    /**
     * Prepares the render-context of all the backgrounds so they can be displayed on the screen
     */
    void prepareOutputs ();
    /**
     * Prepares output debugging for all opengl errors
     */
    void setupOpenGLDebugging ();
    /**
     * Takes an screenshot of the background and saves it to the specified path
     *
     * @param filename
     */
    void takeScreenshot (const std::filesystem::path& filename) const;

    struct ActivePlaylist {
	ApplicationContext::PlaylistDefinition definition;
	std::vector<std::size_t> order;
	std::size_t orderIndex = 0;
	std::chrono::steady_clock::time_point nextSwitch;
	std::chrono::steady_clock::time_point lastUpdate;
	std::set<std::size_t> failedIndices;
    };

    void initializePlaylists ();
    void updatePlaylists ();
    /**
     * @return true if any battery handling (--fps-battery/--pause-on-battery) is enabled
     */
    [[nodiscard]] bool batteryFeatureEnabled () const;
    /**
     * Refreshes state.render.batteryActive (cached, cheap to call every frame).
     * @return true if rendering must pause for battery (batteryMaximumFPS == 0 while on battery)
     */
    bool refreshBatteryState ();
    /**
     * Idle pause check (MIT-SCREEN-SAVER, 5s cache). False when disabled.
     * @return true if rendering must pause for input idleness
     */
    bool idlePauseActive ();
    /**
     * Applies per-playlist overrides (fps cap, volume) from the active
     * playlists into render/audio state. Logs only on change.
     */
    void applyPlaylistOverrides ();
    /**
     * Current wallpaper title for MPRIS metadata: parent directory name when
     * under workshop (an id like desktophut-sanyo), file name otherwise.
     */
    [[nodiscard]] std::string currentTitle () const;
    void advancePlaylist (
	const std::string& screen, ActivePlaylist& playlist, const std::chrono::steady_clock::time_point& now,
	int direction = 1
    );
    bool selectNextCandidate (ActivePlaylist& playlist, std::size_t& outOrderIndex);
    bool preflightWallpaper (const std::string& path);
    std::vector<std::size_t> buildPlaylistOrder (const ApplicationContext::PlaylistDefinition& definition);
    void ensureBrowserForProject (const Project& project);
    bool makeAnyViewportCurrent () const;

    /** The application context that contains the current app settings */
    ApplicationContext& m_context;
    /** Maps screens to backgrounds */
    std::map<std::string, ProjectUniquePtr> m_backgrounds {};
    std::map<std::string, ActivePlaylist> m_activePlaylists {};

    std::unique_ptr<WallpaperEngine::Audio::Drivers::Detectors::AudioPlayingDetector> m_audioDetector = nullptr;
    std::unique_ptr<WallpaperEngine::Audio::AudioContext> m_audioContext = nullptr;
    std::unique_ptr<WallpaperEngine::Audio::Drivers::SDLAudioDriver> m_audioDriver = nullptr;
    std::unique_ptr<WallpaperEngine::Audio::Drivers::Recorders::PlaybackRecorder> m_audioRecorder = nullptr;
    std::unique_ptr<WallpaperEngine::Render::RenderContext> m_renderContext = nullptr;
    std::unique_ptr<WallpaperEngine::Render::Drivers::VideoDriver> m_videoDriver = nullptr;
    std::unique_ptr<WallpaperEngine::Render::Drivers::Detectors::FullScreenDetector> m_fullScreenDetector = nullptr;
    std::unique_ptr<WallpaperEngine::WebBrowser::WebBrowserContext> m_browserContext = nullptr;
    std::unique_ptr<WallpaperEngine::Media::MediaSource> m_mediaSource = nullptr;
    std::mt19937 m_playlistRng { std::random_device {}() };
    /** Pending playlist skips from SIGUSR1 (+1) / SIGUSR2 (-1), consumed in render() */
    std::atomic<int> m_playlistSkip { 0 };
    /** MPRIS player (metadata + next/prev/quit), null when the session bus is unavailable */
    std::unique_ptr<Media::MPRISServer> m_mpris = nullptr;
    bool m_isPaused = false;
    /** Pause was triggered by a fullscreen window (resume when none remains) */
    bool m_pausedForFullscreen = false;
    /** Pause was triggered by battery mode (resume when back on AC) */
    bool m_pausedForBattery = false;
    /** Pause was triggered by input idleness (resume on next input) */
    bool m_pausedForIdle = false;
    System::BatteryState m_battery;
    /** X11 idle detector, created on first use when --idle-pause is set */
    std::unique_ptr<Render::Drivers::Detectors::X11IdleDetector> m_idleDetector = nullptr;
    std::chrono::steady_clock::time_point m_lastIdleCheck {};
    bool m_idleCache = false;
    bool m_screenShotTaken = false;
    uint32_t m_nextFrameScreenshot = 0;
    std::chrono::steady_clock::time_point m_pauseStart {};
    GLuint m_destinationFramebuffer = 0;
};
} // namespace WallpaperEngine::Application
