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
* Pill en `DwmPanel` (icono ) con popupRequested + toggle,
  coordinado con el resto de popups vía `selectPanelPopup`.
* Panel de propiedades por fondo: `we-wallpaper props <id>` lista
  (`name|type|text|value|saved?`); toggles para boolean, texto para el
  resto; Save guarda en `props/<id>.conf` (auto-cargado al aplicar),
  Apply re-aplica con overrides.

Fuente de datos: `we-wallpaper gallery-json` → `{version, items[]}`
con `thumb` = `thumbs/<id>.png` o preview del workshop.

## Importar, favoritos, categorías y catálogos

* `import <vídeo> [id] [--title]`: copia a `WE_DIR` con validación
  fuerte `ffprobe` (códec + dimensiones); `Library::valid` exige que
  el fichero exista (fantasmas marcados inválidos).
* `fav <id>`: toggle con sidecar `~/.config/we-wallpaper/favorites.json`
  (sobrevive rescans) + `--fav` en el engine; estrella en cada card +
  chip `★ favs` en la galería.
* `tags <id> [tag...]`: edita tags del `project.json` (+ refresh).
  La galería indexa `id+title+description+tags` y filtra por categorías
  (anime/nature/sci-fi/cyberpunk/gaming/minimalist) vía chips.
* `catalog bing|wallhaven [query]` + `catalog-get <ref> [id]`
  (motionbgs sin API pública: pendiente; wallhaven sin key = últimos
  públicos). Pestaña Explorar con thumbs remotos (`Image.async` +
  placeholder): escribir filtra las tarjetas cargadas, Enter busca
  online; click selecciona y `⬇ Instalar` descarga (vídeo→WE_DIR con
  `project.json`, imagen→`backgrounds-lab`) + refresh a Local.
* Tipo `image` (`img-<stem>`): las estáticas de `backgrounds-lab` salen
  en Local con chip `image`, thumb directo y sin estrella/props;
  `apply-id img-*` para el engine y pone `feh --bg-fill` (restore y
  switch lo respetan vía `apply_id`).

## Rofi portable (cualquier X11)

`we-wallpaper grid [modo]`: líneas `id [tipo] título` con
`\0icon\x1f<thumb>` + `rofi -dmenu -show-icons`. Sin rofi o sin
display cae a `switch` (dmenu>fzf>terminal). `WE_MENU` fuerza tier.

## Explícitamente fuera

Editor de propiedades por wallpaper (sigue CLI), GIFs animados
(decode en batería), download Workshop (solo `fetch <url>`).
