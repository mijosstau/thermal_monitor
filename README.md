# Thermal Monitor

Embedded-style thermal monitor implemented in C and C++.

Full project documentation is in [docs/README.md](docs/README.md).

Architecture diagram: [docs/architecture.svg](docs/architecture.svg)

Generated PDF documentation: [docs/README.pdf](docs/README.pdf)

QEMU VM setup: [qemu/README.md](qemu/README.md)

QEMU integration tests require an x86_64 QEMU binary. On Debian/Ubuntu,
install `qemu-system-x86`, which provides `qemu-system-x86_64`.

## Prerequisites

On Debian/Ubuntu:

```bash
sudo apt update
sudo apt install git ca-certificates build-essential cmake libgtest-dev qtbase5-dev qt5-qmake
```

For the QEMU hardware integration test:

```bash
sudo apt install qemu-system-x86 qemu-utils cloud-image-utils openssh-client rsync
```

## Quick Start

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The build includes the C implementation, the C++ implementation, Qt simulator, mocks, unit tests, and real-Linux/QEMU demo targets.
