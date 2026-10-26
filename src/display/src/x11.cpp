#include "we/display/display.hpp"

#include "we/core/log.hpp"
#include "we/gfx/gl.hpp"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/shape.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace we::display {
namespace {

    class X11Display {
    public:
	X11Display () {
	    this->display_ = XOpenDisplay (nullptr);

	    XSetErrorHandler ([] (Display*, XErrorEvent* event) -> int {
		core::log_error (
		    "X11 error code=", static_cast<int> (event->error_code),
		    " request=", static_cast<int> (event->request_code),
		    " resource=", static_cast<unsigned long> (event->resourceid)
		);
		return 0;
	    });

	    if (this->display_ == nullptr) {
		return;
	    }

	    this->screen_ = DefaultScreen (this->display_);
	    this->root_ = RootWindow (this->display_, this->screen_);
	    this->depth_ = DefaultDepth (this->display_, this->screen_);

	    this->atom_net_wm_state = this->intern ("_NET_WM_STATE");
	    this->atom_net_wm_state_below = this->intern ("_NET_WM_STATE_BELOW");
	    this->atom_net_wm_state_sticky = this->intern ("_NET_WM_STATE_STICKY");
	    this->atom_net_wm_state_skip_taskbar = this->intern ("_NET_WM_STATE_SKIP_TASKBAR");
	    this->atom_net_wm_state_skip_pager = this->intern ("_NET_WM_STATE_SKIP_PAGER");
	    this->atom_net_wm_state_fullscreen = this->intern ("_NET_WM_STATE_FULLSCREEN");
	    this->atom_net_active_window = this->intern ("_NET_ACTIVE_WINDOW");
	    this->atom_net_wm_name = this->intern ("_NET_WM_NAME");
	    this->atom_utf8_string = this->intern ("UTF8_STRING");
	    this->atom_xrootpmap_id = this->intern ("_XROOTPMAP_ID");
	    this->atom_esetroot_pmap_id = this->intern ("ESETROOT_PMAP_ID");
	    this->atom_wm_delete_window = this->intern ("WM_DELETE_WINDOW");
	}

	~X11Display () {
	    if (this->res_ != nullptr) {
		XRRFreeScreenResources (this->res_);
	    }

	    if (this->display_ != nullptr) {
		XCloseDisplay (this->display_);
	    }
	}

	X11Display (const X11Display&) = delete;
	X11Display& operator= (const X11Display&) = delete;

	X11Display (X11Display&& other) noexcept :
	    display_ (other.display_), screen_ (other.screen_), depth_ (other.depth_), root_ (other.root_),
	    res_ (other.res_), atom_net_wm_state (other.atom_net_wm_state),
	    atom_net_wm_state_below (other.atom_net_wm_state_below),
	    atom_net_wm_state_sticky (other.atom_net_wm_state_sticky),
	    atom_net_wm_state_skip_taskbar (other.atom_net_wm_state_skip_taskbar),
	    atom_net_wm_state_skip_pager (other.atom_net_wm_state_skip_pager),
	    atom_net_wm_state_fullscreen (other.atom_net_wm_state_fullscreen),
	    atom_net_active_window (other.atom_net_active_window), atom_net_wm_name (other.atom_net_wm_name),
	    atom_utf8_string (other.atom_utf8_string), atom_xrootpmap_id (other.atom_xrootpmap_id),
	    atom_esetroot_pmap_id (other.atom_esetroot_pmap_id), atom_wm_delete_window (other.atom_wm_delete_window) {
	    other.display_ = nullptr;
	    other.res_ = nullptr;
	}

	X11Display& operator= (X11Display&& other) = delete;

	[[nodiscard]] bool valid () const noexcept { return this->display_ != nullptr; }
	[[nodiscard]] Display* display () const noexcept { return this->display_; }
	[[nodiscard]] int screen () const noexcept { return this->screen_; }
	[[nodiscard]] int depth () const noexcept { return this->depth_; }
	[[nodiscard]] Window root () const noexcept { return this->root_; }

