# retro_writer

Editor de texto en terminal con interfaz **Turbo Vision**: panel de archivos, editor y preferencias (código en `retro_writer_tv.cpp`, librería en `vendor/tvision`).

**Repositorio:** [https://github.com/pepinisillo/retro_writer](https://github.com/pepinisillo/retro_writer)

## Requisitos

- CMake 3.16 o superior  
- Compilador con **C++17** (`g++`)  
- **ncurses** (headers de desarrollo), por ejemplo en Debian/Ubuntu:

```bash
sudo apt install build-essential cmake libncurses-dev
```

## Compilar

```bash
cmake -S . -B build
cmake --build build -j
```

El ejecutable es `build/retro_writer_tv`.

## Ejecutar

```bash
./build/retro_writer_tv
./build/retro_writer_tv /ruta/a/tu_proyecto
```

Sin argumentos usa el **directorio actual** (`.`). Esa carpeta (directorio de **configuración**) **debe existir ya**; ahí se guardan `workspace.cfg` (última ruta del panel, archivo abierto, panel visible) y `appearance.cfg`. Abre archivos desde el panel (F4) o arranca con un buffer sin título hasta que guardes.

Los menús y la barra de estado muestran atajos (panel de archivos, preferencias, salida, etc.).


