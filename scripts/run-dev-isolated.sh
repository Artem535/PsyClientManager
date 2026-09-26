#!/usr/bin/env bash
# Run a Sessio dev/test build with an isolated HOME and XDG dirs,
# so it never reads or writes the real ~/.config/Sessio install
# (settings, app-lock state, client database, backups).
#
# Usage: scripts/run-dev-isolated.sh <path-to-Sessio-binary> [app args...]
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "usage: $0 <path-to-Sessio-binary> [app args...]" >&2
  exit 1
fi

binary="$1"
shift

iso_home="$(mktemp -d "${TMPDIR:-/tmp}/pcm-dev-home-XXXXXX")"
export HOME="$iso_home"
export XDG_CONFIG_HOME="$iso_home/.config"
export XDG_DATA_HOME="$iso_home/.local/share"
export XDG_CACHE_HOME="$iso_home/.cache"
export XDG_STATE_HOME="$iso_home/.local/state"
mkdir -p "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_CACHE_HOME" "$XDG_STATE_HOME"

echo "Isolated HOME: $iso_home" >&2
echo "Real ~/.config/Sessio is NOT touched by this run." >&2

exec "$binary" "$@"
