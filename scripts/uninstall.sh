#!/bin/sh
set -eu

LIB=/var/lib/hakchi/libcanoe_analog_state.so
WRAPPER=/usr/bin/clover-canoe-shvc
BACKUP=$(ls -t /var/lib/hakchi/clover-canoe-shvc.before-analog-* 2>/dev/null | head -n 1 || true)

[ -n "$BACKUP" ] || {
    echo 'No installer backup found; refusing to change the wrapper.' >&2
    exit 1
}

cp "$BACKUP" "$WRAPPER"
chmod 755 "$WRAPPER"
rm -f "$LIB"
echo "Restored $BACKUP and removed $LIB"
