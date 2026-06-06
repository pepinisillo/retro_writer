#!/usr/bin/env sh
set -eu

DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN="$DIR/build/retro_writer_tv.codex"
[ -x "$BIN" ] || BIN="$DIR/build/retro_writer_tv"

exec xterm -ti vt340 -tn xterm-256color \
  -e sh -c 'stty -ixon 2>/dev/null || true; exec "$@"' sh \
  env TERM=xterm-256color COLORTERM=truecolor RETRO_WRITER_FORCE_SIXEL=1 \
  "$BIN" "$DIR" "$@"
