#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ZIG="${ZIG:-zig}"
mkdir -p "$ROOT/dist"
"$ZIG" cc -target arm-linux-gnueabihf.2.17 -mcpu=cortex_a7 -marm \
    -shared -fPIC -O2 -s -Wall -Wextra -Werror \
    "$ROOT/src/canoe_analog_state.c" "$ROOT/src/canoe_pause.c" \
    "$ROOT/src/canoe_fast_forward.c" "$ROOT/src/frame_pacer.c" \
    "$ROOT/src/canoe_scanline.c" "$ROOT/src/canoe_scanline_transform.c" \
    -ldl -o "$ROOT/dist/libcanoe_analog_state.so"
shasum -a 256 "$ROOT/dist/libcanoe_analog_state.so"
