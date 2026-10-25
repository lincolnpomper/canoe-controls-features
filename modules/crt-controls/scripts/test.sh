#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$(mktemp -d "${TMPDIR:-/tmp}/canoe-crt-test.XXXXXX")"
trap 'rm -rf "$BUILD"' EXIT
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Wno-deprecated-declarations \
    -fsanitize=address,undefined \
    -I"$ROOT/src" "$ROOT/tests/settings_test.c" -ldl -o "$BUILD/settings-test"
(cd "$BUILD" && ./settings-test)
