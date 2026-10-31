# Seeker

Explorador de archivos **Material You** para [Sylphrena](https://github.com/iMacnucintosh/sylphrena-shell).

- Producto: **Seeker** · lanzador: **Archivos**
- Stack: **Qt 6 Quick / QML + C++** (qmake)
- Binario: `~/.local/bin/seeker` · config: `~/.config/seeker/`

## Funciones

- **Lugares** — Inicio, Papelera y favoritos anclados (pins XDG por defecto, reordenables; Inicio/Papelera fijos)
- **Navegación** — atrás/adelante; subir y actualizar opcionales (menú ⋮); historial
- **Barra de dirección** — edición inline, refresh en la pastilla, sugerencias en jerarquía hacia arriba; rueda del ratón encima = adelante/atrás

- **Vistas** — cuadrícula / lista / detalles (rueda + menú split)
- **Selección** — clic, Ctrl+clic, marco; RMB conserva multi-selección
- **Portapapeles** — copiar / cortar / pegar; papelera freedesktop; deshacer
- **Arrastre** — mover/copiar entre carpetas; soltar en favoritos para anclar
- **Previsualizaciones** — thumbs de imágenes; mosaico 2×2 en iconos de carpeta (opt-in)
- **Menú contextual** — abrir, propiedades, icono de carpeta, abrir con…
- **Chrome** — botones nueva carpeta / subir / actualizar mostrables u ocultables desde ⋮
- **Tema** — Material You en vivo desde Sylphrena (`theme.json`)
- **Portal** — FileChooser xdg-desktop-portal + stub `FileManager1`

## Build

```bash
cd ~/Desarrollo/seeker
qmake6 PREFIX=$HOME/.local
make -j$(nproc)
make install
```

## Tema Sylphrena

`scripts/sylphrena-quiet.py` escribe `~/.config/seeker/theme.json` en cada apply. Seeker recarga en caliente.

## Miniaturas

Con *Previsualizar* activo: `image://thumb` → caché JPEG en `~/.cache/Sylphrena/Seeker/thumbs/` (~256 MiB, poda LRU).

## FileChooser (portal)

```bash
make install
./portal/install-portal.sh
```

```ini
# ~/.config/xdg-desktop-portal/portals.conf
[preferred]
default=hyprland;seeker;gtk
org.freedesktop.impl.portal.FileChooser=seeker
```

Prueba sin portal: `seeker --pick --pick-mode=folder --pick-result=/tmp/pick.json`

## Licencia

MIT
