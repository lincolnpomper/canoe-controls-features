#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ZIG="${ZIG:-zig}"

ZIG="$ZIG" "$ROOT/modules/controller-controls/scripts/build.sh"
ZIG="$ZIG" "$ROOT/modules/crt-controls/scripts/build.sh"
