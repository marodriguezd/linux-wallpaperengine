#pragma once

#include <dbus/dbus.h>

#include <cstring>
#include <functional>
#include <string>

namespace WallpaperEngine::Media {
/**
 * Minimal MPRIS server: exposes the engine as org.mpris.MediaPlayer2 on the
 * session bus with the current wallpaper as metadata. Next/Previous skip
 * the active playlist, Quit stops the engine. All callbacks run on the
 * render thread via dispatch(), never from a DBus thread.
 */
class MPRISServer {
public:
    struct Callbacks {
	std::function<void ()> next;
	std::function<void ()> previous;
	std::function<void ()> quit;
	/** Current wallpaper title/id, "" when none */
	std::function<std::string ()> title;
	/** True while the engine is paused (fullscreen/battery/idle) */
	std::function<bool ()> paused;
	/** True when a multi-item playlist is active (advertised as CanGo*) */
	std::function<bool ()> canSkip;
    };

    explicit MPRISServer (Callbacks callbacks);
    ~MPRISServer ();

    MPRISServer (const MPRISServer&) = delete;
    MPRISServer& operator= (const MPRISServer&) = delete;

    /**
     * Pumps the bus (non-blocking) and emits PropertiesChanged when the
     * title or status changed. Safe to call every frame.
     */
    void dispatch ();

    /** "/wallpaperengine/<sanitized title>" object path for metadata */
    static std::string trackId (const std::string& title);

private:
    static DBusHandlerResult messageHandler (DBusConnection* connection, DBusMessage* message, void* userData);
    DBusHandlerResult handleMessage (DBusConnection* connection, DBusMessage* message);

    void sendPropertiesChanged (DBusConnection* connection, bool title, bool status);
    static void appendMetadata (DBusMessageIter* dict, const std::string& title);

    DBusConnection* m_connection = nullptr;
    Callbacks m_callbacks;
    std::string m_lastTitle;
    bool m_lastPaused = false;
    bool m_haveState = false;
};
} // namespace WallpaperEngine::Media
