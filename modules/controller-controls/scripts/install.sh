#!/bin/sh
set -eu

LIB=/var/lib/hakchi/libcanoe_analog_state.so
WRAPPER=/usr/bin/clover-canoe-shvc
LAUNCH_PATTERN='^[[:space:]]*exec[[:space:]]+([^[:space:]]*/)?canoe-shvc([[:space:]]|$)'
STAMP=$(date +%Y%m%d-%H%M%S)
BACKUP=/var/lib/hakchi/clover-canoe-shvc.before-analog-$STAMP
DRY_RUN=0
ALREADY_CONFIGURED=0

case "${1:-}" in
    '') ;;
    --dry-run) DRY_RUN=1 ;;
    *)
        echo "Usage: $0 [--dry-run]" >&2
        exit 2
        ;;
esac

[ -f "$LIB" ] || {
    echo "Missing $LIB. Copy libcanoe_analog_state.so there first." >&2
    exit 1
}
[ -f "$WRAPPER" ] || {
    echo "Missing $WRAPPER." >&2
    exit 1
}

if grep -q 'libcanoe_analog_state.so' "$WRAPPER"; then
    ALREADY_CONFIGURED=1
    if [ "$DRY_RUN" -eq 0 ]; then
        echo 'Analog library is already configured.'
        exit 0
    fi
fi

# Add the export immediately before Canoe's final exec. The matching exec must
# exist exactly once; fail rather than modifying an unexpected wrapper.
COUNT=$(grep -Ec "$LAUNCH_PATTERN" "$WRAPPER") || GREP_STATUS=$?
case "${GREP_STATUS:-0}" in
    0|1) ;;
    *)
        echo "Could not inspect $WRAPPER (grep exit status $GREP_STATUS)." >&2
        exit 1
        ;;
esac
[ "$COUNT" -eq 1 ] || {
    echo "Expected one Canoe launch line; found $COUNT. No changes made." >&2
    echo "Expected a line such as: exec canoe-shvc ... or exec /usr/bin/canoe-shvc ..." >&2
    exit 1
}

if [ "$DRY_RUN" -eq 1 ]; then
    echo "Dry run passed. Found Canoe launch line:"
    grep -nE "$LAUNCH_PATTERN" "$WRAPPER"
    if [ "$ALREADY_CONFIGURED" -eq 1 ]; then
        echo "The analog library is already configured; no changes would be made."
    else
        echo "Would back up $WRAPPER to $BACKUP and add LD_PRELOAD immediately before that line."
    fi
    exit 0
fi

cp "$WRAPPER" "$BACKUP"

awk '
/^[[:space:]]*exec[[:space:]]+([^[:space:]]*\/)?canoe-shvc([[:space:]]|$)/ {
    print "if [ -r /var/lib/hakchi/libcanoe_analog_state.so ]; then"
    print "    export LD_PRELOAD=/var/lib/hakchi/libcanoe_analog_state.so"
    print "fi"
}
{ print }
' "$BACKUP" > "$WRAPPER"
chmod 755 "$WRAPPER"

echo "Installed. Wrapper backup: $BACKUP"
