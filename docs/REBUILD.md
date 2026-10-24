# Rebuilt engine (`feat/rebuild-from-scratch`)

This branch contains a ground-up implementation of the runtime in C++23 under
`src/`, built as small static libraries behind the `we` command line tool. The
legacy tree (`src/WallpaperEngine`, `src/Steam`, ...) is kept for
reference and is **not** part of the build any more; the new root
`CMakeLists.txt` only compiles the components described below.

## Design goals

- **Zero-copy presentation on X11.** Frames are drawn straight into a desktop
  window through an EGL swapchain; there is no `glReadPixels` frame pump. The
  root-pixmap backend exists for window managers that want the wallpaper painted
  on the root window and uses MIT-SHM (`XShmPutImage`) instead of core X11
  `PutImage` requests.
- **One decode per wallpaper, regardless of monitor count.** Surfaces are
  created per output but they share a single renderer/context, so video and
  scene state are uploaded once.
- **Native Steam content.** Libraries are discovered from `libraryfolders.vdf`
  recursively, including Flatpak, Snap, Proton prefixes and Windows mounts.
  Paths are resolved through a case-insensitive VFS (Windows separators and
  casing are normalised), and `.pkg` archives are read in place, without
  extracting them to disk.
- **Power aware.** Frame pacing uses `clock_nanosleep(CLOCK_MONOTONIC,
  TIMER_ABSTIME)`, the AC/battery policy changes the target rate, and outputs
  covered by a fullscreen window are skipped.
- **Typed errors and RAII everywhere.** Fallible operations return
  `std::expected<T, we::core::Error>`; GL/EGL/X11/mpv handles own themselves.

## Components

| Target | Contents |
| --- | --- |
| `we_core` | `std::expected` error type, logging, math (`Vec2/Vec3/Mat4/Rect/Color`), LZ4 block decoder, memory-mapped files |
| `we_steam` | KeyValues/`libraryfolders.vdf` parser, Steam discovery, case-insensitive VFS, `.pkg` (PKGV) archive reader |
| `we_gfx` | EGL context (X11, Wayland, surfaceless), shaders, textures, framebuffers (RGBA8/RGBA16F), batching sprite renderer |
| `we_display` | `x11-desktop` (EGL window), `x11-root` (full-screen EGL window), `wayland` (`wlr-layer-shell` background layer), `headless` |
| `we_scene` | `project.json`/`scene.json` parsing, asset container over directory + `.pkg`, TEXB textures (ARGB8888, RGB888, RGB565, R8, RG88, DXT1/3/5, BC7, LZ4 mipmaps), scene renderer, libmpv video player |
| `we_power` | UPower/sysfs battery monitor, monotonic frame pacer |
| `we_app` | engine: opens a wallpaper, builds the renderer, runs the frame loop |
| `we_cli` | the `we` executable (`run`, `list`, `steam`, `doctor`, `vfs`, `pkg`) |

Scenes also search the Wallpaper Engine installation's `assets` directory as a
fallback root, so shared resources (`models/util/fullscreenlayer.json`,
built-in materials) resolve even when the wallpaper does not ship them.

Dependencies: GCC 13+ or Clang 17+ (C++23), CMake 3.24+, Ninja, EGL and OpenGL,
X11 with Xrandr/Xext/Xfixes, libmpv, nlohmann/json, and optionally
wayland-client, wayland-egl, wayland-protocols and `wayland-scanner` for the
Wayland backend. `third_party/` provides stb and bcdec.

On Fedora:

```sh
sudo dnf install gcc-c++ cmake ninja-build mesa-libEGL-devel mesa-libGL-devel \
    libX11-devel libXrandr-devel libXext-devel libXfixes-devel wayland-devel \
    wayland-protocols-devel mpv-libs-devel nlohmann-json-devel
```

## Build and test

```sh
cmake --preset debug        # or release / relwithdebinfo / asan
cmake --build --preset debug
ctest --preset debug
```

The `asan` preset enables AddressSanitizer + UndefinedBehaviorSanitizer with
`-Werror` and runs the suite with leak detection enabled. Sanitizer runs prefer
Mesa's EGL vendor when it is installed (see `tests/CMakeLists.txt`): the
proprietary NVIDIA ICD keeps process-global caches alive after `eglTerminate()`
and libmpv leaves its session D-Bus connection open, both of which LeakSanitizer
would otherwise attribute to the engine.

## Usage

```sh
we run 2412175986                  # Steam workshop id
we run ~/.steam/steam/steamapps/workshop/content/431960/2412175986
we run ./my-wallpaper --backend x11-root --fps 30
we run ./my-wallpaper --headless --frames 120 --screenshot shot.png
we list
we doctor
we steam --json
we pkg scene.pkg --cat materials/particles.json
we vfs --root ./my-wallpaper 'Textures\background.TEX'
```

Useful `run` flags: `--backend auto|x11-desktop|x11-root|wayland|headless`,
`--output NAME`, `--fps`, `--battery-fps`, `--pause-on-battery`,
`--no-pause-fullscreen`, `--no-vsync`, `--audio`, `--volume`, `--frames`,
`--screenshot`, `--verbose`.

### Backends

