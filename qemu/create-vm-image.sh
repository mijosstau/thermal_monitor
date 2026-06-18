#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

DEBIAN_IMAGE_URL="${DEBIAN_IMAGE_URL:-https://cloud.debian.org/images/cloud/bookworm/latest/debian-12-genericcloud-amd64.qcow2}"
BASE_IMAGE="${BASE_IMAGE:-$SCRIPT_DIR/debian12-base.qcow2}"
VM_IMAGE="${VM_IMAGE:-$SCRIPT_DIR/debian12.qcow2}"
VM_SIZE="${VM_SIZE:-8G}"
VM_KEY="${VM_KEY:-$SCRIPT_DIR/vm_key}"
USER_DATA_TEMPLATE="${USER_DATA_TEMPLATE:-$SCRIPT_DIR/user-data.template}"
USER_DATA="${USER_DATA:-$SCRIPT_DIR/user-data}"
META_DATA="${META_DATA:-$SCRIPT_DIR/meta-data}"
CLOUD_INIT_ISO="${CLOUD_INIT_ISO:-$SCRIPT_DIR/cloud-init.iso}"

need_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "ERROR: missing command: $1" >&2
        return 1
    fi
}

need_cmd qemu-img
need_cmd ssh-keygen

if command -v cloud-localds >/dev/null 2>&1; then
    ISO_CMD=(cloud-localds "$CLOUD_INIT_ISO" "$USER_DATA" "$META_DATA")
elif command -v genisoimage >/dev/null 2>&1; then
    ISO_CMD=(genisoimage -output "$CLOUD_INIT_ISO" -volid cidata -joliet -rock "$USER_DATA" "$META_DATA")
elif command -v xorriso >/dev/null 2>&1; then
    ISO_CMD=(xorriso -as mkisofs -output "$CLOUD_INIT_ISO" -volid cidata -joliet -rock "$USER_DATA" "$META_DATA")
else
    echo "ERROR: need cloud-localds, genisoimage, or xorriso to create cloud-init.iso" >&2
    echo "Debian/Ubuntu package options: cloud-image-utils, genisoimage, or xorriso" >&2
    exit 1
fi

if [ ! -f "$VM_KEY" ]; then
    ssh-keygen -t ed25519 -N "" -f "$VM_KEY" -C "embedded-demo"
fi

if [ ! -f "$BASE_IMAGE" ]; then
    if command -v curl >/dev/null 2>&1; then
        curl -L "$DEBIAN_IMAGE_URL" -o "$BASE_IMAGE"
    elif command -v wget >/dev/null 2>&1; then
        wget -O "$BASE_IMAGE" "$DEBIAN_IMAGE_URL"
    else
        echo "ERROR: need curl or wget to download Debian cloud image" >&2
        exit 1
    fi
fi

if [ ! -f "$VM_IMAGE" ]; then
    qemu-img create -f qcow2 -F qcow2 -b "$BASE_IMAGE" "$VM_IMAGE" "$VM_SIZE"
fi

PUBLIC_KEY="$(cat "$VM_KEY.pub")"
sed "s|__SSH_PUBLIC_KEY__|$PUBLIC_KEY|" "$USER_DATA_TEMPLATE" > "$USER_DATA"
"${ISO_CMD[@]}"

echo "Created VM image setup:"
echo "  image:       $VM_IMAGE"
echo "  cloud-init:  $CLOUD_INIT_ISO"
echo "  ssh key:     $VM_KEY"
echo
echo "Next step:"
echo "  qemu/provision-vm.sh"
echo
echo "After provisioning, run:"
echo "  scripts/run_qemu_hw_integration.sh"
