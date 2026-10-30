# Seeker

Explorador de archivos **Material You** pensado para [Sylphrena](https://github.com/iMacnucintosh/sylphrena-shell).

- Producto: **Seeker**
- Nombre en el lanzador: **Archivos** (`seeker.desktop`)
- Stack v0: **Qt 6 Quick / QML + C++** (qmake). El plan original apuntaba a Rust+cxx-qt; en esta máquina no había `rustc`/`cmake`/`clang`, así que el v0 usa C++ nativo de Qt (mismo UI QML y D-Bus). Se puede añadir un crate Rust después sin cambiar la capa QML.

## Funciones v0

- Lugares (Inicio, Escritorio, Documentos, Descargas, …, Papelera)
- Navegación con historial atrás/adelante
- Abrir ficheros (`QDesktopServices` / `xdg-open`)
- Copiar / cortar / pegar / papelera freedesktop
- Menú de acciones con popup animado desde el botón
- Tema desde `~/.config/seeker/theme.json` (escrito por Sylphrena) o fallback `~/.local/state/sylphrena/scheme.json`
- Stub D-Bus `org.freedesktop.FileManager1`

## Build

```bash
cd ~/Desarrollo/seeker
qmake6 PREFIX=$HOME/.local
make -j$(nproc)
make install
```

Binario: `~/.local/bin/seeker`

## Tema Sylphrena

`scripts/sylphrena-quiet.py` escribe `~/.config/seeker/theme.json` en cada apply de esquema. Seeker recarga en caliente.

## Licencia

MIT
