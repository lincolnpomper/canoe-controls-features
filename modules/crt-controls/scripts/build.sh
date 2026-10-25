#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ZIG="${ZIG:-zig}"
OUT="$ROOT/dist"
mkdir -p "$OUT"
"$ZIG" cc -target arm-linux-gnueabihf.2.17 -mcpu=cortex_a7 -marm \
    -shared -fPIC -O2 -s -Wall -Wextra -Werror \
    -DCANOE_FILTER_DYNAMIC_CONTROL=1 \
    "$ROOT/src/canoe_crt_control.c" "$ROOT/src/arm_hook.c" -ldl \
    -o "$OUT/libcanoe_filter_shadow_test.so"
shasum -a 256 "$OUT/libcanoe_filter_shadow_test.so"