	[[nodiscard]] std::optional<core::Vec2> cursor_position () const {
	    if (this->display_ == nullptr) {
		return std::nullopt;
	    }

	    Window root_ret = None;
	    Window child_ret = None;
	    int root_x = 0;
	    int root_y = 0;
	    int win_x = 0;
	    int win_y = 0;
	    unsigned int mask = 0;

	    const Bool ok = XQueryPointer (
		this->display_, this->root_, &root_ret, &child_ret, &root_x, &root_y, &win_x, &win_y, &mask
	    );

	    if (ok == False) {
		return std::nullopt;
	    }

	    return core::Vec2 { static_cast<float> (root_x), static_cast<float> (root_y) };
	}

	[[nodiscard]] Atom intern (std::string_view name) const {
	    return XInternAtom (this->display_, std::string (name).c_str (), False);
	}

	void set_atom (Window window, std::string_view property, std::string_view value) const {
	    const Atom atom = this->intern (value);
	    XChangeProperty (
		this->display_, window, this->intern (property), XA_ATOM, 32, PropModeReplace,
		reinterpret_cast<const unsigned char*> (&atom), 1
	    );
	}

	void set_atoms (Window window, std::string_view property, std::span<const Atom> atoms) const {
	    XChangeProperty (
		this->display_, window, this->intern (property), XA_ATOM, 32, PropModeReplace,
		reinterpret_cast<const unsigned char*> (atoms.data ()), static_cast<int> (atoms.size ())
	    );
	}

	void set_cardinal (Window window, std::string_view property, long value) const {
	    XChangeProperty (
		this->display_, window, this->intern (property), XA_CARDINAL, 32, PropModeReplace,
		reinterpret_cast<const unsigned char*> (&value), 1
	    );
	}

	[[nodiscard]] Window create_desktop_window (const Output& output) const {
	    XSetWindowAttributes attributes {};
	    attributes.background_pixel = BlackPixel (this->display_, this->screen_);
	    attributes.border_pixel = 0;
	    // Unmanaged, like the root backend: the window manager must not tile or move the wallpaper.
	    attributes.override_redirect = True;
	    attributes.event_mask = StructureNotifyMask | ExposureMask | PropertyChangeMask;

	    const Window window = XCreateWindow (
		this->display_, this->root_, output.x, output.y, static_cast<unsigned int> (output.width),
		static_cast<unsigned int> (output.height), 0, CopyFromParent, InputOutput, CopyFromParent,
		CWBackPixel | CWBorderPixel | CWOverrideRedirect | CWEventMask, &attributes
	    );

	    if (window == None) {
		return window;
	    }

	    this->set_atom (window, "_NET_WM_WINDOW_TYPE", "_NET_WM_WINDOW_TYPE_DESKTOP");

	    const std::array<Atom, 4> states
		= { this->atom_net_wm_state_below, this->atom_net_wm_state_sticky, this->atom_net_wm_state_skip_taskbar,
		    this->atom_net_wm_state_skip_pager };
	    this->set_atoms (window, "_NET_WM_STATE", states);

	    constexpr long all_desktops = 0xFFFFFFFFL;
	    this->set_cardinal (window, "_NET_WM_DESKTOP", all_desktops);
	    XStoreName (this->display_, window, "linux-wallpaperengine");

	    std::array<char, 24> instance {};
	    std::array<char, 4> class_name { 'W', 'E', '\0', '\0' };
	    std::strncpy (instance.data (), "linux-wallpaperengine", instance.size () - 1);
	    XClassHint hint { instance.data (), class_name.data () };
	    XSetClassHint (this->display_, window, &hint);

	    XWMHints hints {};
	    hints.flags = InputHint;
	    hints.input = False;
	    XSetWMHints (this->display_, window, &hints);

	    const Atom protocols[] = { this->atom_wm_delete_window };
	    XSetWMProtocols (this->display_, window, const_cast<Atom*> (protocols), 1);

	    const XserverRegion empty = XFixesCreateRegion (this->display_, nullptr, 0);
	    XFixesSetWindowShapeRegion (this->display_, window, ShapeInput, 0, 0, empty);
	    XFixesDestroyRegion (this->display_, empty);

	    XMapWindow (this->display_, window);
	    XLowerWindow (this->display_, window);
	    XFlush (this->display_);

	    return window;
	}

