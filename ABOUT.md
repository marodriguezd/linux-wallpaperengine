# Acerca de Linux Wallpaper Engine (rebuild C++23)

Linux Wallpaper Engine lleva los fondos animados de Wallpaper Engine (Steam) a
escritorios Linux. El proyecto ha sido reescrito desde cero en C++23 y se entrega
como un único ejecutable moderno y ultra-optimizado: `we`.

## Qué pretende

- **Un decode por fondo, da igual cuántos monitores haya.** Las superficies se crean por
  salida, pero comparten un único renderer y contexto.
- **Contenido nativo de Steam.** Encuentra las bibliotecas de Steam, incluidos Flatpak,
  Snap y los prefijos de Proton, y lee los `.pkg` en sitio sin descomprimir a disco.
- **Consciente de la batería.** La cadencia de frames cambia con la fuente de energía, y
  las salidas tapadas por una ventana a pantalla completa se saltan.
- **Presentación sin copias en X11 y Wayland.** Los frames se dibujan directamente en la
  ventana del escritorio mediante un swapchain EGL.

Detalles técnicos y estado por componente: [docs/REBUILD.md](docs/REBUILD.md).

## Alcance actual

Funciona:

- **Escenas y capas de imagen:** origen centrado ($y$ hacia arriba en viewport), escala,
  rotación horaria en radianes, alineación, blending, capas sólidas, imágenes a pantalla
  completa, texturas recortadas (`cropped`) y color de fondo.
- **Hojas de sprites animadas:** decodificación de atlas `TEXS0001` y `TEXS0003` con
  tiempos de fotograma y coordenadas UV calculadas dinámicamente.
- **Sistema de partículas completo:** simulación de lluvia, nieve, niebla, estrellas y
  halos luminosos; emisores (`boxrandom`, `sphererandom`), inicializadores (`lifetime`,
  `size`, `velocity`, `alpha`, `color`, `rotation`, `angularvelocity`), operadores
  (`movement` con gravedad y arrastre, `alphafade`, `oscillatealpha`, `oscillateposition`,
  `sizechange`), mezcla aditiva y translúcida, y renderizado por lotes GPU (`sprite` y
  `spritetrail`).
- **Materiales con `LIGHTING`:** luces puntuales (`lpoint`), término ambiente, specular
  GGX y máscaras PBR (`metallic`, `roughness`, `reflection`, `emissive`).
- **Materiales con `REFLECTION`:** reflejo en pantalla a partir del normal map, con
  `reflectivity`, `roughness` y `metallic`.
- **Pipeline de post-procesado y composición FBO (`_rt_FullFrameBuffer`):** aberración
  cromática, desenfoque y efectos con temporizador `g_Time` y samplers normalizados.
- **Soporte de paquetes `.pkg` en sitio:** lectura sin extracción en memoria para
  `scene.pkg`, `gifscene.pkg` y archivos `.pkg` arbitrarios.
- **Vídeos (MP4):** reproducidos con libmpv con decodificación compartida sin duplicación,
  incluidos los embebidos en texturas TEXB.
- **Formatos de textura:** DXT1, DXT3, DXT5, ARGB8888 y RG88 (luminancia-alfa).
- **Backends de pantalla:** `x11-desktop`, `x11-root`, `wayland` y `headless`.
- **Herramientas de línea de comandos:** comandos `run`, `list`, `steam`, `doctor`, `vfs` y `pkg`.
- **Perfiles de rendimiento:** `battery`, `balanced`, `smooth`.

Todavía no:

- Wallpapers web (CEF): se rechazan con un error explícito.
- Parallax de ratón y desplazamiento de cámara.
- Espectro de audio: no hay captura de PulseAudio ni PipeWire.
- Texto, sonido, esqueletos cinemáticos y scripts QuickJS.
- Luces de tipo spot, tube y directional: sólo se interpretan las puntuales.
- Traducción HLSL a GLSL: los materiales con shader propio usan el shader de sprites.

El estado de verificación y los comandos están en
[docs/REBUILD.md](docs/REBUILD.md#verification-status). Las convenciones de trabajo, en
[AGENTS.md](AGENTS.md).

## Estado del código

- Motor escrito en C++23 con semántica de valor, RAII estricto y retornos `std::expected`
  sin excepciones en flujos normales de ejecución.
- Cobertura de pruebas unitarias (`we_tests`) con 86/86 pruebas en verde, verificada
  con AddressSanitizer y LeakSanitizer (`ctest --preset asan`).
- El código legado se conserva en `src/WallpaperEngine` y `src/Steam` únicamente como
  referencia histórica y no forma parte del proceso de compilación.
