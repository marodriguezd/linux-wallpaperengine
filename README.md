<p align="center">
  <img src="assets/icons/we-wallpaper.svg" width="72" height="72" alt="Linux Wallpaper Engine" />
</p>

# Linux Wallpaper Engine

High-performance, modern C++23 Wallpaper Engine runtime for Linux with zero-duplicate multimonitor decoding, hardware acceleration, full particle simulation, and native desktop integration.

![Build Status](https://img.shields.io/badge/build-passing-brightgreen?style=flat-square)
![Platform](https://img.shields.io/badge/platform-Linux%20(X11%20%7C%20Wayland)-blue?style=flat-square)
![Language](https://img.shields.io/badge/language-C%2B%2B23-blue?style=flat-square)
![Tests](https://img.shields.io/badge/tests-86%2F86%20passing-brightgreen?style=flat-square)
![Sanitizers](https://img.shields.io/badge/sanitizers-ASan%20%7C%20LSan%20clean-purple?style=flat-square)

---

## Overview

**Linux Wallpaper Engine** brings animated Steam Wallpaper Engine wallpapers to Linux desktops without draining your battery or overloading your GPU. Built from the ground up in modern C++23, it delivers a single unified executable: `we`.

```
[ Displays: eDP-1 (144Hz) + HDMI-1 (75Hz) ]
           │
           ├─► Viewport 1 (eDP-1)  ──┐
           │                         ├─► Shared Wallpaper Instance
           └─► Viewport 2 (HDMI-1) ──┘     ├─► Single libmpv Decoder (Hardware Acceleration)
                                           └─► Single Scene / Particles / Shader Pass
```

> [!TIP]
> When running across multiple monitors, the engine reuses video decodes and rendered scene framebuffers across viewports, eliminating redundant GPU workloads and memory allocations.

---

## Key Features

* **Complete Scene & 2D/3D Quad Pipeline**
  * Exact transformation calculations: top-left origin ($y$-up in viewport), scale, and clockwise radian rotation (`angles.z`).
  * Dynamic animated spritesheets (`TEXS0001` and `TEXS0003`) with frametime cycling and sub-rectangle UV mapping.
  * Correct auto-cropped and padded texture sampling for DXT1, DXT3, DXT5, ARGB8888, and RG88 (luminance-alpha) formats.
* **Full Particle Simulation Engine**
  * Real-time GPU batch-quad rendering (`Renderer::draw_quad`) for rain, snow, fog, twinkling stars, and light halos.
  * Emitters: `boxrandom`, `sphererandom` with continuous (`rate`) and burst (`instantaneous`) emission.
  * Initializers & Operators: `lifetime`, `size`, `velocity`, `alpha`, `color`, `rotation`, `angularvelocity`, `movement` (with gravity and drag), `alphafade`, `oscillatealpha`, `oscillateposition`, `sizechange`.
  * Velocity-aligned particles (`spritetrail`), additive (`additive`) and refractive/translucent blending modes.
* **PBR Lighting & Screen-Space Reflections**
  * Point lights (`lpoint`), ambient lighting term, and GGX specular calculations.
  * PBR material masks (`metallic`, `roughness`, `reflection`, `emissive`).
  * Real-time screen-space reflections using normal maps and offscreen captured scene framebuffers.
* **Full-Frame Post-Processing Pipeline (`_rt_FullFrameBuffer`)**
  * Ping-pong offscreen FBO compositing supporting chromatic aberration, blur, bloom, and multi-pass effects.
  * Dynamic shader timers (`g_Time`) and automatic texture/sampler slot binding.
* **In-Situ `.pkg` Archive Reader**
  * High-performance memory mapping without extracting files to disk.
  * Automatically resolves standard `scene.pkg`, `gifscene.pkg`, and custom `.pkg` archives.
* **Zero-Configuration Steam Discovery**
  * Auto-locates Steam libraries across native paths, Flatpak, Snap, and Proton prefixes.
  * Case-insensitive Virtual File System (VFS) to seamlessly resolve Windows case variations.
* **Multiple Display Backends**
  * `x11-desktop`: Zero-copy presentation via EGL desktop windows.
  * `x11-root`: Draws directly to the X11 root window.
  * `wayland`: Native Wayland integration.
  * `headless`: Offscreen rendering with PBuffer support and frame capture (`--screenshot`).
* **Power Awareness & Presets**
  * Dynamic AC/battery rate management (`battery`, `balanced`, `smooth` presets).
  * Automatically pauses rendering when covered by fullscreen applications.

---

## Architecture

The project is structured into modular modern C++23 libraries under `src/`:

| Module | Description |
| :--- | :--- |
| `src/core` | `std::expected` error handling, zero-cost logging, math types (`Vec2`, `Vec3`, `Mat4`), LZ4 decompression, memory-mapped files |
| `src/steam` | VDF parsing, Steam library discovery (Native/Flatpak/Snap/Proton), case-insensitive VFS, zero-copy `.pkg` reader |
| `src/gfx` | EGL context management, GLSL shaders, textures, framebuffers, batch quad renderer |
| `src/display` | Display drivers: `x11-desktop`, `x11-root`, `wayland`, `headless` |
| `src/scene` | Project/scene JSON parsing, TEX/TEXB textures, spritesheets, particle systems, PBR lighting, FBO effects, `VideoPlayer` (libmpv) |
| `src/power` | Battery monitoring and frame pacing |
| `src/presets` | Performance profiles (`battery`, `balanced`, `smooth`) |
| `src/app` | Core engine lifecycle, multimonitor orchestration, render loop |
| `src/cli` | The `we` command-line executable |
| `tests/` | Unit test suite (`we_tests`) with test harness and fixtures |

---

## Prerequisites & Dependencies

To render Steam Workshop wallpapers, you must own and install **Wallpaper Engine** via Steam. The engine automatically finds your assets.

### Fedora (40 / 41 / 42+)
```bash
sudo dnf install gcc g++ cmake ninja-build \
  mesa-libEGL-devel mesa-libGL-devel libX11-devel libXrandr-devel \
  wayland-devel wayland-protocols-devel mpv-devel lz4-devel
```

### Ubuntu / Debian (24.04+)
```bash
sudo apt-get install build-essential cmake ninja-build \
  libegl1-mesa-dev libgl1-mesa-dev libx11-dev libxrandr-dev \
  libwayland-dev wayland-protocols libmpv-dev liblz4-dev
```

### Arch Linux
```bash
sudo pacman -S base-devel cmake ninja \
  mesa libglvnd libx11 libxrandr wayland wayland-protocols mpv lz4
```

---

## Building

The project uses modern CMake presets:

### 1. Optimized Release Build
```bash
cmake --preset release
cmake --build --preset release
```
The resulting binary will be at `./build/release/bin/we`.

### 2. Debug Build & Unit Tests
```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

### 3. AddressSanitizer & LeakSanitizer (ASan / LSan)
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

### Common Examples

#### Run a Workshop Wallpaper
```bash
# By workshop ID:
we run 3615154110

# Target a specific monitor:
we run 3615154110 --output HDMI-1

# Apply a preset (battery: 15fps, balanced: 30fps, smooth: 60fps):
we run 3615154110 --preset smooth

# Run headless and export a screenshot after 60 frames:
we run 3615154110 --backend headless --frames 60 --screenshot preview.png
```

#### List Installed Wallpapers
```bash
we list
```

#### Diagnose Environment & Hardware
```bash
we doctor
```
Reports active display backends, detected monitors with refresh rates, GPU driver info, battery status, and Steam library paths.

#### Inspect `.pkg` Files
```bash
we pkg ~/.local/share/Steam/steamapps/workshop/content/431960/3615154110/scene.pkg --list
```

---

## Code Quality & Standards

- **Modern C++23**: Semantics centered around RAII, `std::expected` for explicit failure propagation, and zero unhandled exceptions.
- **Sanitizer Clean**: Verified with AddressSanitizer, UndefinedBehaviorSanitizer, and LeakSanitizer.
- **Strict Linting & Formatting**: Follows `.clang-format` and strict `-Werror` compiler warnings.

> [!NOTE]
> The legacy C++17 implementation is kept under `src/WallpaperEngine/` and `src/Steam/` for historical reference and is no longer part of the build system.
