# Handoff: transforms corregidos, autosize/UV y siguiente paso (offscreen + efectos)

**Fecha:** 2026-10-25
**Repo:** `/home/marodriguezd/Github/linux-wallpaperengine`
**Rama:** `feat/rebuild-from-scratch`
**Git:** commit local de la sesión con el rebuild, los fixes y este handoff;
sin push (ver `git log -1`). El árbol queda limpio; `AGENTS.md`, `third_party/`,
`build/` y `.freebuff/` siguen ignorados. No usar `git add -A` ni tocar trabajo
ajeno. Este fichero está en la raíz del proyecto a propósito: en `/tmp` se
perdería al apagar el ordenador.

Este documento continúa el handoff anterior de transforms; lo relevante de
aquella sesión está resumido aquí. No dupliques el contenido de `AGENTS.md`,
`ABOUT.md`, `docs/REBUILD.md`, `docs/textures/TEXTURE_FORMAT.md` ni
`docs/rendering/`: consúltalos.

## Cerrado en esta sesión

### 1. Transformaciones y espacio de escena

- `vec2`/`vec3` aceptan las cadenas `"x y z"` de Wallpaper Engine y los arrays
  JSON. Cubre `origin`, `scale`, `angles`, `size`, `parallaxDepth` y cámara.
- Convención única: `origin` es el centro del objeto en espacio de escena con
  origen arriba-izquierda e `y` hacia abajo; el renderer convierte a un espacio
  centrado con `y` hacia arriba. Capas y luces usan la misma conversión.
- `angles` en radianes; `angles.z` rota el quad y un valor positivo gira en
  sentido horario en pantalla. La rotación se aplica en el espacio de mundo
  (ángulo negado) y los tangentes/bitangentes giran con el quad.
- La proyección ortográfica queda centrada en el tamaño de escena. El
  `eye`/`center` guardado por el editor no desplaza las capas: en el motor
  original la traslación de la proyección y el `lookAt` se cancelan.
- `orthogonalprojection.auto` usa los bounds reales de los quads transformados
  (escala y rotación incluidas), con tolerancia `1e-3` para que `cos(pi/2)` no
  infle el tamaño un píxel.

### 2. `autosize` / `cropoffset` y UV

- La cabecera TEX guarda el canvas original (`texture_width/height`), pero el
  mipmap y el bitmap subido contienen ya la imagen recortada (`width/height`).
  Las capas muestrean UV 0..1; antes se muestreaba sólo
  `width/texture_width`, dejando las texturas auto-recortadas a medias.
- `cropoffset`/`autosize` son metadatos de autoría para el caso estático: la
  geometría sale de `size` del objeto, luego width/height del modelo y luego
  las dimensiones del bitmap recortado. No hace falta recortar UV en runtime.
- Se eliminaron `TextureData::uv_width()/uv_height()`; el campo
  `texture_width/height` queda documentado como canvas de origen.

### 3. Tests y documentación

- 6 tests nuevos en `tests/scene_tests.cpp`: posición por `origin`, escala,
  rotación (y radianes), parseo de cadenas, textura con canvas mayor que el
  bitmap y bounds de proyección automática con rotación.
- Total **80 tests**; los 74 anteriores siguen intactos, incluidos
  LIGHTING/REFLECTION.
