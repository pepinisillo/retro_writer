#!/usr/bin/env sh
set -eu

# Use this launcher in xterm-like terminals with Sixel support.
# It enables Retro Writer's pixel-image renderer instead of the U+2580 block fallback.
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

export RETRO_WRITER_ALLOW_SIXEL=1

exec "$DIR/build/retro_writer_tv" "$@"
