# Acerca de Linux Wallpaper Engine (rebuild)

Linux Wallpaper Engine lleva los fondos animados de Wallpaper Engine (Steam) a
escritorios Linux. La rama `feat/rebuild-from-scratch` reescribe el motor desde
cero en C++23 y lo entrega como un único ejecutable, `we`.

## Qué pretende

- **Un decode por fondo, da igual cuántos monitores haya.** Las superficies se crean por
  salida, pero comparten un único renderer y contexto.
- **Contenido nativo de Steam.** Encuentra las bibliotecas de Steam, incluidos Flatpak,
  Snap y los prefijos de Proton, y lee los `.pkg` en sitio.
- **Consciente de la batería.** La cadencia de frames cambia con la fuente de energía, y
  las salidas tapadas por una ventana a pantalla completa se saltan.
- **Presentación sin copias en X11.** Los frames se dibujan directamente en la ventana del
  escritorio mediante un swapchain EGL.

Detalles técnicos y estado por componente: [docs/REBUILD.md](docs/REBUILD.md).

## Alcance actual

Funciona:

- Escenas con capas de imagen: origen, escala, rotación, alineación, blending,
  capas sólidas, imágenes a pantalla completa, texturas recortadas y color de fondo.
- Materiales con `LIGHTING`: luces puntuales (`lpoint`), término ambiente, specular
  GGX y máscaras PBR (`metallic`, `roughness`, `reflection`, `emissive`).
- Materiales con `REFLECTION`: reflejo en pantalla a partir del normal map, con
  `reflectivity`, `roughness` y `metallic`.
- Vídeos (MP4) reproducidos con libmpv, incluidos los embebidos en texturas TEXB.
- Backends `x11-desktop`, `x11-root`, `wayland` (sin probar en un compositor real) y `headless`.
- Comandos `run`, `list`, `steam`, `doctor`, `vfs` y `pkg`.

Todavía no:

- Wallpapers web (CEF): se rechazan con un error explícito.
- Parallax de ratón y desplazamiento de cámara.
- Espectro de audio: no hay captura de PulseAudio ni PipeWire.
- Partículas, texto, sonido, cadenas de efectos, esqueletos, hojas de sprites
  (`SPRITESHEET`) y scripts QuickJS.
- Luces de tipo spot, tube y directional: sólo se interpretan las puntuales.
- Traducción HLSL a GLSL: los materiales con shader propio usan el shader de sprites.

El estado de verificación y los comandos están en
[docs/REBUILD.md](docs/REBUILD.md#verification-status). Las convenciones de trabajo, en
[AGENTS.md](AGENTS.md).

## Estado del código

- El build y las pruebas unitarias (`we_tests`) viven en `CMakeLists.txt`,
  `CMakePresets.json` y `tests/`.
- El código legado queda en `src/WallpaperEngine` y `src/Steam` como referencia. No forma parte del build.
- Los cambios de esta rama todavía no están confirmados en git.
