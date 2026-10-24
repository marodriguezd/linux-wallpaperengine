# AGENTS.md

Guía para agentes que trabajan en este repositorio. Describe el rebuild en C++23
de la rama `feat/rebuild-from-scratch`. El código legado (`src/WallpaperEngine`,
`src/Steam`, `src/General`) no se compila; sólo se conserva como referencia.

Antes de cambiar código, lee:

- [ABOUT.md](ABOUT.md): qué es el proyecto y hacia dónde va.
- [docs/REBUILD.md](docs/REBUILD.md): arquitectura, componentes, backends y estado de verificación.
- [docs/textures/TEXTURE_FORMAT.md](docs/textures/TEXTURE_FORMAT.md) y [docs/rendering/](docs/rendering/): formatos de Wallpaper Engine que el motor interpreta.

## Estructura

| Ruta | Contenido |
| --- | --- |
| `src/core` | `std::expected` de errores, logging, matemáticas, LZ4, mapeo de ficheros |
| `src/steam` | VDF, descubrimiento de Steam, VFS sin distinguir mayúsculas, lector `.pkg` |
| `src/gfx` | Contexto EGL, shaders, texturas, framebuffers, renderer de sprites |
| `src/display` | Backends `x11-desktop`, `x11-root`, `wayland` y `headless` |
| `src/scene` | Parseo de proyectos y escenas, texturas TEXB, `SceneRenderer`, `VideoPlayer` (libmpv) |
| `src/power` | Monitor de batería y pacer de frames |
| `src/presets` | Perfiles de rendimiento |
| `src/app` | Motor: abre un wallpaper, construye el renderer y ejecuta el bucle |
| `src/cli` | Ejecutable `we` (`run`, `list`, `steam`, `doctor`, `vfs`, `pkg`) |
| `tests/` | Pruebas unitarias (`we_tests`), con `support/harness.hpp` y `support/temp_dir.hpp` |
| `cmake/`, `CMakePresets.json` | Opciones de compilación y presets `base`, `debug`, `release`, `relwithdebinfo`, `asan` |
| `third_party/` | stb y bcdec |

## Compilar y probar

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Para comprobar sanitizers, usa el preset `asan`, que compila con `-Werror` y ejecuta
las pruebas con detección de fugas. Ver [tests/CMakeLists.txt](tests/CMakeLists.txt)
para el entorno de LeakSanitizer.

Verificación de un cambio no trivial:

1. `cmake --build --preset debug` sin errores.
2. `ctest --preset debug` en verde.
3. Para escena o vídeo, `we run <ruta o id> --backend headless --frames 30` sin
   `warn` nuevos en el log.
4. Para cambios visuales, una captura real en la sesión X11 o Wayland afectada.

No des por válido un paso que no has ejecutado. Si una comprobación no se puede
ejecutar, dilo explícitamente.

## Estilo y convenciones

- C++23. Los fallos se devuelven como `std::expected<T, we::core::Error>`; no
  se lanzan excepciones en rutas normales.
- Los recursos GL, EGL, X11 y mpv son RAII: no dejes handles sueltos.
- Formato con [.clang-format](.clang-format) y análisis con [.clang-tidy](.clang-tidy).
  Ojo: `tools/formatting.sh` y `tools/linting.sh` todavía apuntan a `src/WallpaperEngine`
  y `src/Steam`, así que no cubren el rebuild. Aplica `clang-format` a los
  ficheros que tocas.
- No añadas supresiones de warnings ni desactives pruebas para que pase el build.
- Los cambios van a ficheros existentes salvo que haga falta un módulo nuevo.

## Pruebas

- Cada componente tiene su fichero en `tests/` (p. ej. `scene_tests.cpp`, `gfx_tests.cpp`).
- Las pruebas que usan GL usan contextos headless (`gfx::Context::create_headless`).
- Un test nuevo de escena o vídeo debe comprobar comportamiento observable
  (píxeles, `layer_count`, retornos de error), no sólo que no falle.

## Notas del dominio

- Las texturas TEXB pueden contener un MP4 en lugar de píxeles. Se detectan por
  `ftyp` en el byte 4 del payload y se reproducen como capa de vídeo.
- Las escenas usan origen arriba-izquierda e `y` hacia abajo; el renderer
  convierte a un espacio centrado con `y` hacia arriba. `origin` es el centro
  del objeto, `scale` multiplica el quad y `angles.z` está en radianes (un
  valor positivo gira en sentido horario en pantalla). Los `vec2`/`vec3` de
  Wallpaper Engine llegan como cadenas `"x y z"` y como arrays.
- Un `.pkg` se lee en sitio, sin extraerlo a disco.
- Las capas que el motor todavía no renderiza (partículas, texto, sonido,
  scripts, efectos y luces que no sean `lpoint`) se descartan y se cuentan en el
  log `scene: N image layers, K point lights, M objects not rendered`.

## Lo que no hacer

- No reintroduzcas código legado en el build.
- No subas secretos ni ficheros de Steam de usuario.
- No hagas commit ni push sin que el usuario lo pida.
