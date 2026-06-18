# QEMU Test VM

This directory contains the reproducible QEMU VM setup for the hardware integration test.

Large generated VM artifacts are intentionally not committed:

- `debian12-base.qcow2`
- `debian12.qcow2`
- `cloud-init.iso`
- `vm_key`
- logs

## Build The VM Image

Install host tools:

```bash
sudo apt install qemu-system-x86 qemu-utils cloud-image-utils openssh-client
```

The VM is an x86_64 Debian VM and requires `qemu-system-x86_64`. On
Debian/Ubuntu this binary is provided by the `qemu-system-x86` package.

Then create the VM image:

```bash
qemu/create-vm-image.sh
```

The script downloads the Debian 12 generic cloud image, creates a local qcow2 overlay, generates an SSH key, writes cloud-init user data, and creates `cloud-init.iso`. It does not boot the VM.

The Debian VM installs the packages needed by this project through cloud-init:

- build tools
- CMake
- Qt 6 development package
- GoogleTest development package
- kernel headers
- rsync/git

## Clean Generated VM Artifacts

Remove all generated VM files and keys:

```bash
qemu/clean.sh
```

Keep the downloaded Debian base image to avoid downloading it again:

```bash
qemu/clean.sh --keep-base
```

## Provision The VM

Run the first boot and verify the test dependencies:

```bash
qemu/provision-vm.sh
```

This waits for cloud-init, installs and checks the required packages, verifies kernel headers, loads `i2c-dev` and `i2c-stub`, builds `tools/gpio_sysfs_mock/thermal_gpio_mock.ko`, loads it once, checks GPIO 10/11/12, and shuts the VM down.

The Debian generic cloud image initially boots a `cloud-amd64` kernel that may not provide `i2c-dev`. The provision script installs `linux-image-amd64`, configures GRUB to boot that standard Debian kernel, reboots, and then verifies I2C and kernel module support.

First boot can take a few minutes because cloud-init installs packages.

## Launch VM Manually

```bash
qemu/launch-vm.sh
```

This only launches the VM. It does not run the project tests.

Then connect:

```bash
ssh -i qemu/vm_key -p 2222 -o StrictHostKeyChecking=no dev@localhost
```

## Run Integration Test

From the repository root:

```bash
scripts/run_qemu_hw_integration.sh
```

This script launches the VM, syncs the repository, builds the project inside the VM, runs the unit tests and hardware demos, then shuts the VM down.

The integration script defaults to `qemu/` relative to the repository root. You can still override it:

```bash
QEMU_DIR=/path/to/qemu-dir scripts/run_qemu_hw_integration.sh
```
