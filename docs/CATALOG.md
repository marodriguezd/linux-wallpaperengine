# Fork: catálogo local (Fase 2)

## Qué hay

* `src/WallpaperEngine/Library/Library.{h,cpp}`: escanea todas las raíces
  workshop (`Steam::FileSystem::workshopRoots(431960)`, first-root-wins),
  parsea `project.json` sin GL (solo `title/type/file/preview/description/tags`,
  `type` en minúsculas, `workshopid` o nombre de carpeta como id).
* Caché versionada `~/.cache/linux-wallpaperengine/library.json`
  (`XDG_CACHE_HOME` respetado): `load()` rápido, `scan()` completo en
  milisegundos para ~18 items, `search()` insensible a caso + filtro de tipo.
* Flags engine (salen antes de construir el renderer, no piden background):
  `--list-library [--json] [--refresh]`, `--search <q> [--type scene|video|web] [--json]`.
* `--screenshot-delay` admite hasta 600 frames (antes 5) para calentar
  escenas/videos en `make-previews`.

## Helper

* `list` / `is_video` leen la caché con un solo `python3` (antes 2 por item);
  si no hay caché, el fork la regenera; si el engine es viejo, scan legacy.
* `refresh`: reescanea vía engine fork.
* `make-previews [--force]` → `thumbs/<id>.png`: video con `ffmpeg`
  (primer frame), scene/web con render real del engine en ventana 480x270
  (necesita display, `WE_PREVIEW_TIMEOUT` default 60s).

## Precedencia de IDs

`workshopid` numérico o string gana; si falta, el nombre de carpeta
(`desktophut-sanyo` funciona como `--bg` igual que antes).

## Store online (`catalog` / `catalog-get`)

TSV `ref|title|thumb|file|kind`; ref lista para `catalog-get`.
Inspiración y formatos descubiertos vía
[antwny/aura](https://github.com/antwny/aura) (GPL-3.0, como este
fork): comportamiento reimplementado desde cero en el helper, sin
copiar su código.

* `bing`: 16 dailies (`idx=0,8`, `mkt=en-US`); thumb con
  `pid=hp&w=480&h=270` (el `_480x270` pelado da 404) y full `_UHD`
  (4K). `catalog bing archive[N]`: histórico zkeq (1616 dailies,
  `~/.cache/we-wallpaper/online/bing_archive.json`); `catalog-get`
  resuelve live (16 días) + fallback al archivo.
* `wallhaven`: search API (`purity=100`, `toplist`, 24); con key
  opcional (`keys.conf` o `WALLHAVEN_KEY`) hay búsqueda completa.
* `motionbgs`: vídeos MP4 sin API pública — scraping HTML con UA
  Firefox (portada, `tag:xxx`, búsqueda). `catalog-get
  motionbgs:<id> [hd|4k]` descarga el mp4 (`/dl/hd|4k/<id>/`, HD por
  defecto) y lo registra como proyecto de vídeo. Si cambian el HTML,
  falla cerrado con mensaje.
* `minimal`: colección DenverCoder1 (337 fondos, JSON en GitHub raw
  con caché 24h); thumbs vía proxy `wsrv.nl`; título/autor del
  filename. `catalog-get` usa URL determinista `images/<nombre>`.
* Hosts quisquillosos: `fetch_url` acepta `WE_FETCH_UA`,
  `WE_FETCH_REFERER`, `WE_FETCH_EXT`, `WE_FETCH_NAME`.