| Backend | Presentation | Notes |
| --- | --- | --- |
| `x11-desktop` | EGL window surface, `_NET_WM_WINDOW_TYPE_DESKTOP` | Default on X11. Zero-copy: the compositor picks up the swapped buffer. |
| `x11-root` | Full-screen EGL window below other windows | Same window setup as `x11-desktop`, covering every output at once. |
| `wayland` | `zwlr_layer_shell_v1` background layer | Requires a compositor implementing `wlr-layer-shell`; surfaces one `wl_surface` per `wl_output`. |
| `headless` | EGL pbuffer | Tests, CI and `--screenshot` rendering. |

`we run` picks `x11-desktop` when `DISPLAY` is set, `wayland` when only
`WAYLAND_DISPLAY` is set, and fails with an explicit error otherwise; pass
`--backend` to override.

### Video specifics

The video player renders libmpv output into an **RGBA16F** offscreen texture and
draws it on every output, so a multi-monitor clone decodes once. The float
target is required, not cosmetic: libmpv renders in 16-bit float internally and
its 8-bit dithering path produces a black image on Mesa's llvmpipe. mpv is polled
once per engine frame (`mpv_render_context_update`) because the update callback
is not delivered per frame unless `ADVANCED_CONTROL` is enabled, and
`report_swap()` is only sent for frames that were actually rendered.

## Verification status

- 80 unit tests across core/Steam/VFS/`.pkg`/gfx/display/scene, green in
  `debug` and `release`. The sanitized suite runs 80/80 under the `asan` preset
  with no ASan or UBSan diagnostics. With leak detection enabled, LeakSanitizer
  only reports the driver allocations made by libmpv's CUDA probe (6509 bytes
  in 135 allocations, all frames inside `libcuda`), with no frames from this
  repository. A temporary `leak:libcuda` suppression outside the repository
  lets the leak-enabled run pass; it is not part of `tests/lsan.supp`.
- End-to-end under `Xvfb :99` (1280x720, GLX + MIT-SHM): scene and video playback
  through `x11-desktop`, `x11-root` and `headless`; window verified as
  `_NET_WM_WINDOW_TYPE_DESKTOP` with `BELOW`/`STICKY`/`SKIP_TASKBAR`; root
  properties verified as `_XROOTPMAP_ID == ESETROOT_PMAP_ID`; PNG captures
  checked visually.
- CLI flows exercised against a synthetic Steam library: `steam` (text/JSON),
  `list`, `run <workshop id>`, `vfs`, `pkg`, `doctor`.

## Scene space and transforms

- Image layers use the editor's scene space: `origin` is the object centre in a
  top-left, y-down coordinate system, `scale` multiplies the quad, and `angles`
  are radians (`angles.z` rotates the quad; a positive value turns clockwise on
  screen). `vec2`/`vec3` accept both the `"x y z"` strings written by Wallpaper
  Engine and JSON arrays.
- The renderer converts scene positions to a centred, y-up space shared by
  image layers and lights. The orthographic projection is centred on the scene
  size; the saved camera eye/centre do not offset image placement (the original
  engine translates the projection by the eye and its view matrix cancels it).
  Automatic projections use the rotated bounds of the image quads.
- Auto-cropped models (`autosize`, `cropoffset`) need no runtime UV cropping:
  the TEX mipmaps store the cropped bitmap and the header's full width/height is
  only the canvas it came from, so layers sample the whole uploaded texture
  (UV 0..1). Quad dimensions come from the scene object's `size`, falling back
  to the model's width/height and then the cropped bitmap dimensions.

## Not implemented yet

- Web wallpapers (CEF) are rejected with an explicit "unsupported" error.
- Audio spectrum wallpapers: no PulseAudio/PipeWire loopback capture yet.
- Particle systems, effect chains, skeletons, text, sounds, scripts and
  non-point lights are not rendered: they are dropped and counted in the log
  line `scene: N image layers, K point lights, M objects not rendered`, and the
  `effects` array of an image is parsed but ignored.
- Mouse parallax (`parallaxDepth`, `cameraparallax`) and the saved camera eye
  are parsed but not applied yet.
- `SPRITESHEET` materials are drawn as a single image; frame selection and
  animation of sprite sheets are not implemented.
- Lit materials (`LIGHTING`/`REFLECTION` combos) use their textured albedo
  multiplied by the object colour, as `genericimage4.frag` does. `LIGHTING`
  evaluates up to four `lpoint` lights with the GGX specular of
  `common_pbr_2.h` (`pow(1 - distance / radius, exponent)` falloff, intensity
  folded into the light colour), the ambient term (`ambientcolor * albedo`),
  `speculartint` and the PBR mask channels: metallic R, roughness G,
  reflection B and emissive A. Light `density` and shadow mapping are ignored.
  `REFLECTION` samples a mipmapped copy of the frame drawn so far through the
  material's normal map, scaled by `reflectivity`, `roughness`, `metallic` and
  `reflectivitydistance`; the reflection map multiplies `reflectivity`. The
  copy is taken once per contiguous run of reflective layers, so later layers
  in the same run do not reflect earlier ones.
- Internal render targets (`_rt_*`, such as the post-processing layer's
  `_rt_FullFrameBuffer`) are recognised and skipped with an info log.
- HLSL→GLSL translation is not wired in; materials that name a custom shader
  fall back to the built-in sprite shader.
- The Wayland backend compiles but has not been exercised on a live compositor
  in this environment.
