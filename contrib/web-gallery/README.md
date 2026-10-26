# Web Gallery & Control Dashboard

Lightweight, zero-dependency standalone web catalog and runtime dashboard for `linux-wallpaperengine`.

Built adhering to the `frontend-design` specification in `frontend_design_plan.md`:
- **Palette**: Deliberate cold dark scheme (`#0d1117` canvas, `#161c24` elevated surfaces, `#232d3b` structural borders, `#00d4be` electric cyan active indicators, `#f59e0b` amber power indicator).
- **Media-first 16:9 Grid**: Custom responsive grid with resolution tags (4K, 1080p), render type pills (Scene / Video), and active engine status.
- **Inspector Side Panel**: Full wallpaper metadata, power profile selector (Battery 15 FPS / Balanced 30 FPS / Smooth 60 FPS), interactive engine settings (backends: X11/Wayland/headless, outputs, battery pause, audio), and live CLI command generator (`we run <id> ...`).
- **Zero Dependencies**: Pure HTML5, modern CSS3 (custom properties, grid, aspect-ratio, blur filters), and vanilla JS with inline SVG theme artwork generators.

---

## Usage

### 1. Direct Browser Opening
Simply open `index.html` in any modern web browser:

```bash
xdg-open contrib/web-gallery/index.html
```

Or serve via Python's built-in HTTP server:

```bash
python3 -m http.server 8080 --directory contrib/web-gallery/
# Open http://localhost:8080
```

### 2. Loading Live Steam Workshop Catalog
You can export your installed wallpapers directly using `we list --json` and load them into the web dashboard:

```bash
we list --json > workshop.json
```

Then click **Load JSON** in the dashboard toolbar and select `workshop.json`.

---

## Keyboard Shortcuts

| Shortcut | Action |
| :--- | :--- |
| `/` | Focus search bar |
| `b` | Toggle animated background ("Fondo Activo" / "Fondo Apagado") |
| `Esc` | Close inspector panel or defocus search |
| `Enter` / `Space` | Select / inspect focused wallpaper card |
