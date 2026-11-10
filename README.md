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
- **ZIP** — menú contextual → **Comprimir en ZIP** (uno o varios elementos de la misma carpeta; crea `nombre.zip` al lado, o `Comprimido.zip` si son varios; si el nombre ya existe añade la fecha y hora, p. ej. `Comprimido 2026-11-10 01.44.55.zip`, así nunca se pisan) y **Extraer aquí** sobre un `.zip` (también `.tar.gz/.xz/.bz2/.zst`, `.7z`, `.rar`). Corre en segundo plano con aviso de progreso y botón Cancelar, y en trabajos que tardan (más de ~0,7 s) una **barra de progreso en el borde inferior** con el porcentaje real (bytes leídos por `bsdtar` / tamaño total); usa `bsdtar` (libarchive). Un zip con una sola carpeta se extrae tal cual junto al archivo; si no, en una carpeta nueva con el nombre del archivo. Nunca sobrescribe (añade « (2)»), no deja archivos a medias si se cancela o falla y rechaza rutas inseguras (`..`).
- **Previsualizaciones** — thumbs FreeDesktop (imágenes + `.thumbnailer` por MIME, p. ej. Blender); mosaico 2×2 en carpetas (opt-in)
- **Tamaño de iconos** — slider en ⋮ (5 pasos: 48–80 px; por defecto 64)
- **Papelera** — el aviso superior muestra cuántos elementos hay y cuánto espacio ocupan (se calcula en segundo plano); «Vaciar papelera» lo repite en la confirmación
- **Sonidos** — avisos del tema de sonido del sistema (`freedesktop`: comprimir `device-removed`, extraer `device-added`, error, papelera vaciada) con `pw-play`/`paplay`. Activados por defecto; se desactivan en ⋮ → **Sonidos**
- **Iconos** — si el tema no tiene el icono exacto de un tipo, usa el genérico de archivo (y, sin tema, un símbolo propio); nunca queda un hueco en blanco
- **Menú contextual** — abrir, propiedades, icono de carpeta, abrir con…, comprimir/extraer
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

Con *Previsualizar* activo, `image://thumb` resuelve en este orden (estilo Dolphin):

1. Decode nativo Qt (`QImageReader`) para formatos imagen.
2. Caché shared FreeDesktop `~/.cache/thumbnails/{normal,large,…}/`.
3. Thumbnailers XDG (`~/.local/share/thumbnailers/*.thumbnailer` y `/usr/share/…`) por MIME (extensión antes que content-sniff; los `.blend` zstd cuentan como `application/x-blender`).

Caché propia de Seeker: `~/.cache/seeker/thumbs/` (~256 MiB, poda LRU). Los thumbs generados también se publican en la caché shared.

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
