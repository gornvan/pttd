#!/bin/bash
# Merge graphical-session env into a user-systemd service and start p2td with a tray-friendly Qt setup.
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

# Tray on Plasma/Wayland: use XCB (XWayland). Avoid KDE QPA theme SNI path until watcher exists.
export QT_QPA_PLATFORM=xcb
export QT_QPA_PLATFORMTHEME=
export QT_WAYLAND_DISABLE_GL=1

wait_for_kde_sni() {
  case "${XDG_CURRENT_DESKTOP:-}${DESKTOP_SESSION:-}" in
    *KDE*|*plasma*|*Plasma*) ;;
    *) return 0 ;;
  esac
  if ! command -v dbus-send >/dev/null 2>&1; then
    return 0
  fi
  local max=60 i
  for ((i = 0; i < max; i++)); do
    if dbus-send --session --print-reply --dest=org.freedesktop.DBus /org/freedesktop/DBus \
      org.freedesktop.DBus.NameHasOwner string:org.kde.StatusNotifierWatcher 2>/dev/null \
      | grep -q 'boolean true'; then
      return 0
    fi
    sleep 1
  done
  echo "p2td-session-launch: KDE StatusNotifierWatcher not ready after ${max}s; starting anyway" >&2
}

wait_for_kde_sni

exec /usr/bin/p2td
