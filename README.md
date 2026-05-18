# retro_writer_tv
<img width="1920" height="1080" alt="full-20260516-230543" src="https://github.com/user-attachments/assets/be3cf54e-eb51-4686-8985-d82c032cd0d4" />

Editor TUI (terminal) para escritura y manejo de recursos visuales, **construido** sobre **Turbo Vision**.

**Incluidos:**
- Editor de texto principal.
- Panel navegador de archivos.
- Ventanas Mini con vista previa (fondo/personajes).
- Diálogos para gestión de biblioteca visual y escena por capítulo.
- Persistencia de apariencia, layout y sesión.

---

## 1) Tecnologías usadas

- **C++17** (aplicación principal).
- **Turbo Vision** (`vendor/tvision`) para la UI TUI (ventanas, menús, diálogos, eventos, scrollbars).
- **STB Image** (`vendor/stb`) para carga de imágenes raster (PNG/JPG/WebP/etc.).
- **ncurses** como backend de terminal.
- **Kitty Graphics Protocol** (opcional) para previews de alta fidelidad en terminal Kitty.
- **libsixel** (opcional) para salida Sixel en terminales compatibles.
- **CMake** como sistema de build.

---

## 2) Estructura del proyecto

- `retro_writer_tv.cpp`: lógica completa de la app (UI, editor, previews, persistencia, diálogos visuales).
- `vendor/tvision/`: librería Turbo Vision vendorizada.
- `vendor/stb/`: dependencia de carga de imágenes.
- `appearance.cfg`: preferencias visuales y de comportamiento.
- `workspace.cfg`: sesión/layout/estado restaurable.
- `visuals.cfg`: biblioteca visual global (fondos/personajes y variantes).
- **`assets/`** (en la novela/proyecto de trabajo): carpeta en la que **suelen almacenarse** las imágenes **importadas/normalizadas** cuando **se usa** la biblioteca visual (“Agregar elemento visual”). **No forma parte** del repo del IDE; **se crea** o **se usa** en el árbol del proyecto narrativo.

### Carpeta del IDE vs carpeta raíz de la historia

**Deben distinguirse** dos cosas:

1. **Carpeta del programa** (por ejemplo el clon de `retro_writer/`): aquí **viven** el código, `vendor/`, el `build/` y **puede compilarse** el binario. **No es** obligatorio que aquí exista `visuals.cfg` ni `assets/` de la novela.
2. **Carpeta raíz de la obra** (por ejemplo `meep/`): al ejecutarse con argumento, **se pasa** esa ruta como “directorio de proyecto”. Ahí **se espera** (cuando **se trabaja** con visuales) un `visuals.cfg` coherente y, según el propio `visuals.cfg`, una carpeta **`assets/`** **junto a** ese archivo (por la línea típica `assetRoot assets` → rutas como `assets/characters/...` **resueltas** desde el directorio donde **está** `visuals.cfg`).

**No se crean** `appearance.cfg`, `workspace.cfg` ni `visuals.cfg` durante el build; **se crean o actualizan** al ejecutar la app y usar preferencias, layout o biblioteca visual.

### Estructura recomendada de una novela existente

Para que otra máquina o otra persona **pueda abrir** la misma historia sin adivinar rutas, **se recomienda**:

- Una **carpeta raíz** por obra (ej. `meep/`).
- Dentro: **`visuals.cfg`** en esa raíz.
- **`assets/`** en esa misma raíz (o la estructura que indique `assetRoot` en `visuals.cfg`).
- **Capítulos separados por subcarpetas** (ej. `cap1/`, `cap2/`), cada una con su `.txt` y, si aplica, `chapter_scene.cfg` u otros archivos de escena por capítulo.

Ejemplo ilustrativo (nombres flexibles):

```text
meep/
  visuals.cfg
  assets/
    backgrounds/...
    characters/...
  cap1/
    capitulo.txt
    chapter_scene.cfg
  cap2/
    ...
```

**Debe lanzarse** la app apuntando a esa raíz cuando **se desee** que ahí **se lean/escriban** `appearance.cfg`, `workspace.cfg` y `visuals.cfg` juntos:

```bash
./build/retro_writer_tv /ruta/completa/meep
```

---

## 3) Requisitos

- CMake 3.16+
- Compilador C++17
- Headers de ncurses

Ejemplo Debian/Ubuntu:

