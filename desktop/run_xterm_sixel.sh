#!/usr/bin/env sh
set -eu

DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

exec xterm -ti vt340 -tn xterm-256color -fn fixed \
  -e env TERM=xterm-256color COLORTERM=truecolor RETRO_WRITER_FORCE_SIXEL=1 \
  "$DIR/build/retro_writer_tv" "$DIR" "$@"
