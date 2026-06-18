#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

IMAGE="${IMAGE:-$SCRIPT_DIR/debian12.qcow2}"
CLOUD_INIT_ISO="${CLOUD_INIT_ISO:-$SCRIPT_DIR/cloud-init.iso}"
VM_PORT="${VM_PORT:-2222}"
VM_MEMORY="${VM_MEMORY:-2048}"
VM_CPUS="${VM_CPUS:-2}"
QEMU_BIN="${QEMU_BIN:-qemu-system-x86_64}"

if [ ! -f "$IMAGE" ]; then
    echo "ERROR: VM image not found: $IMAGE" >&2
    echo "Run: qemu/create-vm-image.sh" >&2
    exit 1
fi

if [ ! -f "$CLOUD_INIT_ISO" ]; then
    echo "ERROR: cloud-init ISO not found: $CLOUD_INIT_ISO" >&2
    echo "Run: qemu/create-vm-image.sh" >&2
    exit 1
fi

KVM_ARGS=()
if [ -e /dev/kvm ]; then
    KVM_ARGS=(-enable-kvm -cpu host)
else
    KVM_ARGS=(-cpu max)
fi

exec "$QEMU_BIN" \
  -name "embedded-demo" \
  -m "$VM_MEMORY" \
  -smp "$VM_CPUS" \
  "${KVM_ARGS[@]}" \
  -drive file="$IMAGE",format=qcow2,if=virtio \
  -drive file="$CLOUD_INIT_ISO",format=raw,if=virtio,readonly=on \
  -netdev user,id=net0,hostfwd=tcp::"$VM_PORT"-:22 \
  -device virtio-net-pci,netdev=net0 \
  -nographic \
  -serial mon:stdio