	// A single full-screen window covering every output, used by the root backend.
	[[nodiscard]] Window create_root_window () const {
	    XSetWindowAttributes attributes {};
	    attributes.background_pixel = BlackPixel (this->display_, this->screen_);
	    attributes.border_pixel = 0;
	    // Unmanaged: the window manager must not reparent, tile or move the wallpaper.
	    attributes.override_redirect = True;
	    attributes.event_mask = StructureNotifyMask | ExposureMask | PropertyChangeMask;

	    const Window window = XCreateWindow (
		this->display_, this->root_, 0, 0,
		static_cast<unsigned int> (DisplayWidth (this->display_, this->screen_)),
		static_cast<unsigned int> (DisplayHeight (this->display_, this->screen_)), 0, CopyFromParent,
		InputOutput, CopyFromParent, CWBackPixel | CWBorderPixel | CWOverrideRedirect | CWEventMask, &attributes
	    );

	    if (window == None) {
		return window;
	    }

	    this->set_atom (window, "_NET_WM_WINDOW_TYPE", "_NET_WM_WINDOW_TYPE_DESKTOP");

	    const std::array<Atom, 4> states
		= { this->atom_net_wm_state_below, this->atom_net_wm_state_sticky, this->atom_net_wm_state_skip_taskbar,
		    this->atom_net_wm_state_skip_pager };
	    this->set_atoms (window, "_NET_WM_STATE", states);

	    constexpr long all_desktops = 0xFFFFFFFFL;
	    this->set_cardinal (window, "_NET_WM_DESKTOP", all_desktops);
	    XStoreName (this->display_, window, "linux-wallpaperengine");

	    const Atom protocols[] = { this->atom_wm_delete_window };
	    XSetWMProtocols (this->display_, window, const_cast<Atom*> (protocols), 1);

	    const XserverRegion empty = XFixesCreateRegion (this->display_, nullptr, 0);
	    XFixesSetWindowShapeRegion (this->display_, window, ShapeInput, 0, 0, empty);
	    XFixesDestroyRegion (this->display_, empty);

	    XMapWindow (this->display_, window);
	    XLowerWindow (this->display_, window);
	    XFlush (this->display_);

	    return window;
	}

	[[nodiscard]] std::vector<Output> query_outputs () const {
	    std::vector<Output> result;

	    int monitor_count = 0;
	    XRRMonitorInfo* monitors = XRRGetMonitors (this->display_, this->root_, True, &monitor_count);

	    if (monitors != nullptr && monitor_count > 0) {
		result.reserve (static_cast<std::size_t> (monitor_count));

		for (int index = 0; index < monitor_count; ++index) {
		    const auto& monitor = monitors[index];
		    Output output;
		    output.x = monitor.x;
		    output.y = monitor.y;
		    output.width = monitor.width;
		    output.height = monitor.height;
		    output.primary = monitor.primary != 0;

		    if (monitor.name != None) {
			char* name = XGetAtomName (this->display_, monitor.name);
			if (name != nullptr) {
			    output.name = name;
			    XFree (name);
			}
		    }

		    if (output.name.empty ()) {
			output.name = "output-" + std::to_string (index);
		    }

		    if (monitor.noutput > 0) {
			output.refresh_hz = this->refresh_rate (monitor.outputs[0]);
		    }

		    result.push_back (std::move (output));
		}

		XRRFreeMonitors (monitors);
	    }

	    if (result.empty ()) {
		Output output;
		output.name = "screen";
		output.width = DisplayWidth (this->display_, this->screen_);
		output.height = DisplayHeight (this->display_, this->screen_);
		output.primary = true;
		result.push_back (std::move (output));
	    }

	    return result;
	}

