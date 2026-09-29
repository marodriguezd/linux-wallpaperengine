#include "X11IdleDetector.h"

#include "WallpaperEngine/Logging/Log.h"

#include <dlfcn.h>

using namespace WallpaperEngine::Render::Drivers::Detectors;

// layout mirror of XScreenSaverInfo (we only read idle), avoids including
// <X11/extensions/scrnsaver.h> so nothing new is needed at build time
struct IdleInfoMirror {
    Window window;
    int state;
    int kind;
    unsigned long til_or_since;
    unsigned long idle;
    unsigned long event_mask;
};

X11IdleDetector::X11IdleDetector () {
    this->m_xss = dlopen ("libXss.so.1", RTLD_LAZY);

    if (this->m_xss == nullptr) {
	this->m_xss = dlopen ("libXss.so", RTLD_LAZY);
    }

    if (this->m_xss == nullptr) {
	sLog.debugerror ("X11IdleDetector: libXss not found, idle pause disabled");
	return;
    }

    this->m_allocInfo = reinterpret_cast<AllocInfoFn> (dlsym (this->m_xss, "XScreenSaverAllocInfo"));
    this->m_queryInfo = reinterpret_cast<QueryInfoFn> (dlsym (this->m_xss, "XScreenSaverQueryInfo"));

    if (this->m_allocInfo == nullptr || this->m_queryInfo == nullptr) {
	sLog.debugerror ("X11IdleDetector: incomplete libXss, idle pause disabled");
	dlclose (this->m_xss);
	this->m_xss = nullptr;
	return;
    }

    this->m_display = XOpenDisplay (nullptr);

    if (this->m_display == nullptr) {
	sLog.debugerror ("X11IdleDetector: cannot open display");
	dlclose (this->m_xss);
	this->m_xss = nullptr;
	return;
    }

    this->m_info = this->m_allocInfo ();

    if (this->m_info == nullptr) {
	XCloseDisplay (this->m_display);
	this->m_display = nullptr;
	dlclose (this->m_xss);
	this->m_xss = nullptr;
    }
}

X11IdleDetector::~X11IdleDetector () {
    if (this->m_info != nullptr) {
	// XFree lives in libX11, already linked to the engine
	XFree (this->m_info);
	this->m_info = nullptr;
    }

    if (this->m_display != nullptr) {
	XCloseDisplay (this->m_display);
	this->m_display = nullptr;
    }

    if (this->m_xss != nullptr) {
	dlclose (this->m_xss);
	this->m_xss = nullptr;
    }
}

std::uint64_t X11IdleDetector::idleMilliseconds () {
    if (this->m_display == nullptr || this->m_queryInfo == nullptr || this->m_info == nullptr) {
	return 0;
    }

    if (this->m_queryInfo (this->m_display, DefaultRootWindow (this->m_display), this->m_info) == 0) {
	return 0;
    }

    return static_cast<std::uint64_t> (static_cast<IdleInfoMirror*> (this->m_info)->idle);
}
