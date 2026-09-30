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
  (anime/nature/sci-fi/cyberpunk/gaming/minimalist) vía chips (solo Local).
* Local rediseñado (ideas de Aura, clean-room): filtros con contadores
  `Todo (n) · Vídeo (n) · Imagen (n) · ★ (n)` + orden A-Z; cards con meta
  (`Vídeo • 12 MB`) y badge `ACTIVA` (vía `current-id`); empty state con
  "ver todo". `mood:` queda como segunda fila solo en Local.
* Explorar rediseñado: subtítulo por fuente; filtros contextuales
  (motion: tags + HD/4K; wallhaven: top/hot/random; bing:
  recientes/archivo); botón por tarjeta que muta
  `[Instalar] → [Instalando…] → [✓ Instalado]` (click en instalado =
  aplicar); error con Reintentar; `Cargar más` (motion/wallhaven pág.2,
  minimal amplía límite, bing avanza archiveN).
* `catalog <src> [query] [page]` + `catalog-get <ref> [id|4k]`
  (wallhaven sin key = últimos públicos). Pestaña Explorar con thumbs
  remotos (`Image.async` + placeholder): escribir filtra las tarjetas
  cargadas, Enter busca online; instalar descarga (vídeo→WE_DIR con
  `project.json`, imagen→`backgrounds-lab`) + refresh a Local.
* Iconografía: nada de emoji, solo symbolic del tema
  (`user-trash-symbolic`, `folder-download-symbolic`, `emblem-ok-symbolic`,
  `view-refresh-symbolic`, `go-down-symbolic`, `emblem-favorite-symbolic`)
  resueltos con `Quickshell.iconPath(name, true)`. `ShellButton` acepta
  `icon:` (string) para pintarlos; sin él, texto plano.
* `remove <id> [--force]`: borra items del store (`motionbgs-*`,
  `bing-*`, `img-*`) con su thumb y título, y refresca. Los wallpapers
  reales del workshop se niegan sin `--force`.
* Steam/unsubscribe o borrado manual: el rescan no elimina la entrada,
  la marca `missing` (caché + `--list-library` con `(missing)` + `list`).
  La galería la atenúa con badge "no disponible", bloquea el click/Enter
  y conserva título y favorito; `apply-id` sobre un id ausente no mata
  la sesión: reaplica el último bueno (`current-id`).
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
