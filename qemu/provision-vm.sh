#!/usr/bin/env bash
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
QEMU_DIR="${QEMU_DIR:-$REPO_DIR/qemu}"
VM_KEY="${VM_KEY:-$QEMU_DIR/vm_key}"
VM_USER="${VM_USER:-dev}"
VM_HOST="${VM_HOST:-localhost}"
VM_PORT="${VM_PORT:-2222}"
VM_PROJECT_DIR="${VM_PROJECT_DIR:-/home/dev/embedded_bewerbung}"
KNOWN_HOSTS="${KNOWN_HOSTS:-/tmp/qemu_provision_known_hosts}"
QEMU_LOG="${QEMU_LOG:-/tmp/qemu_provision.log}"

SSH_BASE=(
    ssh
    -i "$VM_KEY"
    -p "$VM_PORT"
    -o StrictHostKeyChecking=no
    -o UserKnownHostsFile="$KNOWN_HOSTS"
)
SSH_TARGET="$VM_USER@$VM_HOST"
rm -f "$KNOWN_HOSTS"

wait_for_ssh() {
    echo "Waiting for SSH on $VM_HOST:$VM_PORT..."
    for _ in $(seq 1 120); do
        if "${SSH_BASE[@]}" -o ConnectTimeout=2 "$SSH_TARGET" "echo ready" >/dev/null 2>&1; then
            "${SSH_BASE[@]}" -o ConnectTimeout=5 "$SSH_TARGET" "echo ready" >/dev/null
            return 0
        fi
        sleep 2
    done
    echo "ERROR: SSH did not become ready." >&2
    return 1
}

wait_for_cloud_init() {
    echo "Waiting for cloud-init..."
    "${SSH_BASE[@]}" "$SSH_TARGET" "if command -v cloud-init >/dev/null 2>&1; then sudo cloud-init status --wait; fi"
}

if [ ! -f "$QEMU_DIR/launch-vm.sh" ]; then
    echo "ERROR: missing QEMU launcher: $QEMU_DIR/launch-vm.sh" >&2
    echo "Run qemu/create-vm-image.sh first." >&2
    exit 1
fi

if [ ! -f "$VM_KEY" ]; then
    echo "ERROR: missing VM SSH key: $VM_KEY" >&2
    echo "Run qemu/create-vm-image.sh first." >&2
    exit 1
fi

cleanup() {
    if "${SSH_BASE[@]}" -o ConnectTimeout=2 "$SSH_TARGET" "true" >/dev/null 2>&1; then
        "${SSH_BASE[@]}" "$SSH_TARGET" "sudo poweroff" >/dev/null 2>&1 || true
    fi
}
trap cleanup EXIT

echo "Starting QEMU VM for provisioning..."
setsid bash "$QEMU_DIR/launch-vm.sh" >"$QEMU_LOG" 2>&1 &
QEMU_PID=$!
echo "QEMU launcher PID: $QEMU_PID"
echo "QEMU log: $QEMU_LOG"

wait_for_ssh
wait_for_cloud_init

echo "Installing and verifying VM dependencies..."
"${SSH_BASE[@]}" "$SSH_TARGET" "set -eu
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    rsync \
    pkg-config \
    qt6-base-dev \
    libgtest-dev \
    linux-image-amd64 \
    linux-headers-amd64
sudo apt-get install -y linux-headers-\$(uname -r) || true

command -v cmake
command -v make
command -v gcc
command -v g++
command -v rsync
pkg-config --exists Qt6Core
test -d /lib/modules/\$(uname -r)/build
"

TARGET_KERNEL="$("${SSH_BASE[@]}" "$SSH_TARGET" "ls /boot/vmlinuz-*-amd64 2>/dev/null | sed 's|.*/vmlinuz-||' | grep -v cloud | sort -V | tail -1")"
CURRENT_KERNEL="$("${SSH_BASE[@]}" "$SSH_TARGET" "uname -r")"

if [ -n "$TARGET_KERNEL" ] && [ "$CURRENT_KERNEL" != "$TARGET_KERNEL" ]; then
    echo "Configuring GRUB to boot $TARGET_KERNEL instead of $CURRENT_KERNEL..."
    "${SSH_BASE[@]}" "$SSH_TARGET" "set -eu
MENU='Advanced options for Debian GNU/Linux>Debian GNU/Linux, with Linux $TARGET_KERNEL'
if grep -q '^GRUB_DEFAULT=' /etc/default/grub; then
    sudo sed -i \"s|^GRUB_DEFAULT=.*|GRUB_DEFAULT=\\\"\$MENU\\\"|\" /etc/default/grub
else
    echo \"GRUB_DEFAULT=\\\"\$MENU\\\"\" | sudo tee -a /etc/default/grub >/dev/null
fi
sudo update-grub
"
    echo "Rebooting into $TARGET_KERNEL..."
    "${SSH_BASE[@]}" "$SSH_TARGET" "sudo reboot" >/dev/null 2>&1 || true
    sleep 5
    wait_for_ssh
    wait_for_cloud_init
    "${SSH_BASE[@]}" "$SSH_TARGET" "set -eu
sudo apt-get update
sudo apt-get install -y linux-headers-\$(uname -r)
test -d /lib/modules/\$(uname -r)/build
"
fi

echo "Verifying I2C kernel modules..."
"${SSH_BASE[@]}" "$SSH_TARGET" "set -eu
sudo modprobe i2c-dev
sudo modprobe -r i2c-stub 2>/dev/null || true
sudo modprobe i2c-stub chip_addr=0x48
ls /dev/i2c-* >/dev/null
"

echo "Syncing repository to VM for kernel module verification..."
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

echo "Building and loading GPIO kernel module once..."
"${SSH_BASE[@]}" "$SSH_TARGET" "set -eu
cd '$VM_PROJECT_DIR/tools/gpio_sysfs_mock'
make clean
make
sudo rmmod thermal_gpio_mock 2>/dev/null || true
sudo insmod thermal_gpio_mock.ko
for n in 10 11 12; do
    if [ ! -d /sys/class/gpio/gpio\$n ]; then
        echo \$n | sudo tee /sys/class/gpio/export >/dev/null
    fi
done
test -e /sys/class/gpio/gpio10/value
test -e /sys/class/gpio/gpio11/value
test -e /sys/class/gpio/gpio12/value
sudo rmmod thermal_gpio_mock
"

echo "Provisioning passed. Shutting VM down..."
cleanup
trap - EXIT

echo "Waiting for VM shutdown..."
for _ in $(seq 1 60); do
    if ! "${SSH_BASE[@]}" -o ConnectTimeout=2 "$SSH_TARGET" "true" >/dev/null 2>&1; then
        echo "VM is provisioned and down."
        echo
        echo "Next step:"
        echo "  scripts/run_qemu_hw_integration.sh"
        exit 0
    fi
    sleep 1
done

echo "WARNING: VM did not shut down within timeout." >&2
exit 1
