#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$(mktemp -d "${TMPDIR:-/tmp}/canoe-input-test.XXXXXX")"
trap 'rm -rf "$BUILD"' EXIT
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
    -Ddlsym=mock_dlsym -c "$ROOT/src/canoe_analog_state.c" -o "$BUILD/input.o"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
    "$ROOT/tests/input_test.c" "$BUILD/input.o" -o "$BUILD/input-test"
"$BUILD/input-test"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
    -Dclock_gettime=fake_clock_gettime -Dnanosleep=fake_nanosleep \
    -c "$ROOT/src/frame_pacer.c" -o "$BUILD/pacer.o"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
    "$ROOT/tests/pacer_test.c" "$BUILD/pacer.o" -o "$BUILD/pacer-test"
"$BUILD/pacer-test"
"$ROOT/mods/canoe-crt-control/scripts/test.sh"
