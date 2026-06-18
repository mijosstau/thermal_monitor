#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

KEEP_BASE=0
if [ "${1:-}" = "--keep-base" ]; then
    KEEP_BASE=1
elif [ "${1:-}" = "-h" ] || [ "${1:-}" = "--help" ]; then
    cat <<EOF
Usage: qemu/clean.sh [--keep-base]

Remove generated QEMU VM artifacts.

Options:
  --keep-base  Keep debian12-base.qcow2 to avoid downloading it again.
EOF
    exit 0
elif [ "${1:-}" != "" ]; then
    echo "ERROR: unknown option: $1" >&2
    echo "Run: qemu/clean.sh --help" >&2
    exit 1
fi

rm -f \
    "$SCRIPT_DIR/debian12.qcow2" \
    "$SCRIPT_DIR/cloud-init.iso" \
    "$SCRIPT_DIR/user-data" \
    "$SCRIPT_DIR/vm_key" \
    "$SCRIPT_DIR/vm_key.pub" \
    "$SCRIPT_DIR/known_hosts" \
    "$SCRIPT_DIR"/*.log

if [ "$KEEP_BASE" -eq 0 ]; then
    rm -f "$SCRIPT_DIR/debian12-base.qcow2"
fi

echo "QEMU generated artifacts removed."
if [ "$KEEP_BASE" -eq 1 ]; then
    echo "Kept: $SCRIPT_DIR/debian12-base.qcow2"
fi