	[[nodiscard]] XRRScreenResources* resources () const {
	    if (this->res_ == nullptr) {
		this->res_ = XRRGetScreenResourcesCurrent (this->display_, this->root_);
	    }

	    return this->res_;
	}

	[[nodiscard]] float refresh_rate (RROutput output) const {
	    XRROutputInfo* output_info = XRRGetOutputInfo (this->display_, this->resources (), output);

	    if (output_info == nullptr) {
		return 60.0F;
	    }

	    float rate = 60.0F;

	    if (output_info->crtc != None) {
		XRRCrtcInfo* crtc = XRRGetCrtcInfo (this->display_, this->resources (), output_info->crtc);

		if (crtc != nullptr) {
		    if (crtc->mode != None) {
			XRRScreenResources* screen = this->resources ();

			for (int index = 0; index < screen->nmode; ++index) {
			    const XRRModeInfo& mode = screen->modes[index];

			    if (mode.id == crtc->mode && mode.hTotal > 0 && mode.vTotal > 0) {
				rate = static_cast<float> (mode.dotClock)
				    / (static_cast<float> (mode.hTotal) * static_cast<float> (mode.vTotal));
				break;
			    }
			}
		    }

		    XRRFreeCrtcInfo (crtc);
		}
	    }

	    XRRFreeOutputInfo (output_info);

	    if (rate < 1.0F || rate > 1000.0F) {
		rate = 60.0F;
	    }

	    return rate;
	}

	[[nodiscard]] std::optional<core::Rect>
	fullscreen_window (std::span<const Output> outputs, std::span<const Window> own_windows) const {
	    if (this->display_ == nullptr) {
		return std::nullopt;
	    }

	    Atom actual_type = None;
	    int actual_format = 0;
	    unsigned long item_count = 0;
	    unsigned long bytes_after = 0;
	    unsigned char* data = nullptr;

	    if (XGetWindowProperty (
		    this->display_, this->root_, this->atom_net_active_window, 0, 1, False, XA_WINDOW, &actual_type,
		    &actual_format, &item_count, &bytes_after, &data
		) != Success
		|| data == nullptr || item_count == 0) {
		if (data != nullptr) {
		    XFree (data);
		}
		return std::nullopt;
	    }

	    const Window active = *reinterpret_cast<Window*> (data);
	    XFree (data);

	    if (active == None || std::ranges::find (own_windows, active) != own_windows.end ()) {
		return std::nullopt;
	    }

	    bool fullscreen = false;
	    data = nullptr;

	    if (XGetWindowProperty (
		    this->display_, active, this->atom_net_wm_state, 0, 32, False, XA_ATOM, &actual_type,
		    &actual_format, &item_count, &bytes_after, &data
		) == Success
		&& data != nullptr) {
		const auto* atoms = reinterpret_cast<Atom*> (data);
		fullscreen = std::ranges::find (atoms, atoms + item_count, this->atom_net_wm_state_fullscreen)
		    != atoms + item_count;
		XFree (data);
	    }

	    if (!fullscreen) {
		return std::nullopt;
	    }

	    XWindowAttributes attributes {};
	    if (XGetWindowAttributes (this->display_, active, &attributes) == 0) {
		return std::nullopt;
	    }

	    int root_x = 0;
	    int root_y = 0;
	    Window child = None;
	    XTranslateCoordinates (this->display_, active, this->root_, 0, 0, &root_x, &root_y, &child);

	    const core::Rect window_rect { static_cast<float> (root_x), static_cast<float> (root_y),
					   static_cast<float> (attributes.width),
					   static_cast<float> (attributes.height) };

	    for (const auto& output : outputs) {
		const core::Rect output_rect { static_cast<float> (output.x), static_cast<float> (output.y),
					       static_cast<float> (output.width), static_cast<float> (output.height) };
		const bool intersects = window_rect.x < output_rect.right () && window_rect.right () > output_rect.x
		    && window_rect.y < output_rect.bottom () && window_rect.bottom () > output_rect.y;

		if (intersects) {
		    return window_rect;
		}
	    }

	    return std::nullopt;
	}

