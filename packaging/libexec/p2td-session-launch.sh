#!/bin/sh
# User systemd often starts without a full desktop environment block; merge the
# graphical session variables Plasma/GNOME registered with systemd --user.
set -eu

if command -v systemctl >/dev/null 2>&1; then
  while IFS= read -r line || [ -n "$line" ]; do
    case "$line" in
      DISPLAY=*|WAYLAND_DISPLAY=*|XAUTHORITY=*|DBUS_SESSION_BUS_ADDRESS=*|XDG_RUNTIME_DIR=*|XDG_CURRENT_DESKTOP=*|KDE_FULL_SESSION=*|SESSION_MANAGER=*|DESKTOP_SESSION=*|XDG_SESSION_TYPE=*)
        export "$line"
        ;;
    esac
  done <<EOF
$(systemctl --user show-environment 2>/dev/null || true)
EOF
fi

export QT_QPA_PLATFORM=xcb
exec /usr/bin/p2td
