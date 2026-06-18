#!/usr/bin/env bash
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
QEMU_DIR="${QEMU_DIR:-$REPO_DIR/qemu}"
VM_KEY="${VM_KEY:-$QEMU_DIR/vm_key}"
VM_USER="${VM_USER:-dev}"
VM_HOST="${VM_HOST:-localhost}"
VM_PORT="${VM_PORT:-2222}"
VM_PROJECT_DIR="${VM_PROJECT_DIR:-/home/dev/embedded_bewerbung}"
KNOWN_HOSTS="${KNOWN_HOSTS:-/tmp/qemu_hw_known_hosts}"
QEMU_LOG="${QEMU_LOG:-/tmp/qemu_hw_integration.log}"

SSH_BASE=(
    ssh
    -i "$VM_KEY"
    -p "$VM_PORT"
    -o StrictHostKeyChecking=no
    -o UserKnownHostsFile="$KNOWN_HOSTS"
)
SSH_TARGET="$VM_USER@$VM_HOST"
rm -f "$KNOWN_HOSTS"

if [ ! -f "$QEMU_DIR/launch-vm.sh" ]; then
    echo "ERROR: missing QEMU launcher: $QEMU_DIR/launch-vm.sh" >&2
    echo "Run qemu/create-vm-image.sh or set QEMU_DIR=/path/to/existing/qemu-dir." >&2
    exit 1
fi

if [ ! -f "$VM_KEY" ]; then
    echo "ERROR: missing VM SSH key: $VM_KEY" >&2
    echo "Run qemu/create-vm-image.sh or set VM_KEY=/path/to/key." >&2
    exit 1
fi

cleanup() {
    if "${SSH_BASE[@]}" -o ConnectTimeout=2 "$SSH_TARGET" "true" >/dev/null 2>&1; then
        "${SSH_BASE[@]}" "$SSH_TARGET" "sudo poweroff" >/dev/null 2>&1 || true
    fi
}
trap cleanup EXIT

echo "Starting QEMU VM..."
setsid bash "$QEMU_DIR/launch-vm.sh" >"$QEMU_LOG" 2>&1 &
QEMU_PID=$!
echo "QEMU launcher PID: $QEMU_PID"
echo "QEMU log: $QEMU_LOG"

echo "Waiting for SSH on $VM_HOST:$VM_PORT..."
for _ in $(seq 1 90); do
    if "${SSH_BASE[@]}" -o ConnectTimeout=2 "$SSH_TARGET" "echo ready" >/dev/null 2>&1; then
        break
    fi
    sleep 2
done

"${SSH_BASE[@]}" -o ConnectTimeout=5 "$SSH_TARGET" "echo ready" >/dev/null

echo "Waiting for cloud-init, if present..."
"${SSH_BASE[@]}" "$SSH_TARGET" "if command -v cloud-init >/dev/null 2>&1; then sudo cloud-init status --wait; fi"

echo "Syncing project to VM..."
rsync -az --delete \
    --exclude build \
    --exclude CMakeLists.txt.user \
    --exclude qemu/debian12-base.qcow2 \
    --exclude qemu/debian12.qcow2 \
    --exclude qemu/cloud-init.iso \
    --exclude qemu/vm_key \
    --exclude qemu/vm_key.pub \
    --exclude qemu/user-data \
    --exclude qemu/*.log \
    -e "ssh -i $VM_KEY -p $VM_PORT -o StrictHostKeyChecking=no -o UserKnownHostsFile=$KNOWN_HOSTS" \
    "$REPO_DIR/" "$SSH_TARGET:$VM_PROJECT_DIR/"

echo "Running hardware integration test in VM..."
"${SSH_BASE[@]}" "$SSH_TARGET" "set -eu
cd '$VM_PROJECT_DIR'

cd tools/gpio_sysfs_mock
make clean
make
cd ../..

sudo rmmod thermal_gpio_mock 2>/dev/null || true
sudo insmod tools/gpio_sysfs_mock/thermal_gpio_mock.ko
for n in 10 11 12; do
    if [ ! -d /sys/class/gpio/gpio\$n ]; then
        echo \$n | sudo tee /sys/class/gpio/export >/dev/null
    fi
done

sudo modprobe -r i2c-stub 2>/dev/null || true
sudo modprobe i2c-dev
sudo modprobe i2c-stub chip_addr=0x48

cmake -S . -B build -DBUILD_TESTS=ON
cmake --build build -j\$(nproc)
ctest --test-dir build --output-on-failure

sudo ./build/demo_real_hw
sudo ./build/c_impl/thermal_monitor_c_demo_real
"

echo "Hardware integration test passed."
cleanup
trap - EXIT

echo "Waiting for VM shutdown..."
for _ in $(seq 1 30); do
    if ! "${SSH_BASE[@]}" -o ConnectTimeout=2 "$SSH_TARGET" "true" >/dev/null 2>&1; then
        echo "VM is down."
        exit 0
    fi
    sleep 1
done

echo "WARNING: VM did not shut down within timeout." >&2
exit 1