```bash
sudo apt install build-essential cmake libncurses-dev pkg-config
```

Arch Linux (`pacman`):

```bash
sudo pacman -Syu --needed base-devel cmake ncurses pkgconf
```

Si **se desea** soporte Sixel:

```bash
sudo apt install libsixel-dev
```

En Arch **puede intentarse** (si el paquete **está** en los repos habilitados):

```bash
sudo pacman -S --needed libsixel
```

Si **no** **está** disponible, **puede buscarse** en AUR o **dejarse** Sixel desactivado.

---

## 4) Compilación

```bash
cmake -S . -B build
cmake --build build -j$(nproc)
```

Ejecutable generado:

```bash
build/retro_writer_tv
```

---

## 5) Ejecución

### Sin argumento

```bash
./build/retro_writer_tv
```

**Se usa** `.` como directorio de proyecto/configuración.

### Con argumento

```bash
./build/retro_writer_tv /ruta/a/tu/proyecto
```

**Debe usarse** como `/ruta/a/tu/proyecto` la **carpeta raíz de la historia** cuando **se trabaje** con capítulos y visuales ahí (ej. `meep/`). En ese directorio **se leen y escriben** `appearance.cfg`, `workspace.cfg` y `visuals.cfg` (no **se mezclan** por defecto con la carpeta solo del código fuente del IDE, salvo que **se ejecute** sin argumento desde esa carpeta).

### Otra computadora / copiar proyecto

1. **Deben instalarse** las dependencias (sección 3) y **debe compilarse** el proyecto (sección 4).
2. **Debe copiarse** **toda la carpeta raíz de la novela** (ej. `meep/` completa), incluyendo:
   - `visuals.cfg`
   - **`assets/`** (las rutas de `visuals.cfg` **son relativas** a la raíz de la obra según `assetRoot`; si **falta** contenido, las imágenes **no se resolverán**)
   - subcarpetas de capítulos (`cap1/`, `cap2/`, …) con `.txt` y `chapter_scene.cfg` si **se usan**
3. Opcional y habitualmente personal: `appearance.cfg` y `workspace.cfg` (colores, layout, última sesión). **Pueden no copiarse** entre máquinas si **se prefiere** empezar con layout por defecto.

**Solo texto:** la app **puede usarse** sin `assets/` ni `visuals.cfg`.  
**Con visuales:** **se necesita** un `visuals.cfg` coherente y la carpeta **`assets/`** (u otras rutas válidas); si **falta** y las entradas **apuntan** a archivos bajo `assets/`, las vistas previas **fallarán** hasta que **se restaure** el árbol o **se vuelva a importar**.

---

## 6) Flujo de uso diario

1. **Se abre** la app.
2. **Se navega** por archivos en el panel izquierdo.
3. **Se abre/edita** `.txt`.
4. **Se gestionan** visuales desde menús:
   - `Escena visual` para asignación de fondo/personajes por capítulo.
   - `Agregar elemento visual` para carga/normalización de recursos en la biblioteca.
5. **Puede guardarse/restaurarse** layout cuando **se desee** congelar una distribución de ventanas.

---

## 7) Menús y funcionalidades principales

- **Windows**
  - Mostrar/ocultar panel de archivos (`Ctrl+E`).
  - Guardar layout.
  - Restaurar layout.
- **Visual**
  - Escena visual (capítulo actual).
  - Agregar elemento visual (biblioteca).
- **Preferencias**
  - Colores/fondo/autoguardado.

En la barra de estado **se reflejan** los atajos activos.

---

## 8) Persistencia y archivos de configuración

`appearance.cfg`, `workspace.cfg` y `visuals.cfg` **son** archivos de configuración (texto): **no** **forman parte** del binario ni **se generan** con el build; **se crean o actualizan** al ejecutar la app y usar preferencias, layout o visuales.

### `appearance.cfg`

<img width="1202" height="591" alt="region-20260517-230654" src="https://github.com/user-attachments/assets/dab414f9-0ed3-4ab2-bc84-0c64a2be2624" />

**Se almacenan** preferencias visuales y de render:

- `textColor`, `backColor`
- `patternUtf8`
- `miniPreviewMaxSide`
- `miniPreviewSixel`
- `miniPreviewSixelColors`
- `miniPreviewKittyNative`
- `editorBold`
- `autoSaveIntervalSec`
- `kittyCellHeightPx`

### `workspace.cfg`

