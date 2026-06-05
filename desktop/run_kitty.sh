#!/usr/bin/env sh
set -eu

DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

exec kitty --working-directory "$DIR" "$DIR/build/retro_writer_tv" "$DIR" "$@"
