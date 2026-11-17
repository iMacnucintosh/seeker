<h1 align="center">Seeker</h1>

<p align="center"><strong>English</strong> · <a href="./README.es.md">Español</a></p>

<p align="center">
The <strong>Material You</strong> file manager of <a href="https://github.com/iMacnucintosh/sylenara-shell">Sylenara</a>: quick look on Space, a folder that refreshes by itself, ZIP in one click and the colours of your desktop, live.
</p>

<p align="center"><img src="docs/screenshot.webp" width="760" alt="Seeker with Quick Look showing a photo"></p>

- Product: **Seeker** · launcher name: **Files**
- Stack: **Qt 6 Quick / QML + C++** (qmake)
- Binary: `~/.local/bin/seeker` · config: `~/.config/seeker/`

## Features

- **Places**: Home, Trash and pinned favourites (the XDG folders by default, reorderable; Home and Trash stay fixed).
- **Navigation**: back/forward; up and refresh optional (⋮ menu); history.
- **Address bar**: inline editing, refresh in the pill, suggestions going up the hierarchy; the mouse wheel over it goes back/forward.
- **Views**: grid / list / details (wheel + split menu). Icon size slider in ⋮ (5 steps: 48–80 px, 64 by default).
- **Selection**: click, Ctrl+click, rubber band; right click keeps a multiple selection.
- **Clipboard**: copy / cut / paste; freedesktop trash; undo.
- **Drag and drop**: move/copy between folders; drop on favourites to pin.
- **Quick Look** (Space, as in macOS): it comes out of the file's icon and shows only the file. **Images** with rounded corners; **video** and **audio** play by themselves with a minimal bar (play/pause, progress and time; the volume is the system's; audio gets a music icon); **PDF** page by page (buttons, wheel, `PgUp`/`PgDn`; pages are drawn by `pdftoppm` and cached); **text** with its first lines. The arrows move to the previous or next file without closing it. A folder, a file of another type or several selected files open **Properties**. Close it with Space, `Esc` or a click outside.
- **Live refresh**: the open folder updates by itself when another app adds, deletes or changes files (only that folder, without reloading it all: the selection and the scroll are kept).
- **ZIP**: context menu → **Compress to ZIP** (one or several items of the same folder; it creates `name.zip` next to them, or `Archive.zip` for several; if the name exists it adds the date and time, so nothing is ever overwritten) and **Extract here** on a `.zip` (also `.tar.gz/.xz/.bz2/.zst`, `.7z`, `.rar`). It runs in the background with a progress notice and a Cancel button, and long jobs (over ~0.7 s) show a **progress bar on the bottom edge** with the real percentage (bytes read by `bsdtar` / total size); it uses `bsdtar` (libarchive). A zip with a single folder extracts as it is next to the archive; otherwise into a new folder named after it. It never overwrites (it adds " (2)"), leaves nothing half done if cancelled or failed and rejects unsafe paths (`..`).
- **Previews**: FreeDesktop thumbnails (images + `.thumbnailer` by MIME type, e.g. Blender); 2×2 mosaic on folders (opt-in).
- **Trash**: the top notice shows how many items there are and how much space they take (computed in the background); "Empty trash" repeats it in the confirmation.
- **Sounds**: notices from the system sound theme (`freedesktop`: compress `device-removed`, extract `device-added`, error, trash emptied) with `pw-play`/`paplay`. On by default; turn them off in ⋮ → **Sounds**.
- **Icons**: if the theme lacks the exact icon of a type, it uses the generic file one (and, without a theme, its own symbol); there is never a blank gap.
- **Context menu**: open, properties, folder icon, open with…, compress/extract.
- **Chrome**: the new folder / up / refresh buttons can be shown or hidden from ⋮.
- **Theme**: Material You, live from Sylenara (`theme.json`).
- **Portal**: xdg-desktop-portal FileChooser + a `FileManager1` stub.

## Install

It comes with Sylenara: [its installer](https://github.com/iMacnucintosh/sylenara-shell#installation) sets it up and makes it the file manager. On its own, on Arch Linux, add the [Sylenara package repository](https://github.com/iMacnucintosh/sylenara-packages) and install it with pacman:

```sh
sudo pacman -S seeker-git
```

## Build from source

It needs Qt 6 (`qt6-base`, `qt6-declarative`, `qt6-svg`, `qt6-wayland`, `qt6-multimedia` + `qt6-multimedia-ffmpeg` for the video and audio quick look) and `poppler` (`pdftoppm`/`pdfinfo`, for the PDF one).

```sh
git clone https://github.com/iMacnucintosh/seeker
cd seeker
qmake6 PREFIX=$HOME/.local
make -j$(nproc)
make install
```

## Sylenara integration

- **Theme**: `scripts/sylenara-quiet.py`, from Sylenara, writes `~/.config/seeker/theme.json` with every apply, and Seeker reloads it live (colours, transparency and the animation speed of the shell).
- **File chooser portal**: so that other apps open Seeker to choose files:

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

  Test without the portal: `seeker --pick --pick-mode=folder --pick-result=/tmp/pick.json`

### Thumbnails

With *Preview* on, `image://thumb` resolves in this order (Dolphin style):

1. Native Qt decode (`QImageReader`) for image formats.
2. The shared FreeDesktop cache `~/.cache/thumbnails/{normal,large,…}/`.
3. XDG thumbnailers (`~/.local/share/thumbnailers/*.thumbnailer` and `/usr/share/…`) by MIME type (extension before content sniffing; zstd `.blend` files count as `application/x-blender`).

Seeker's own cache: `~/.cache/seeker/thumbs/` (~256 MiB, LRU pruning). The thumbnails it generates are also published to the shared cache.

## Languages

The code is in English (`qsTr("…")`, `tr("…")`) and each language is a JSON catalogue in `i18n/<language>.json` (English text → translation). Today: Spanish (`es`). A language without a catalogue shows in English; `SEEKER_LANG=es` forces one. `scripts/check-i18n.py` checks that no string is missing or left over.

## License

GPL-3.0-only, see [LICENSE](LICENSE). Copyright (C) 2026 Manuel López Mallorquín. See [NOTICE](NOTICE).
