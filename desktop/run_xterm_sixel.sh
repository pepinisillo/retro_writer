#!/usr/bin/env sh
set -eu

# Launcher for machines where Kitty is not available.
# Opens Retro Writer inside xterm with Sixel enabled.
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

exec xterm -ti vt340 -fn fixed -e env RETRO_WRITER_FORCE_SIXEL=1 "$DIR/build/retro_writer_tv" "$@"