**Se guarda** estado de sesión y geometrías:

- último archivo/ruta
- panel visible y split
- geometría de editor/panel/minis
- snapshot “guardado” de layout (Save/Restore)
- altura de celda Kitty guardada para zoom persistente

### `visuals.cfg`

Biblioteca visual global:
- IDs de personajes/fondos
- variantes por ID
- rutas relativas a medios; al **importarse** desde “Agregar elemento visual” el flujo actual **apunta** a la subcarpeta **`assets/`** bajo la base visual de la novela (no **es necesario** crear `assets/` a mano antes del primer import si el programa **puede escribir** en ese árbol).

### Carpeta `assets/`

- **Constituye** la **bóveda de imágenes** que la biblioteca visual **suele usar** tras la importación.
- **No es obligatoria** para edición de texto plano.
- **Resulta importante** si ya existe un `visuals.cfg` que **referencia** archivos dentro de `assets/`, o si **se desea** que todo quede autocontenido al mover el proyecto a otra máquina: **deben copiarse** **`visuals.cfg` + `assets/`** juntos.
- **Debe recordarse**: con `assetRoot assets` en `visuals.cfg`, la carpeta **`assets/`** **esperada** **suele estar** al mismo nivel que `visuals.cfg` (raíz de la novela), **no** dentro de `retro_writer/` salvo que **se haya elegido** esa estructura a propósito.

---

## 9) Render de previews (Mini y diálogos)

El pipeline **se selecciona** según entorno/configuración:

1. **Kitty nativo** (mejor calidad en terminal Kitty).
2. **Sixel** (si **está habilitado** y **fue compilado**).
3. **Fallback truecolor/U+2580**.

**Se aplica** tanto en ventanas Mini como en selectores visuales.

---

## 10) Variables de entorno relevantes

- `RETRO_WRITER_KITTY_FORCE=1`  
  **Se fuerza** el uso de Kitty incluso en escenarios donde normalmente **queda desactivado** (ej. multiplexer).

- `RETRO_WRITER_KITTY_SAVE_ZOOM=0`  
  **Se desactiva** la persistencia del zoom (altura de celda) en Kitty.

- `RETRO_WRITER_ALLOW_SIXEL=1`  
  **Se permite** Sixel si la app y el terminal lo soportan.

- `RETRO_WRITER_FORCE_SIXEL=1`  
  **Se fuerza** la vía Sixel cuando sea posible.

---

## 11) Arquitectura interna (alto nivel)

`retro_writer_tv.cpp` **está organizado** por bloques:

1. Utilidades base (rutas, color, parseo, carga de imagen).
2. Views personalizadas (`NavigatorListView`, previews, botones planos, etc.).
3. Editor (`RetroFileEditor`, `RetroEditWindow`).
4. Diálogos visuales (escena/biblioteca).
5. `RetroWriterTVApp` (event loop, comandos, layout, persistencia).
6. `browseImageFileAbs` (explorador de imagen integrado con preview).
7. `main`.

### Clases clave

- `RetroWriterTVApp`: **orquestación** de estado global, eventos y persistencia.
- `RetroFileEditor`: comportamiento del editor (wheel, ajuste de autoscroll).
- `RetroEditWindow`: marco y márgenes internos del editor.
- `NavigatorListView`: lista de navegación con teclado/ratón/filtro y scroll.
- `ImageNavPickerDialog`: selector de imagen con lista + preview + scrollbar.

---

## 12) Integrar o reutilizar en otra app

Si **se desea** portar esta lógica a otro proyecto:

1. **Aislamiento** del núcleo de UI  
   - **Extracción/copia** de views reutilizables (`NavigatorListView`, previews).
2. **Conservación** del modelo de persistencia  
   - **Separación** de lectura/escritura de `appearance/workspace/visuals` en módulos.
3. **Conservación** de contratos de comandos  
   - Los `cm...` de Turbo Vision **constituyen** el bus de acciones; **deben documentarse** los IDs.
4. **Encapsulación** del render de imagen  
   - Kitty/Sixel/fallback **detrás** de una interfaz única.
5. **Separación** del dominio narrativo  
   - La escena de capítulo (fondo/personaje/variante) **puede ubicarse** en un servicio aparte.

Recomendación práctica: **dividir** `retro_writer_tv.cpp` en módulos (`editor`, `visuals`, `persistence`, `ui/widgets`) antes de una integración grande.

