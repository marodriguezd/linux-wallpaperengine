# Fork: resultados de batería (medidos desenchufado)

Máquina: i5-12450H (UHD ADL GT2) + RTX 3050 Mobile, driver NVIDIA 615.71.09,
Fedora 44, DWM X11. `OnBattery=true` (UPower + sysfs `Discharging`).
Ventana 480x270, video `desktophut-sanyo`, `--profile lite --silent`.

## iGPU (modo obligatorio del setup)

| Modo | CPU media | Nota |
| ---- | --------- | ---- |
| `--fps 60` sin cap | ~59% | sin `hwdec` útil |
| `--fps 15` (daily) | ~43% | con `vaapi-copy` |
| `--fps 60 --fps-battery 10` | ~39% | cap activo: -34% vs 60, -10% vs daily |
| `--fps 60 --pause-on-battery` | ~2-7% | pausa real (era 25% por busy-loop) |

El decode manda: el throttle solo frena el dispatch GL, mpv decodifica
igual. Por eso 60→10 fps no es 6x menos CPU.

## Hallazgos que cambiaron código

1. `--hwdec vaapi` cae a software (`VO yuv420p`, 110% CPU). El bueno en
   Intel es **`vaapi-copy`** (~43% en daily). `auto` también llega ahí,
   pero tras probar vulkan/cuda primero.
2. Pausa al 25%: `FULLSCREEN_CHECK_WAIT_TIME 250` = `usleep(250µs)` +
   `XQueryTree` a 4kHz. Ahora 200000 (200ms): ~2% en pausa, reanuda igual.
3. `--hwdec` se pasa como **opción** pre-init de mpv además de propiedad
   (la propiedad post-init llegaba tarde y no frenaba el probe CUDA).

## dGPU (solo de cara a la repo, NO para el daily)

* Video + dGPU (`__NV_PRIME_RENDER_OFFLOAD=1`) = **segfault** en
  `libcuda` (`cuMemFreeAsync` desde `ff_nvdec_decode_init`), con y sin
  `--hwdec`. Backtraces en `cuMemFreeAsync <- ff_nvdec_decode_init`.
  Conclusión: video es **solo-iGPU** en este stack.
* Scene + dGPU = OK: 105MiB VRAM, ~0% CPU (shaders 100% GPU).
* dGPU en reposo ya chupa ~7-10W (`handy` 24MiB la mantiene despierta):
  otro motivo para iGPU siempre.

## Recomendación daily (batería)

```bash
WE_PROFILE=lite WE_FPS_BATTERY=10 WE_HWDEC=vaapi-copy we-wallpaper apply <id> lite eDP-1
```

Y para duración máxima: `WE_PAUSE_ON_BATTERY=1` (o `--pause-on-battery`).
Medición: `/tmp/opencode/batt/measure.sh` (no forma parte del repo).
