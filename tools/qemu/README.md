# ESP32 QEMU firmware tests

`make qemu-test` incrementally builds each firmware test under `tests/qemu/`
from the current working tree and runs the resulting images in
[Ebiroll/calib](https://github.com/Ebiroll/calib).

## Prerequisites

The host needs Git, GNU Make, Python 3, pkg-config, libgcrypt, and libslirp.
For example:

```sh
# macOS with Homebrew
brew install libgcrypt libslirp pkg-config

# Debian or Ubuntu
sudo apt install build-essential git libglib2.0-dev libgcrypt20-dev \
  libpixman-1-dev libslirp-dev ninja-build pkg-config python3
```

Firmware builds require a running Apple `container`, Docker, or Podman engine.
The default image is `espressif/idf:v4.4.8` on `linux/amd64`; Apple
`container` receives `--rosetta` automatically on Apple silicon.

## Prepare and run

Build and validate the pinned native ESP32 QEMU independently:

```sh
make qemu-prepare
```

Run every firmware test:

```sh
make qemu-test
```

Build the test firmware images without running QEMU:

```sh
make firmware-test-images-container
```

Tests run alphabetically. ESP-IDF reuses the normal component objects and
archives under `build/`, while each test gets a separate ELF, map, application
binary, and flash image under `build/qemu-tests/<name>/`. The normal
`build/nina-fw.elf` and `build/nina-fw.bin` are not replaced. QEMU serial logs
are written under `.qemu-work/tests/<name>/`.

Remove QEMU logs explicitly with:

```sh
make qemu-clean
```

Use `make firmware-clean` to remove normal and test firmware build outputs.

Useful overrides include:

```sh
make qemu-test \
  FIRMWARE_CONTAINER_ENGINE=docker \
  FIRMWARE_JOBS=2 \
  QEMU_RUN_TIMEOUT=900
```

## Adding a test

Create `tests/qemu/<name>/sketch.ino.cpp`. ESP-IDF compiles it as a separate
entry point and links it against the existing firmware component archives.
The test must define `setup()` and `loop()` and must not depend on symbols from
the production `main/sketch.ino.cpp`.

Every test must emit exactly one terminal serial record:

```text
QEMU_TEST,PASS,name=<name>,...
QEMU_TEST,FAIL,name=<name>,reason=...
QEMU_TEST,SKIP,name=<name>,reason=...
```

`FAIL`, a timeout, an emulator exit without a terminal record, or a mismatched
test name fails the suite. `SKIP` is reported without failing it.

## Limitations

QEMU exercises ESP32 firmware without a NINA host board. It does not emulate
the complete NINA SPI integration and does not replace hardware testing.
