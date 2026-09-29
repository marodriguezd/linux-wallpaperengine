# Fork: MPRIS out (el engine como player)

El engine se publica en el bus de sesión como
`org.mpris.MediaPlayer2.linux-wallpaperengine`
(`/org/mpris/MediaPlayer2`). Nunca es fatal: sin bus, sin player.

## Qué expone

* `Identity`, `CanQuit=true`, `CanRaise=false`, `HasTrackList=false`.
* `PlaybackStatus` Playing/Paused (sigue a la pausa real:
  fullscreen, batería, idle).
* `Metadata`: `mpris:trackid` (`/wallpaperengine/<id>` saneado),
  `xesam:title` (id workshop o fichero actual), `xesam:artist`.
* `CanGoNext/CanGoPrevious`: true solo con playlist multi-item.
  `CanPlay/CanPause/CanSeek`: false honestos (la pausa es automática).
* Métodos: `Next`/`Previous` (saltan playlist), `Quit` (para el engine),
  `Raise`/`Play`/`Pause`/`PlayPause`/`Stop` (acknowledge sin efecto).
* `PropertiesChanged` al cambiar título o estado.

`render()` bombea el bus cada frame (non-blocking, timeout 0),
también en pausa, así que el estado no se queda rancio.

## Uso

```bash
playerctl -p linux-wallpaperengine status     # Playing|Paused
playerctl -p linux-wallpaperengine next       # siguiente (requiere playlist)
playerctl -p linux-wallpaperengine previous
```

Nota: `playerctl metadata` falla igual con Brave en este equipo
(quirk de playerctl, no del engine); por D-Bus crudo el `Metadata`
es válido y completo.
