#!/usr/bin/env bash
# Install Seeker as the session FileChooser portal backend (user-local).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PREFIX="${PREFIX:-$HOME/.local}"
LIBEXEC="$PREFIX/libexec"
SHARE="$PREFIX/share"
CONFIG="${XDG_CONFIG_HOME:-$HOME/.config}/xdg-desktop-portal"

mkdir -p "$LIBEXEC" \
  "$SHARE/dbus-1/services" \
  "$SHARE/xdg-desktop-portal/portals" \
  "$CONFIG" \
  "${XDG_CONFIG_HOME:-$HOME/.config}/environment.d"

# Portal discovers ~/.local/share/xdg-desktop-portal/portals via XDG_DATA_DIRS.
ENV_D="${XDG_CONFIG_HOME:-$HOME/.config}/environment.d/xdg-data-dirs.conf"
if [[ ! -f "$ENV_D" ]]; then
  cat >"$ENV_D" <<'EOF'
XDG_DATA_DIRS=${HOME}/.local/share:/usr/local/share:/usr/share
EOF
  echo "Wrote $ENV_D (re-login applies to all services)"
fi
systemctl --user set-environment "XDG_DATA_DIRS=${HOME}/.local/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}" 2>/dev/null || true
systemctl --user import-environment XDG_DATA_DIRS 2>/dev/null || true

install -m755 "$ROOT/portal/xdg-desktop-portal-seeker" "$LIBEXEC/xdg-desktop-portal-seeker"
install -m644 "$ROOT/portal/seeker.portal" "$SHARE/xdg-desktop-portal/portals/seeker.portal"

# Expand libexec into the D-Bus service file.
sed "s|@LIBEXEC@|$LIBEXEC|g" \
  "$ROOT/portal/org.freedesktop.impl.portal.desktop.seeker.service.in" \
  > "$SHARE/dbus-1/services/org.freedesktop.impl.portal.desktop.seeker.service"

# Prefer Seeker for FileChooser; keep Hyprland for screencast / etc.
CONF="$CONFIG/portals.conf"
if [[ ! -f "$CONF" ]]; then
  cat >"$CONF" <<'EOF'
[preferred]
default=hyprland;seeker;gtk
org.freedesktop.impl.portal.FileChooser=seeker
EOF
  echo "Wrote $CONF"
else
  if ! grep -q 'org.freedesktop.impl.portal.FileChooser=seeker' "$CONF" 2>/dev/null; then
    if grep -q '^\[preferred\]' "$CONF"; then
      # Insert FileChooser preference after [preferred] if missing.
      tmp="$(mktemp)"
      awk '
        BEGIN{done=0}
        /^\[preferred\]/{print; print "org.freedesktop.impl.portal.FileChooser=seeker"; done=1; next}
        {print}
        END{if(!done){print ""; print "[preferred]"; print "org.freedesktop.impl.portal.FileChooser=seeker"}}
      ' "$CONF" >"$tmp"
      mv "$tmp" "$CONF"
    else
      printf '\n[preferred]\ndefault=hyprland;seeker;gtk\norg.freedesktop.impl.portal.FileChooser=seeker\n' >>"$CONF"
    fi
    echo "Updated $CONF (FileChooser=seeker)"
  else
    echo "FileChooser=seeker already in $CONF"
  fi
fi

# Restart portal stack so the new backend is picked up.
systemctl --user restart xdg-desktop-portal.service 2>/dev/null || true
systemctl --user stop xdg-desktop-portal-gtk.service 2>/dev/null || true
# Kill any stale seeker portal; next request will D-Bus-activate it.
pkill -f '/xdg-desktop-portal-seeker' 2>/dev/null || true

echo "Seeker FileChooser portal installed."
echo "  backend: $LIBEXEC/xdg-desktop-portal-seeker"
echo "  config:  $CONF"
echo "Test: open a folder from Cursor, or:"
echo "  gdbus call --session --dest org.freedesktop.portal.Desktop \\"
echo "    --object-path /org/freedesktop/portal/desktop \\"
echo "    --method org.freedesktop.portal.FileChooser.OpenFile \\' \\' 'Open Folder' \\'{'directory': <true>}\\'"
