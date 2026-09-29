# Fork: GUI galería (quickshell + rofi)

## Quickshell (setup propio, cero installs)

Módulo en `~/.config/quickshell/gallery/` (nunca en managed):

* `GalleryModel.qml`: `FileView` + `JsonAdapter` sobre
  `~/.config/dwm-titus/gallery-library.json` (watch auto),
  búsqueda, filtro scene|video|web, `apply` vía `we-wallpaper apply-id`,
  `refresh` que regenera el JSON.
* `GalleryWindow.qml`: `FloatingWindow` 880x640 con header, buscador
  (Enter aplica el primero, Esc cierra), chips de tipo y `GridView`
  con `Image file://thumb` async + fallback de texto.
* `shell.qml`: `import qs.gallery`, `GalleryModel`, `GalleryWindow`,
  `IpcHandler target gallery` (`open/close/toggle/count`).
* Hotkey `SUPER+SHIFT+G` → `qs ipc ... call gallery toggle`
  (junto a `SUPER+SHIFT+W` de randomize).

Fuente de datos: `we-wallpaper gallery-json` → `{version, items[]}`
con `thumb` = `thumbs/<id>.png` o preview del workshop.

## Rofi portable (cualquier X11)

`we-wallpaper grid [modo]`: líneas `id [tipo] título` con
`\0icon\x1f<thumb>` + `rofi -dmenu -show-icons`. Sin rofi o sin
display cae a `switch` (dmenu>fzf>terminal). `WE_MENU` fuerza tier.

## Explícitamente fuera

Editor de propiedades por wallpaper (sigue CLI), GIFs animados
(decode en batería), download Workshop (solo `fetch <url>`).
