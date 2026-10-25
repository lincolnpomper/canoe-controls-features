#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

"$ROOT/modules/controller-controls/scripts/test.sh"
"$ROOT/modules/crt-controls/scripts/test.sh"