	[[nodiscard]] Atom net_wm_state_fullscreen () const noexcept { return this->atom_net_wm_state_fullscreen; }
	[[nodiscard]] Atom net_active_window () const noexcept { return this->atom_net_active_window; }
	[[nodiscard]] Atom wm_delete_window () const noexcept { return this->atom_wm_delete_window; }
	[[nodiscard]] Atom xrootpmap_id () const noexcept { return this->atom_xrootpmap_id; }
	[[nodiscard]] Atom esetroot_pmap_id () const noexcept { return this->atom_esetroot_pmap_id; }

    private:
	Display* display_ = nullptr;
	int screen_ = 0;
	int depth_ = 24;
	Window root_ = None;
	mutable XRRScreenResources* res_ = nullptr;

	Atom atom_net_wm_state = None;
	Atom atom_net_wm_state_below = None;
	Atom atom_net_wm_state_sticky = None;
	Atom atom_net_wm_state_skip_taskbar = None;
	Atom atom_net_wm_state_skip_pager = None;
	Atom atom_net_wm_state_fullscreen = None;
	Atom atom_net_active_window = None;
	Atom atom_net_wm_name = None;
	Atom atom_utf8_string = None;
	Atom atom_xrootpmap_id = None;
	Atom atom_esetroot_pmap_id = None;
	Atom atom_wm_delete_window = None;
    };

    class X11WindowSurface final : public Surface {
    public:
	X11WindowSurface (X11Display& x11, gfx::Context& context, Window window, int width, int height) :
	    x11_ (x11), context_ (context), window_ (window) {
	    auto surface = context.create_window_surface (reinterpret_cast<void*> (window), width, height);

	    if (surface) {
		this->surface_ = std::move (*surface);
		this->valid_ = true;
	    } else {
		core::log_error ("cannot create EGL window surface: ", surface.error ().message);
	    }
	}

	~X11WindowSurface () override = default;

	[[nodiscard]] bool valid () const noexcept override { return this->valid_; }

	[[nodiscard]] core::Extent size () const noexcept override {
	    return { this->surface_.width (), this->surface_.height () };
	}

	void make_current () override { this->context_.make_current (&this->surface_); }

	void swap () override {
	    this->make_current ();
	    this->surface_.swap ();
	    XFlush (this->x11_.display ());
	}

	void set_vsync (bool enabled) override { this->context_.set_swap_interval (enabled ? 1 : 0); }

	void readback (std::span<std::uint8_t> rgba) override {
	    this->make_current ();
	    glReadBuffer (GL_BACK);
	    this->surface_.read_rgba (rgba, this->surface_.width (), this->surface_.height ());
	}

    private:
	X11Display& x11_;
	gfx::Context& context_;
	Window window_ = None;
	gfx::Surface surface_;
	bool valid_ = false;
    };

    class X11DesktopDriver final : public Driver {
    public:
	X11DesktopDriver (X11Display x11, std::vector<Output> outputs, bool vsync) :
	    x11_ (std::move (x11)), outputs_ (std::move (outputs)), vsync_ (vsync) { }

	~X11DesktopDriver () override {
	    for (const Window window : this->windows_) {
		XDestroyWindow (this->x11_.display (), window);
	    }
	    XFlush (this->x11_.display ());
	}

	[[nodiscard]] Backend backend () const noexcept override { return Backend::x11_desktop; }
	[[nodiscard]] std::string_view name () const noexcept override { return "x11-desktop"; }
	[[nodiscard]] std::span<const Output> outputs () const noexcept override { return this->outputs_; }

	std::expected<std::unique_ptr<gfx::Context>, core::Error> create_context () override {
	    auto context = gfx::Context::create_x11 (this->x11_.display ());

	    if (!context) {
		return std::unexpected (std::move (context.error ()));
	    }

	    context->set_swap_interval (this->vsync_ ? 1 : 0);
	    return std::make_unique<gfx::Context> (std::move (*context));
	}

