#!/bin/sh
# Run on the Mini: sh upgrade-controls.sh STAGED_LIBRARY EXPECTED_SHA256
set -eu
STAGED=${1:?Pass the staged library path}
EXPECTED=${2:?Pass its expected SHA-256}
LIB=/var/lib/hakchi/libcanoe_analog_state.so
WRAPPER=/usr/bin/clover-canoe-shvc
CANOE_SHA=30d0768623e875dc8f3de334ccc7193807878c8ad9b50c38bec362deb4245a61

[ "$(sha256sum /usr/bin/canoe-shvc | cut -d ' ' -f 1)" = "$CANOE_SHA" ] || {
    echo 'Unsupported Canoe version; no files changed' >&2; exit 1;
}
[ "$(sha256sum "$STAGED" | cut -d ' ' -f 1)" = "$EXPECTED" ] || {
    echo 'Staged library checksum mismatch' >&2; exit 1;
}
[ -r "$LIB" ] && grep -q 'libcanoe_analog_state.so' "$WRAPPER" || {
    echo 'Install the original analog mod before using this upgrade' >&2; exit 1;
}
if pidof canoe-shvc >/dev/null; then
    echo 'Exit the game to the Mini menu before upgrading' >&2; exit 1;
fi
STAMP=$(date +%Y%m%d-%H%M%S)
BACKUP=/var/lib/hakchi/canoe-controls-backups/$STAMP
[ ! -e "$BACKUP" ] || { echo 'Backup already exists' >&2; exit 1; }
mkdir -p "$BACKUP"
cp -p "$LIB" "$BACKUP/libcanoe_analog_state.so"
cp -p "$WRAPPER" "$BACKUP/clover-canoe-shvc"
sha256sum "$LIB" "$WRAPPER" > "$BACKUP/before.sha256"
cp "$STAGED" "$LIB.new"
chmod 644 "$LIB.new"
[ "$(sha256sum "$LIB.new" | cut -d ' ' -f 1)" = "$EXPECTED" ]
# Reject missing runtime dependencies before replacing the existing library.
LD_PRELOAD="$LIB.new" /bin/true
if pidof canoe-shvc >/dev/null; then
    rm -f "$LIB.new"
    echo 'A game started during upgrade; existing library preserved' >&2; exit 1;
fi
mv -f "$LIB.new" "$LIB"
sync
printf 'CONTROLS_UPGRADED\nBACKUP=%s\n' "$BACKUP"
sha256sum /usr/bin/canoe-shvc "$LIB" "$WRAPPER"
