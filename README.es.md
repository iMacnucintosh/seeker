<h1 align="center">Seeker</h1>

<p align="center"><a href="./README.md">English</a> · <strong>Español</strong></p>

<p align="center">
El explorador de archivos <strong>Material You</strong> de <a href="https://github.com/iMacnucintosh/sylenara-shell">Sylenara</a>: vista rápida con Espacio, una carpeta que se actualiza sola, ZIP en un clic y los colores de tu escritorio, en vivo.
</p>

<p align="center"><img src="docs/screenshot.webp" width="760" alt="Seeker con la vista rápida enseñando una foto"></p>

- Producto: **Seeker** · nombre en el lanzador: **Archivos**
- Stack: **Qt 6 Quick / QML + C++** (qmake)
- Binario: `~/.local/bin/seeker` · config: `~/.config/seeker/`

## Funciones

- **Lugares**: Inicio, Papelera y favoritos anclados (las carpetas XDG por defecto, reordenables; Inicio y Papelera fijos).
- **Navegación**: atrás/adelante; subir y actualizar opcionales (menú ⋮); historial.
- **Barra de dirección**: edición inline, refresh en la pastilla, sugerencias subiendo por la jerarquía; la rueda del ratón encima va adelante/atrás.
- **Vistas**: cuadrícula / lista / detalles (rueda + menú split). Slider de tamaño de iconos en ⋮ (5 pasos: 48–80 px; por defecto 64).
- **Selección**: clic, Ctrl+clic, marco; el clic derecho conserva la multiselección.
- **Portapapeles**: copiar / cortar / pegar; papelera freedesktop; deshacer.
- **Arrastre**: mover/copiar entre carpetas; soltar en favoritos para anclar.
- **Vista rápida** (Espacio, como en macOS): sale del icono del archivo y enseña solo el archivo. **Imagen** con esquinas redondeadas; **vídeo** y **audio** reproduciéndose solos con una barra mínima (play/pausa, progreso y tiempo; el volumen es el del sistema; el audio con un icono de música); **PDF** página a página (botones, rueda, `RePág`/`AvPág`; las páginas las dibuja `pdftoppm` y se guardan en caché); **texto** con sus primeras líneas. Con las flechas pasa al archivo anterior o siguiente sin cerrarse. Una carpeta, un archivo de otro tipo o varios seleccionados abren **Propiedades**. Se cierra con Espacio, `Esc` o un clic fuera.
- **Actualización en vivo**: la carpeta abierta se actualiza sola cuando otra app añade, borra o cambia archivos (solo esa carpeta, sin recargarla entera: se conservan la selección y el scroll).
- **ZIP**: menú contextual → **Comprimir en ZIP** (uno o varios elementos de la misma carpeta; crea `nombre.zip` al lado, o `Comprimido.zip` si son varios; si el nombre ya existe añade la fecha y hora, así nunca se pisan) y **Extraer aquí** sobre un `.zip` (también `.tar.gz/.xz/.bz2/.zst`, `.7z`, `.rar`). Corre en segundo plano con aviso de progreso y botón Cancelar, y en trabajos que tardan (más de ~0,7 s) una **barra de progreso en el borde inferior** con el porcentaje real (bytes leídos por `bsdtar` / tamaño total); usa `bsdtar` (libarchive). Un zip con una sola carpeta se extrae tal cual junto al archivo; si no, en una carpeta nueva con el nombre del archivo. Nunca sobrescribe (añade « (2)»), no deja archivos a medias si se cancela o falla y rechaza rutas inseguras (`..`).
- **Previsualizaciones**: miniaturas FreeDesktop (imágenes + `.thumbnailer` por MIME, p. ej. Blender); mosaico 2×2 en carpetas (opcional).
- **Papelera**: el aviso superior muestra cuántos elementos hay y cuánto espacio ocupan (se calcula en segundo plano); «Vaciar papelera» lo repite en la confirmación.
- **Sonidos**: avisos del tema de sonido del sistema (`freedesktop`: comprimir `device-removed`, extraer `device-added`, error, papelera vaciada) con `pw-play`/`paplay`. Activados por defecto; se desactivan en ⋮ → **Sonidos**.
- **Iconos**: si el tema no tiene el icono exacto de un tipo, usa el genérico de archivo (y, sin tema, un símbolo propio); nunca queda un hueco en blanco.
- **Menú contextual**: abrir, propiedades, icono de carpeta, abrir con…, comprimir/extraer.
- **Chrome**: los botones nueva carpeta / subir / actualizar se pueden mostrar u ocultar desde ⋮.
- **Tema**: Material You en vivo desde Sylenara (`theme.json`).
- **Portal**: FileChooser de xdg-desktop-portal + un stub de `FileManager1`.

## Instalación

Viene con Sylenara: [su instalador](https://github.com/iMacnucintosh/sylenara-shell/blob/main/README.es.md#instalación) lo instala y lo deja como explorador de archivos. Por separado, en Arch Linux, añade el [repositorio de paquetes de Sylenara](https://github.com/iMacnucintosh/sylenara-packages) e instálalo con pacman:

```sh
sudo pacman -S seeker-git
```

## Compilar desde el código

Necesita Qt 6 (`qt6-base`, `qt6-declarative`, `qt6-svg`, `qt6-wayland`, `qt6-multimedia` + `qt6-multimedia-ffmpeg` para la vista rápida de vídeo y audio) y `poppler` (`pdftoppm`/`pdfinfo`, para la de PDF).

```sh
git clone https://github.com/iMacnucintosh/seeker
cd seeker
qmake6 PREFIX=$HOME/.local
make -j$(nproc)
make install
```

## Integración con Sylenara

- **Tema**: `scripts/sylenara-quiet.py`, de Sylenara, escribe `~/.config/seeker/theme.json` en cada apply, y Seeker lo recarga en caliente (colores, transparencia y velocidad de animación de la shell).
- **Portal de selección de archivos**: para que otras apps abran Seeker al elegir archivos:

  ```sh
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

### Miniaturas

Con *Previsualizar* activo, `image://thumb` resuelve en este orden (estilo Dolphin):

1. Decodificación nativa de Qt (`QImageReader`) para formatos de imagen.
2. Caché compartida de FreeDesktop `~/.cache/thumbnails/{normal,large,…}/`.
3. Thumbnailers XDG (`~/.local/share/thumbnailers/*.thumbnailer` y `/usr/share/…`) por MIME (extensión antes que content-sniff; los `.blend` zstd cuentan como `application/x-blender`).

Caché propia de Seeker: `~/.cache/seeker/thumbs/` (~256 MiB, poda LRU). Las miniaturas generadas también se publican en la caché compartida.

## Idiomas

El código está en inglés (`qsTr("…")`, `tr("…")`) y cada idioma es un catálogo JSON en `i18n/<idioma>.json` (texto en inglés → traducción). Hoy: español (`es`). Un idioma sin catálogo se ve en inglés; `SEEKER_LANG=es` fuerza uno. `scripts/check-i18n.py` comprueba que no falta ni sobra ninguna cadena.

## Licencia

GPL-3.0-only, ver [LICENSE](LICENSE). Copyright (C) 2026 Manuel López Mallorquín. Ver [NOTICE](NOTICE).