---

## 13) Solución de problemas

- **No se ve calidad Kitty**
  - **Debe verificarse** `miniPreviewKittyNative 1` en `appearance.cfg`.
  - **Debe ejecutarse** dentro de Kitty real.
  - Si **se usa** multiplexer, **puede probarse** `RETRO_WRITER_KITTY_FORCE=1`.

- **Sixel no aparece**
  - **Debe compilarse** con `libsixel`.
  - **Debe habilitarse** `miniPreviewSixel 1` y `RETRO_WRITER_ALLOW_SIXEL=1`.

- **Layout raro al abrir**
  - **Debe revisarse** `workspace.cfg`.
  - **Puede usarse** “Restaurar layout” o **puede limpiarse** ese archivo si **se desea** reset completo.

---

## 14) Desarrollo y contribución

- Estilo: C++17 + Turbo Vision clásico.
- Los cambios visuales **deben probarse** en terminal real (idealmente Kitty).
- Tras cambios: **debe compilarse** y **validarse** navegación/scroll/dialogs.
- **Deben mantenerse** comentarios orientados a mantenimiento (qué/por qué, no obviedades).

---

## 15) Licencia

**Debe revisarse** la licencia del repositorio y de dependencias vendorizadas (`vendor/tvision`, `vendor/stb`) antes de redistribución en un producto externo.

---

## 16) Git y GitHub (qué versionar)

En la raíz de este repositorio el archivo **`.gitignore`** **incluye** `appearance.cfg`, `workspace.cfg` y `visuals.cfg` para que, tras **`git clone`**, **no** **viajen** configuraciones personales de máquina: **se obtiene** un clon limpio y esos tres archivos **se crean o reescriben** al usar la app en la nueva compu.

Flujo típico para probar en otra máquina:

```bash
git clone https://github.com/TU_USUARIO/TU_REPO.git
cd TU_REPO
cmake -S . -B build
cmake --build build -j$(nproc)
./build/retro_writer_tv
```

- **`build/`**: **se ignora** (artefactos de compilación).
- **Historia aparte** (ej. carpeta `meep/`): si **se versiona** en otro repo o **se copia** aparte, ahí **sí** **pueden** existir `visuals.cfg` y `assets/`; ese `.gitignore` **solo afecta** al árbol del clon del IDE, no a carpetas externas no añadidas al repo.

Si **se desea** publicar una novela completa **dentro** del mismo repo, **debe** quitarse del `.gitignore` lo que **deba** subirse (o **usarse** `git add -f`) y **documentarse** en el README; en el flujo por defecto **no** **se suben** las tres configuraciones de la raíz del IDE.

---

## 17) Crear con IA (uso rápido)

- Menú: **Crear con IA → Crear con IA...**
- Flujo mínimo:
  1. Escribir **Nombre carpeta**.
  2. Pulsar **Solicitar** (obligatorio).
  3. Revisar **Idea** y **Días sugeridos**.
  4. Pulsar **Crear**.

Notas:
- El tipo de prompt IA **se elige random automáticamente** en cada solicitud.
- Si no se pulsa **Solicitar**, el botón **Crear** muestra error y no crea proyecto.
- Configuración de endpoints/modelo: **Crear con IA → Configurar IA...**
  - Ahí se define a qué IA se conecta la app (endpoints de idea/feedback, modelo, API key y timeout).
  - Si **Solicitar** falla o responde mal, lo primero es revisar esta pantalla.

---

## 18) Botón [ txt ] y fuentes ASCII

- Al pulsar **[ txt ]** se abre un diálogo para:
  - nombre de archivo `.txt`,
  - texto opcional para título ASCII,
  - lista scrolleable de fuentes (4 visibles),
  - botón **Ver vista previa** (abre ventana aparte),
  - botones **Aceptar** y **Cancelar**.

- Si el texto ASCII queda vacío, el archivo se crea normal.
- Si hay texto ASCII, se renderiza con la fuente seleccionada y se escribe al inicio del archivo.

### Carpeta de fuentes

- Las fuentes se cargan desde: **`ascii_fonts/`**
- Formatos reconocidos: **`.flf`** y **`.tlf`**
- Si no hay fuentes válidas, el diálogo muestra error.

### Archivos auxiliares

- Renderer: **`tools/render_ascii.py`**
- Motor local: **`tools/pyfiglet/`**
