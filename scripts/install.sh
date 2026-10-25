#!/bin/sh
set -eu

LIB=/var/lib/hakchi/libcanoe_analog_state.so
WRAPPER=/usr/bin/clover-canoe-shvc
STAMP=$(date +%Y%m%d-%H%M%S)
BACKUP=/var/lib/hakchi/clover-canoe-shvc.before-analog-$STAMP

[ -f "$LIB" ] || {
    echo "Missing $LIB. Copy libcanoe_analog_state.so there first." >&2
    exit 1
}
[ -f "$WRAPPER" ] || {
    echo "Missing $WRAPPER." >&2
    exit 1
}

if grep -q 'libcanoe_analog_state.so' "$WRAPPER"; then
    echo 'Analog library is already configured.'
    exit 0
fi

cp "$WRAPPER" "$BACKUP"

# Add the export immediately before Canoe's final exec. The matching exec must
# exist exactly once; fail rather than modifying an unexpected wrapper.
COUNT=$(grep -c '^exec canoe-shvc ' "$WRAPPER")
[ "$COUNT" -eq 1 ] || {
    echo "Expected one Canoe exec line; found $COUNT. Restoring backup." >&2
    cp "$BACKUP" "$WRAPPER"
    exit 1
}

sed '/^exec canoe-shvc /i\
if [ -r /var/lib/hakchi/libcanoe_analog_state.so ]; then\
    export LD_PRELOAD=/var/lib/hakchi/libcanoe_analog_state.so\
fi' "$BACKUP" > "$WRAPPER"
chmod 755 "$WRAPPER"

echo "Installed. Wrapper backup: $BACKUP"
