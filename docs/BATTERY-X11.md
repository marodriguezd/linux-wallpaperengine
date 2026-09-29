# Fork: batería + X11 real (DWM)

Diferencias con upstream `Almamu/linux-wallpaperengine`, etapa 1.

## Flags nuevos

| Flag | Efecto |
| ---- | ------ |
| `--fps-battery N` | Cap de FPS en batería (UPower, fallback `/sys/class/power_supply`). `0` = pausar, `-1` = desactivado (default) |
| `--pause-on-battery` | Atajo de `--fps-battery 0` |
| `--profile lite\|balanced\|full` | Preset inmediato. `lite` = fps 15 + sin partículas/mouse/parallax/audio-processing (equivale a tu diario DWM). Los flags que vayan **después** de `--profile` ganan |

Validación: `--fps` se limita a `1..240`, `--fps-battery` a `-1..240`.

## Detección fullscreen X11

`X11FullScreenDetector` ahora mira `_NET_WM_STATE_FULLSCREEN` (EWMH) antes que
la geometría. Los juegos y navegadores en F11 pausan aunque DWM los gestione;
la geometría queda como fallback para clientes viejos. También se corrige un
`XFree` al puntero equivocado (`schildren`).

La pausa distingue motivo: fullscreen vs batería. Solo reanuda cuando su
motivo desaparece (antes, pausar por batería con pantalla libre reanudaba al
instante).

## Throttle

`GLFWOpenGLDriver` y `WaylandOpenGLDriver` calculaban `minimumTime` una vez
(`static`): cambiar el FPS en caliente no hacía nada. Ahora se calcula por
frame vía `ApplicationContext::effectiveMaximumFPS()`.

## Helper `tools/we-wallpaper`

Port del `~/.local/bin/we-wallpaper` (Sway) a X11 vía `xrandr`, con
autodetección de backend (`WE_BACKEND=sway|x11` para forzar) y paso de
`WE_PROFILE`, `WE_FPS_BATTERY`, `WE_PAUSE_ON_BATTERY`. El estado guarda el
backend y los valores para `restore`.

Ejemplo diario DWM (eDP-1, batería):

```bash
WE_PROFILE=lite WE_FPS_BATTERY=10 tools/we-wallpaper apply desktophut-sanyo lite eDP-1
```

Seguir upstream: `git fetch upstream && git rebase upstream/main`.