	std::expected<std::unique_ptr<Surface>, core::Error>
	create_surface (std::size_t output_index, gfx::Context& context) override {
	    if (output_index >= this->outputs_.size ()) {
		return core::fail (core::ErrorCode::out_of_range, "X11 output index out of range");
	    }

	    while (this->windows_.size () < this->outputs_.size ()) {
		const auto index = this->windows_.size ();
		const Window window = this->x11_.create_desktop_window (this->outputs_[index]);

		if (window == None) {
		    return core::fail (core::ErrorCode::io, "XCreateWindow failed");
		}

		this->windows_.push_back (window);
	    }

	    const auto& output = this->outputs_[output_index];
	    auto surface = std::make_unique<X11WindowSurface> (
		this->x11_, context, this->windows_[output_index], output.width, output.height
	    );

	    if (!surface->valid ()) {
		return core::fail (core::ErrorCode::io, "cannot create the X11 OpenGL surface");
	    }

	    return surface;
	}

	void pump () override {
	    while (XPending (this->x11_.display ()) > 0) {
		XEvent event {};
		XNextEvent (this->x11_.display (), &event);

		if (event.type == ClientMessage
		    && static_cast<Atom> (event.xclient.data.l[0]) == this->x11_.wm_delete_window ()) {
		    this->closed_ = true;
		}
	    }
	}

	[[nodiscard]] bool should_close () const noexcept override { return this->closed_; }

	[[nodiscard]] std::optional<core::Rect> fullscreen_window (std::size_t output_index) const override {
	    if (output_index >= this->outputs_.size ()) {
		return std::nullopt;
	    }

	    const auto active = this->x11_.fullscreen_window (this->outputs_, this->windows_);

	    if (!active) {
		return std::nullopt;
	    }

	    const auto& output = this->outputs_[output_index];
	    const core::Rect out { static_cast<float> (output.x), static_cast<float> (output.y),
				   static_cast<float> (output.width), static_cast<float> (output.height) };

	    if (active->x < out.right () && active->right () > out.x && active->y < out.bottom ()
		&& active->bottom () > out.y) {
		return active;
	    }

	    return std::nullopt;
	}

	[[nodiscard]] std::optional<core::Vec2> cursor_position () const override {
	    return this->x11_.cursor_position ();
	}

    private:
	X11Display x11_;
	std::vector<Output> outputs_;
	std::vector<Window> windows_;
	bool vsync_ = true;
	bool closed_ = false;
    };

    class X11RootSurface final : public Surface {
    public:
	X11RootSurface (X11Display& x11, gfx::Context& context, Window window) :
	    x11_ (x11), context_ (context), window_ (window), width_ (DisplayWidth (x11.display (), x11.screen ())),
	    height_ (DisplayHeight (x11.display (), x11.screen ())) {
	    auto surface = context.create_window_surface (reinterpret_cast<void*> (window), width_, height_);

	    if (surface) {
		this->surface_ = std::move (*surface);
		this->valid_ = true;
	    } else {
		core::log_error ("cannot create EGL window surface: ", surface.error ().message);
	    }
	}

	~X11RootSurface () override = default;

	[[nodiscard]] bool valid () const noexcept override { return this->valid_; }

	[[nodiscard]] core::Extent size () const noexcept override { return { this->width_, this->height_ }; }

	void make_current () override { this->context_.make_current (&this->surface_); }

	void set_vsync (bool enabled) override { this->context_.set_swap_interval (enabled ? 1 : 0); }

	void swap () override {
	    this->make_current ();
	    this->surface_.swap ();
	    XFlush (this->x11_.display ());
	}

	void readback (std::span<std::uint8_t> rgba) override {
	    this->make_current ();
	    this->surface_.read_rgba (rgba, this->width_, this->height_);
	}

    private:
	X11Display& x11_;
	gfx::Context& context_;
	Window window_ = None;
	gfx::Surface surface_;
	int width_ = 0;
	int height_ = 0;
	bool valid_ = false;
    };

