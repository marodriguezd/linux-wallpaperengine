# Fork: switch + theming en cualquier X11 (Vía C)

## Sin DWM-ismos duros

Todo parametrizable por entorno (con default sensato):

| Var | Default | Efecto |
| --- | ------- | ------ |
| `WE_DIR` / `WE_ASSETS_DIR` / `WE_ENGINE` | rutas Steam + auto: `/opt/fork` si existe, si no `PATH` | layouts `~/.steam`, flatpak, evita builds rancios en `PATH` (p.ej. `/usr/local/bin` que crashea dejando el thumb estático) |
| `WE_BACKEND` | auto (`swaymsg`→`xrandr`) | fuerza `sway`/`x11`, avisa si es inválido |
| `WE_MODE` | `lite` en `switch` | modo del apply |
| `WE_CURSOR_THEME` / `WE_CURSOR_SIZE` | heredar entorno | antes `Banana:30` fijo; en dwm-titus hereda `cat_cursors:32` |
| `WE_MENU` | auto | `rofi`→`dmenu`→`fzf`→terminal |

`apply-all` lanza **un** proceso con N pares `--screen-root/--bg`
(antes N procesos pisándose el root-pixmap en multi-head).

## Switch

`we-wallpaper switch [modo]`: lee la caché Fase 2, menú en cascada,
`apply` al elegido. `rofi`/`dmenu` exigen display; `fzf`/terminal no.

## Theming (dwm-titus, con fallbacks)

Hook `apply_theme` tras cada `apply`/`restore`/`switch`, nunca rompe nada:

1. `WE_WALLPAPER_REGISTER=1` + existe `dwm-settings-wallpaper` → registra
   thumb/preview como fondo gestionado (fallback estático tras el engine).
2. `WE_THEME=<preset>` → `dwm-settings-theme apply` (quickshell recarga
   solo vía FileView, sin restart, sin pywal).
3. `WE_THEME=auto` → color dominante (PIL) vs `normbgcolor` de cada preset
   en `themes.toml` (usuario o managed, solo lectura) y aplica el cercano.
4. Sin dwm-titus: pasos 1-3 se saltan solos (solo engine).

`WE_THEME_ENABLED=0` apaga todo el hook.