- Documentación actualizada: `docs/REBUILD.md` (sección "Scene space and
  transforms" y estado), `ABOUT.md` y `AGENTS.md`.

**Ficheros tocados:** `src/scene/src/scene.cpp`,
`src/scene/src/scene_renderer.cpp`, `src/scene/include/we/scene/texture.hpp`,
`tests/scene_tests.cpp`, `docs/REBUILD.md`, `ABOUT.md`, `AGENTS.md`.

## Verificación ejecutada

| Comprobación | Resultado |
| --- | --- |
| `cmake --build --preset debug` | OK |
| `ctest --preset debug` | 80/80 |
| `cmake --build --preset release` + `ctest` | 80/80 |
| ASan+UBSan con `detect_leaks=0` | 80/80, sin diagnósticos ASan/UBSan |
| `ctest --preset asan` | Falla sólo por la fuga conocida de `libcuda` vía mpv: 6509 B en 135 allocs, todo dentro de `libcuda` |
| `we run 3615154110 --backend headless --frames 90` | Exit 0, 0 warns, `scene: 10 image layers, 2 point lights, 6 objects not rendered` |

- No se tocó `tests/lsan.supp` ni se añadió ninguna supresión.
- Capturas headless revisadas: `3615154110` (fondo completo y marco de cajas en
  su sitio), `2638079178` (texturas auto-recortadas completas) y `3616910868`
  (fondo 3840x2160 completo).

## Siguiente paso: framebuffer offscreen y cadena de efectos

### Estado actual del pipeline

- `SceneRenderer::build` descarta las capas cuyo `texture_path` empieza por
  `_rt_` (en `parse_image`, dentro de `scene.cpp`, se registra un info y se
  devuelve `nullopt`). Por eso la "Post-processing Layer" de `3615154110` no
  llega al renderer: el log dice `internal render target
  '_rt_FullFrameBuffer' is not implemented, layer skipped`.
- `SceneRenderer::render` dibuja directamente contra el framebuffer por defecto
  con `gfx::Renderer::begin/end`. `Renderer::capture_frame` copia el frame con
  `glCopyTexSubImage2D` para REFLECTION; al introducir un FBO de escena habrá
  que capturar ese FBO, no el default.
- `gfx::Framebuffer` ya existe (`src/gfx/include/we/gfx/framebuffer.hpp`),
  con `rgba8`/`rgba16f`, `bind()`, `resize()` y `view()`. Lo usa el vídeo de
  libmpv; sirve como base para `_rt_FullFrameBuffer`.

### Datos de Wallpaper Engine útiles

- Assets instalados:
  `/home/marodriguezd/.steam/steam/steamapps/common/wallpaper_engine/assets`
  (también hay copia bajo `~/.local/share/Steam/...`).
- `models/util/fullscreenlayer.json` es `fullscreen: true`, `passthrough: true`
  y su material `materials/util/fullscreenlayer.json` declara
  `textures: ["_rt_FullFrameBuffer"]`, shader `passthrough`, blending
  `translucent`.
- `3615154110` aplica `effects/chromaticaberration/effect.json` a esa capa con
  `constantshadervalues`: `ui_editor_properties_center: "0.5 0.5"`,
  `ui_editor_properties_center_falloff: 1`, `ui_editor_properties_strength:
  0.5`.
- `2638079178` es buena segunda prueba: tiene capas "Blur" y "Dot" que también
  usan `_rt_FullFrameBuffer` y el efecto `blur`.
- Los `effect.json` de los assets son autocontenidos bajo
  `assets/effects/<nombre>/` (materiales y shaders incluidos). Un effect define
  `passes` con `material`, opcionalmente `target` y `bind` (nombre + índice), y
  `fbos` con `name`, `scale` y `format`. Ejemplo multi-paso:
  `assets/effects/blur/effect.json` (4 pasadas, `_rt_QuarterCompoBuffer1/2`,
  downsample + gaussian x/y + combine). Chromatic aberration es una sola pasada
  y es el mejor primer objetivo.
- Los shaders de efectos son GLSL, pero **no son GLSL plano**: incluyen
  `#include "common.h"`, combos (`#if MODE`, `#if VARIATION`, `#if MASK`) y
  macros como `mul`, `CAST2` y `texSample2D`. Los nombres de constantes de la
  escena son propiedades de UI (`ui_editor_properties_strength`) y en el shader
  aparecen como uniforms (`u_Strength`) con anotaciones en comentarios. Hay que
  implementar un preprocesador mínimo o pasar los defines/constantes a mano
  para el primer efecto. El bloqueo no es HLSL para estos assets; el fallback a
  sprite shader sigue aplicando a materiales con shader propio no soportado.
- El orden importa: la capa de post-proceso debe aplicarse después de las capas
  de escena; en `3615154110` es el último objeto visual antes de los sonidos.
  En `2638079178` hay dos capas `_rt_` ("Blur", "Dot") y conviene revisar su
  orden en `scene.json`.

### Plan sugerido (orden)

1. Leer `docs/rendering/IMAGE_OBJECTS.md`, `GENERAL_SETTINGS.md`,
   `CAMERA_SETTINGS.md` y `TEXTURE_FORMAT.md`, y el pipeline actual de
   `SceneRenderer`/`Renderer`.
2. Crear un `_rt_FullFrameBuffer` real (RGBA8 o RGBA16F) del tamaño de escena;
   renderizar las capas de escena en él y volcarlo al framebuffer por defecto.
   Mantener la ruta actual sin post-proceso para escenas sin `_rt_`.
3. Representar las capas `_rt_FullFrameBuffer` en el modelo de escena en vez de
   descartarlas; implementar primero chromatic aberration (una pasada) y
   verificar visualmente `3615154110`.
4. Generalizar el parser de `effects` (`file`, `passes`, `constantshadervalues`)
   y añadir `target`/`bind`/`fbos` para blur/bloom. Empezar por
   `2638079178` (blur) y luego bloom (`general.bloom` de las escenas).
5. Sustituir la captura de REFLECTION por el FBO de escena y comprobar que sus
   tests siguen verdes.
6. Tests observables: píxel antes/después del post-proceso, `layer_count` con
   la capa `_rt_`, y que REFLECTION sigue funcionando.

## Pendiente después de efectos

- **Hojas de sprites (`SPRITESHEET`)**: la capa "LUCY KUSHINADA" de
  `3615154110` tiene textura 1080x1214 y objeto 1080x607; ahora se dibujan los
  dos frames aplastados en el quad. `docs/REBUILD.md` ya lo documenta como no
  implementado. Habrá que inferir el layout/animación (no hay TEXS con frames en
  esa textura; el combo lo activa el material).
- Partículas, texto (necesita rasterizador de fuentes; los assets usan OTF/CFF y
  no hay FreeType ni stb_truetype en el repo), sonido (libmpv ya está; respetar
  `--audio`/`--volume`) y scripts QuickJS.
- Parallax de ratón (`parallaxDepth`, `cameraparallax`) y luces spot/tube/
  directional.

## Restricciones y trampas

- No hacer commit/push sin que el usuario lo pida. No tocar cambios de otros
  agentes.
- Usar `./build/debug/bin/we`; `./build/bin/we` es un binario viejo y no
  refleja el código actual.
- ASan: la única fuga es `libcuda` (6509 B/135 allocs). No editar
  `tests/lsan.supp`; preguntar antes de cualquier supresión. Para comprobar el
  resto de la suite bajo ASan se puede usar `detect_leaks=0` (sin fichero de
  supresión).
- `.clang-format` no coincide con el estilo del rebuild; seguir el estilo del
  fichero que se toca, no reformatear el árbol.
- `--frames 30` puede dejar vídeos sin decodificar; usar 90 para capturas.
- Para inspeccionar escenas: `we pkg <scene.pkg> --list` y `--cat`; los
  `scene.json` están dentro del `.pkg`, no en disco. Repetir la extracción a
  `/tmp/scenes` si hace falta (puede haberse perdido al apagar).
- Verificación de un cambio no trivial: build debug, `ctest`, ASan con la
  salvedad de libcuda, `we run 3615154110 --backend headless --frames 90` sin
  warns nuevos y captura visual en la sesión X11/Wayland si el cambio es visual.

## Skills sugeridos

- Del repo, leer antes de usar: `.agents/skills/cpp-pro/SKILL.md`,
  `.agents/skills/modern-cpp/SKILL.md` y `.agents/skills/cmake/SKILL.md`.
- De las skills de cuenta, llamar a `code-review` para revisar el árbol antes de
  cerrar (no hay commits; el punto de comparación es el working tree),
  `antislop-code` para limpiar comentarios nuevos sin tocar el código y
  `handoff` al terminar la próxima sesión.

## Prompt de cinco líneas para el siguiente agente

```text
Repo linux-wallpaperengine (rama feat/rebuild-from-scratch): lee HANDOFF.md y todos los markdowns que referencia (AGENTS.md, ABOUT.md, docs/REBUILD.md, docs/rendering/, docs/textures/) antes de planificar.
Objetivo: implementar el render target offscreen `_rt_FullFrameBuffer` y la cadena de post-proceso de scene.json; empieza por chromatic aberration de 3615154110 y sigue con blur/bloom de 2638079178, sustituyendo la captura de REFLECTION por el FBO de escena.
Mantén LIGHTING/REFLECTION y los 80 tests; añade tests observables del post-proceso y verifica con build debug, ctest, asan y `we run 3615154110 --backend headless --frames 90` sin warns nuevos.
No hagas commit/push sin que el usuario lo pida ni toques cambios ajenos; usa build/debug/bin/we y no modifiques tests/lsan.supp.
ASan sólo falla por libcuda (6509 B/135 allocs); repórtalo y pregunta antes de cualquier supresión.
```
