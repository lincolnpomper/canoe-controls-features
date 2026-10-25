#!/bin/sh
# Restore just the previous control library, preserving the installed launcher.
set -eu
BACKUP=${1:?Pass the backup directory printed by upgrade-controls.sh}
LIB=/var/lib/hakchi/libcanoe_analog_state.so
[ -r "$BACKUP/libcanoe_analog_state.so" ] && [ -r "$BACKUP/before.sha256" ] || {
    echo 'Missing control-library backup' >&2; exit 1;
}
EXPECTED=$(awk '$2=="/var/lib/hakchi/libcanoe_analog_state.so" {print $1}' "$BACKUP/before.sha256")
[ -n "$EXPECTED" ] && [ "$(sha256sum "$BACKUP/libcanoe_analog_state.so" | cut -d ' ' -f 1)" = "$EXPECTED" ] || {
    echo 'Backup checksum mismatch' >&2; exit 1;
}
if pidof canoe-shvc >/dev/null; then
    echo 'Exit the game to the Mini menu before restoring' >&2; exit 1;
fi
cp "$BACKUP/libcanoe_analog_state.so" "$LIB.new"
chmod 644 "$LIB.new"
[ "$(sha256sum "$LIB.new" | cut -d ' ' -f 1)" = "$EXPECTED" ]
LD_PRELOAD="$LIB.new" /bin/true
if pidof canoe-shvc >/dev/null; then
    rm -f "$LIB.new"
    echo 'A game started; existing library preserved' >&2; exit 1;
fi
mv -f "$LIB.new" "$LIB"
sync
printf 'PREVIOUS_CONTROLS_RESTORED\n'
sha256sum "$LIB"
