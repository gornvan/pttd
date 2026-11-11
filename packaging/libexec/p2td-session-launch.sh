#!/bin/bash
# Merge graphical-session env into a user-systemd service (DISPLAY, D-Bus, …) and force Qt XCB for tray.
set -euo pipefail

if command -v systemctl >/dev/null 2>&1; then
  while IFS= read -r line; do
    [[ "$line" == *=* ]] || continue
    export "$line"
  done < <(systemctl --user show-environment 2>/dev/null || true)
fi

uid="$(id -u)"
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/${uid}}"
export DBUS_SESSION_BUS_ADDRESS="${DBUS_SESSION_BUS_ADDRESS:-unix:path=${XDG_RUNTIME_DIR}/bus}"

if [[ -z "${DISPLAY:-}" ]]; then
  for d in :1 :0; do
    sock="/tmp/.X11-unix/X${d#:}"
    if [[ -S "$sock" ]]; then
      export DISPLAY="$d"
      break
    fi
  done
fi

if [[ -z "${XAUTHORITY:-}" && -f "${HOME}/.Xauthority" ]]; then
  export XAUTHORITY="${HOME}/.Xauthority"
fi

export QT_QPA_PLATFORM=xcb

exec /usr/bin/p2td
