<p align="center">
  <img src="assets/icons/we-wallpaper.svg" width="80" height="80" alt="Linux Wallpaper Engine Icon" />
</p>

# Linux Wallpaper Engine

High-performance, modern C++23 Wallpaper Engine runtime for Linux with zero-copy presentation, single-pass multimonitor decoding, full particle simulation, and cross-toolkit desktop integration (Qt, GTK, Quickshell, Web).

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen?style=flat-square)](#building)
[![Platform](https://img.shields.io/badge/platform-Linux%20(X11%20%7C%20Wayland)-blue?style=flat-square)](#display-backends)
[![Language](https://img.shields.io/badge/language-C%2B%2B23-blue?style=flat-square)](#architecture)
[![Tests](https://img.shields.io/badge/tests-87%2F87%20passing-brightgreen?style=flat-square)](#building)
[![Toolkits](https://img.shields.io/badge/toolkits-Qt%20%7C%20GTK%20%7C%20Quickshell%20%7C%20Web-orange?style=flat-square)](#frontends--gui-integration)

---

## Overview

**Linux Wallpaper Engine** brings animated Steam Wallpaper Engine wallpapers to Linux desktops without draining laptop battery or overloading the GPU. Engineered from scratch in modern C++23, it replaces the legacy codebase with small, modular static libraries unified behind a single executable: `we`.

```
[ Monitors: eDP-1 (144Hz Primary) + HDMI-1 (75Hz) ]
           │
           ├─► Viewport 1 (eDP-1)  ──┐
           │                         ├─► Shared Wallpaper Instance
           └─► Viewport 2 (HDMI-1) ──┘     ├─► Single libmpv Decoder (RGBA16F zero-copy)
                                           └─► Single Scene / Particles / FBO Post-Processing Pass
```

> [!TIP]
> **Single decode per wallpaper**: Regardless of monitor count, video decoding and scene compositing run exactly once per frame. The resulting texture is shared across all output swapchains, saving memory bandwidth and keeping power consumption minimal.

---

## Key Features

### Graphics & Scene Pipeline
* **Canonical Coordinate Transforms**: Matches Wallpaper Engine's top-left origin space ($y$-down in `scene.json`), projecting to a centered $y$-up world with clockwise radian quad rotation (`angles.z`).
* **Broad Texture Format Support**: Native decoders for TEX and TEXB mipmaps in `ARGB8888`, `RGB888`, `RGB565`, `R8`, `DXT1`, `DXT3`, `DXT5`, `BC7`, and dual `RG88` (standard two-channel normal/flow maps + `TextureFlags_AlphaChannelPriority` luminance-alpha).
* **Animated Spritesheets**: Decodes `TEXS0001` and `TEXS0003` animated atlas sheets with frame-time cycling, frame counts, and sub-rectangle UV cropping.
* **Full Particle Simulation Engine**: Real-time batch-quad simulation (`boxrandom`, `sphererandom` emitters; continuous rate and burst spawning; gravity, drag, velocity, angular velocity, alpha fades, size scaling, sprite trails, and prewarming) optimized via compile-time enums for zero per-frame heap allocations.
* **Post-Processing & FBO Ping-Pong**: Multi-pass offscreen framebuffers (`_rt_FullFrameBuffer`), bloom downsample/upsample passes, chromatic aberration, blur, and dynamic shader timers (`g_Time`).
* **PBR Lighting & Screen-Space Reflections**: Point lights (`lpoint`), ambient term, GGX specular falloff, PBR masks (`metallic`, `roughness`, `reflection`, `emissive`), and normal-mapped screen reflections.
* **Hardware-Accelerated Video**: Single-decode `libmpv` output directly rendered into a 16-bit float (`RGBA16F`) texture, avoiding 8-bit dithering artifacts on Mesa and supporting embedded MP4 textures inside TEXB.

### System & Desktop Integration
* **In-Situ `.pkg` Archive Reader**: Memory-mapped zero-copy access to `scene.pkg` and `gifscene.pkg` files without extracting them to disk.
* **Native Steam Discovery**: Auto-detects Steam installations and workshop directories from `libraryfolders.vdf`, including native paths, Flatpak, Snap, Proton prefixes, and Windows NTFS/ext4 mounts.
* **Case-Insensitive VFS**: Normalizes Windows directory separators (`\`) and case variations at the filesystem boundary.
* **Power & Battery Governor**: Monotonic frame pacer (`clock_nanosleep`), automatic AC/battery rate throttling, and smart pausing when active windows cover the wallpaper in fullscreen.

---

## Display Backends

| Backend | Platform | Presentation Mechanism | Best Used For |
| :--- | :--- | :--- | :--- |
| `x11-desktop` | X11 | EGL window with `_NET_WM_WINDOW_TYPE_DESKTOP`, `BELOW`, `STICKY` | Default for modern composited X11 desktops (Picom, KWin X11, GNOME X11, Xfce). |
| `x11-root` | X11 | Fullscreen EGL window covering all outputs / MIT-SHM root-pixmap | Traditional tiling and lightweight window managers (dwm, i3, bspwm, Openbox). |
| `wayland` | Wayland | `zwlr_layer_shell_v1` background or bottom layer per `wl_output` | Native Wayland compositors supporting `wlr-layer-shell` (Hyprland, Sway, River). |
| `headless` | Any | EGL PBuffer / Surfaceless offscreen context | Automated testing, CI, benchmarks, and PNG captures via `--screenshot`. |

---

## Frontends & GUI Integration

Linux Wallpaper Engine provides multiple interfaces tailored to different desktop workflows:

### 1. Quickshell Desktop Gallery (`contrib/quickshell-gallery/`)
A responsive, floating control window (`GalleryWindow.qml` + `GalleryModel.qml`) built for Wayland and X11:
* **Toolkit Theme Selector**: Dynamically switch the UI style on the fly between:
  * **Auto / Sistema**: Inherits live colors from `~/.config/quickshell/core/Theme.qml` via desktop portal.
  * **GTK / Libadwaita**: GNOME dark styling (`#1e1e1e`/`#242424`, 10px rounded corners, GNOME blue accent).
  * **Qt / KDE Breeze**: Plasma dark styling (`#232629`/`#31363b`, 5px clean corners, Breeze cyan accent).
  * **Brave / Obsidian**: Obsidian palette (`#0d1117`/`#161c24`, 8px corners, Brave Lion Orange accent).
* **Wayland Layer & Backend Controls**: Select layer (`background`, `bottom`, `overlay`) and toggle fullscreen pause.
* **GPU Governor**: Toggle between `Intel iGPU` (saves laptop battery by keeping dGPU in D3cold) and `dGPU Rendimiento`.

### 2. Standalone Web Gallery & Dashboard (`contrib/web-gallery/`)
A self-contained, zero-dependency browser UI (`index.html`) to browse, preview, and control wallpapers:
* Live engine telemetry (`we 2.0.0`, active wallpaper, monitor refresh rate, battery state).
* Instant zero-reload Toolkit switcher (Auto, GTK, Qt, Brave).
* Media-first 16:9 wallpaper grid with resolution badges (4K, 1080p).
* Reactive CLI generator for `we run` and `we-wallpaper` commands with one-click clipboard copying.

### 3. Lightweight Desktop Menus (`rofi` / `dmenu`)
Via `tools/we-wallpaper`:
```bash
we-wallpaper grid       # Interactive grid with thumbnails in Rofi
we-wallpaper switch     # Cascading selection menu (rofi > dmenu > fzf > terminal)
we-wallpaper toolkit    # View or set toolkit theme (auto | gtk | qt | brave)
we-wallpaper gpu        # View or set GPU governor (intel | nvidia | auto)
```

---

## Architecture

The project is structured into modular C++23 libraries under `src/`:

```
src/
├── core/       # Error handling (std::expected), logging, math (Vec2/3/4, Mat4), LZ4, mapped files
├── steam/      # KeyValues VDF parser, Steam discovery, case-insensitive VFS, .pkg reader
├── gfx/        # EGL contexts, GLSL shaders, textures, framebuffers, batch quad renderer
├── display/    # Backends: x11-desktop, x11-root, wayland (wlr-layer-shell), headless
├── scene/      # project.json/scene.json, TEXB, spritesheets, particles, FBO post-processing, libmpv
├── power/      # UPower/sysfs battery monitor, monotonic frame pacer
├── presets/    # Performance profiles (battery, balanced, smooth)
├── app/        # Engine core lifecycle, monitor management, render loop
└── cli/        # The 'we' executable
```

> [!NOTE]
> The legacy C++17 implementation is kept under `src/WallpaperEngine/` and `src/Steam/` for historical reference only; it is not compiled or linked.

---

## Prerequisites & Installation

To render Steam Workshop wallpapers, you must own and install [Wallpaper Engine](https://store.steampowered.com/app/431960/Wallpaper_Engine/) on Steam. The engine locates your library and shared assets automatically.

### Dependencies

#### Fedora (40 / 41 / 42+)
```bash
sudo dnf install gcc g++ cmake ninja-build \
  mesa-libEGL-devel mesa-libGL-devel libX11-devel libXrandr-devel libXext-devel libXfixes-devel \
  wayland-devel wayland-protocols-devel mpv-libs-devel nlohmann-json-devel
```

#### Ubuntu / Debian (24.04+)
```bash
sudo apt-get install build-essential cmake ninja-build \
  libegl1-mesa-dev libgl1-mesa-dev libx11-dev libxrandr-dev libxext-dev libxfixes-dev \
  libwayland-dev wayland-protocols libmpv-dev nlohmann-json3-dev
```

#### Arch Linux
```bash
sudo pacman -S base-devel cmake ninja \
  mesa libglvnd libx11 libxrandr libxext libxfixes wayland wayland-protocols mpv nlohmann-json
```

---

## Building

The build system is powered by CMake presets:

### Optimized Release Build
```bash
cmake --preset release
cmake --build --preset release
```
The compiled binary will be placed at `./build/release/bin/we`.

### Debug Build & Test Suite
```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
```

### Sanitizer Verification (ASan & LSan)
```bash
cmake --preset asan
cmake --build --preset asan
ctest --preset asan
```

---

## CLI Usage (`we`)

```bash
we 2.0.0 - Wallpaper Engine runtime for Linux

Usage:
  we run PATH|WORKSHOP_ID [options]
  we list [--json]
  we steam [--json]
  we doctor
  we vfs --root DIR [--list] PATH
  we pkg ARCHIVE [--list] [--extract ENTRY --out FILE]
  we --version
```

### Common Commands

#### 1. Play a Workshop Wallpaper
```bash
# Play by workshop ID on primary desktop:
we run 3615154110

# Target a specific output:
we run 3615154110 --output eDP-1

# Use a specific performance profile (battery, balanced, smooth):
we run 3615154110 --preset smooth

# Set custom AC and battery frame rates:
we run 3615154110 --fps 144 --battery-fps 30 --pause-on-battery

# Force a specific display backend:
we run 3615154110 --backend x11-desktop   # or wayland, x11-root, headless
```

#### 2. Headless Offscreen Capture
```bash
# Render 60 frames offscreen and write the final composited frame to PNG:
we run 3615154110 --backend headless --frames 60 --screenshot preview.png
```

#### 3. Inspect System Status & Discovery
```bash
# Report detected display servers, OpenGL renderer, battery level, and Steam libraries:
we doctor

# List all discovered Steam Wallpaper Engine wallpapers:
we list

# Inspect contents of a package file in-place:
we pkg /path/to/scene.pkg --list
```
