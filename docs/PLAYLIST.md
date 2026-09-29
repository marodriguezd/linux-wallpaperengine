# Fork: playlists + next/prev (Vía B)

## Formato propio

`we-wallpaper playlist-play <fich> [pantalla|all]`:

```text
# delay: 30
# order: random
desktophut-sanyo
3726414565
```

Una id o ruta por línea, `#` comentarios. `delay` en minutos (default 60),
`order` sequential|random (default sequential). Flag engine espejo:
`--playlist-file` (misma semántica de posición que `--playlist`:
tras `--screen-root` va a esa pantalla, si no al modo ventana).
El estado guarda el fichero y `restore` lo re-ejecuta como playlist.

## Next/prev por señal

`SIGUSR1` = siguiente, `SIGUSR2` = anterior, en todas las pantallas con
playlist activa. El handler solo fija un `atomic<int>`; el hilo de render
aplica `advancePlaylist (+1/-1)` sin reshuffle al retroceder.
Pausado (fullscreen/batería) encola el salto hasta reanudar.

```bash
we-wallpaper playlist-next   # kill -USR1 al engine con playlist
we-wallpaper playlist-prev   # kill -USR2
```

Seguridad: solo se señaliza a procesos cuya cmdline contenga
`--playlist-file`. Un engine viejo (USR = matar) o uno con fondo fijo
jamás matchean. Sin playlist en marcha avisa y sale 1.

## vaapi en NVIDIA (obligatorio leer)

Cambiar de video a video con `hwdec auto` puede segfaultear dentro de
`libcuda` (race de ffmpeg al negociar backend; probado con backtrace:
`cuMemFreeAsync` desde `ff_nvdec_decode_init`). No es bug del fork
(el timer de 60 min de upstream lo sufriría igual).

Fix: `--hwdec` (`auto|vaapi|nvdec|cuda|...`, default `auto`) que se pasa
tal cual a mpv. En esta RTX 3050 `--hwdec vaapi` (el que ya usaba por
fallback) hace next/prev estables. Helper: `WE_HWDEC=vaapi`.
