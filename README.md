# retro_writer

Editor de novelas en terminal con interfaz **Turbo Vision** (código en `retro_writer_tv.cpp`, librería en `vendor/tvision`).

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

Sin argumentos usa el **directorio actual** (`.`). La carpeta de proyecto **debe existir ya**; el programa no crea la raíz del proyecto por ti. La subcarpeta `novelas/` se crea cuando hace falta (al crear o abrir novelas).

Los menús y la barra de estado muestran atajos (novelas, capítulos, panel de archivos, preferencias, salida, etc.).