    class X11RootDriver final : public Driver {
    public:
	X11RootDriver (X11Display x11, std::vector<Output> outputs, bool vsync) :
	    x11_ (std::move (x11)), outputs_ (std::move (outputs)), vsync_ (vsync) { }

	~X11RootDriver () override {
	    if (this->window_ != None) {
		XDestroyWindow (this->x11_.display (), this->window_);
		XFlush (this->x11_.display ());
	    }
	}

	[[nodiscard]] Backend backend () const noexcept override { return Backend::x11_root; }
	[[nodiscard]] std::string_view name () const noexcept override { return "x11-root"; }
	[[nodiscard]] std::span<const Output> outputs () const noexcept override { return this->outputs_; }

	std::expected<std::unique_ptr<gfx::Context>, core::Error> create_context () override {
	    auto context = gfx::Context::create_x11 (this->x11_.display ());

	    if (!context) {
		return std::unexpected (std::move (context.error ()));
	    }

	    context->set_swap_interval (this->vsync_ ? 1 : 0);
	    return std::make_unique<gfx::Context> (std::move (*context));
	}

	std::expected<std::unique_ptr<Surface>, core::Error>
	create_surface (std::size_t output_index, gfx::Context& context) override {
	    if (output_index >= this->outputs_.size ()) {
		return core::fail (core::ErrorCode::out_of_range, "X11 output index out of range");
	    }

	    if (this->window_ == None) {
		this->window_ = this->x11_.create_root_window ();

		if (this->window_ == None) {
		    return core::fail (core::ErrorCode::io, "XCreateWindow failed");
		}
	    }

	    auto surface = std::make_unique<X11RootSurface> (this->x11_, context, this->window_);

	    if (!surface->valid ()) {
		return core::fail (core::ErrorCode::io, "cannot create the X11 OpenGL surface");
	    }

	    return surface;
	}

	void pump () override {
	    while (XPending (this->x11_.display ()) > 0) {
		XEvent event {};
		XNextEvent (this->x11_.display (), &event);
	    }
	}

	[[nodiscard]] bool should_close () const noexcept override { return false; }

	[[nodiscard]] std::optional<core::Rect> fullscreen_window (std::size_t output_index) const override {
	    if (output_index >= this->outputs_.size ()) {
		return std::nullopt;
	    }

	    return this->x11_.fullscreen_window (this->outputs_, {});
	}

	[[nodiscard]] std::optional<core::Vec2> cursor_position () const override {
	    return this->x11_.cursor_position ();
	}

    private:
	X11Display x11_;
	std::vector<Output> outputs_;
	Window window_ = None;
	bool vsync_ = true;
    };

} // namespace

std::expected<std::unique_ptr<Driver>, core::Error> create_x11_driver (const DriverOptions& options, Backend backend) {
    X11Display x11;

    if (!x11.valid ()) {
	return core::fail (core::ErrorCode::io, "cannot open the X11 display (is DISPLAY set?)");
    }

    auto outputs = x11.query_outputs ();

    if (outputs.empty ()) {
	return core::fail (core::ErrorCode::io, "the X11 server reported no outputs");
    }

    if (!options.output.empty ()) {
	const auto found = std::ranges::find_if (outputs, [&options] (const Output& output) {
	    return output.name == options.output;
	});

	if (found == outputs.end ()) {
	    return core::fail (core::ErrorCode::not_found, "X11 output '" + options.output + "' not found");
	}

	outputs.erase (outputs.begin (), found);
	outputs.resize (1);
    }

    if (backend == Backend::x11_root) {
	return std::make_unique<X11RootDriver> (std::move (x11), std::move (outputs), options.vsync);
    }

    return std::make_unique<X11DesktopDriver> (std::move (x11), std::move (outputs), options.vsync);
}

bool x11_available () noexcept {
    const char* display = std::getenv ("DISPLAY");
    return display != nullptr && *display != '\0';
}

} // namespace we::display
