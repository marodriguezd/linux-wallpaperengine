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
