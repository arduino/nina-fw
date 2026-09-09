# ESP32 QEMU TLS regression harness

This harness compares the firmware on the branch base with the current tree.
It repeatedly exercises the TLS connection-failure path and verifies that the
baseline loses heap while the patched firmware remains stable.

`make qemu-prepare` downloads and builds a pinned revision of
[Ebiroll/calib](https://github.com/Ebiroll/calib). Firmware is built in an
ESP-IDF container, while QEMU runs natively on the host.

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

One supported container engine must be installed and running:

- Apple `container` (`container system start`)
- Docker
- Podman

The firmware image is `espressif/idf:v4.4.8`, which runs as `linux/amd64`.
Apple `container` automatically receives `--rosetta` on Apple silicon.

## Prepare and run

Build and validate native ESP32 QEMU:

```sh
make qemu-prepare
```

The pinned source is fetched with depth one, and the source and build are
stored under `build/qemu/`. Re-running the target skips configuration when its
revision and options are unchanged, then performs an incremental build check.
The source contains the V3 ROM pair required by this QEMU machine; a separately
downloaded R0 ROM is not needed.

Build and run the baseline and patched firmware:

```sh
make qemu-test
```

Firmware source and object trees remain in `.qemu-work/` between runs.
Unchanged files retain their timestamps, allowing ESP-IDF Make to reuse cached
objects. Remove this cache explicitly with `make qemu-clean`.

The baseline defaults to the merge base of `HEAD` and `origin/master`, falling
back to local `master`. The patched ref defaults to `HEAD`. Artifacts and
serial logs are written below `.qemu-work/`.

The test passes when the baseline loses at least 16 KiB (or reaches sustained
resource exhaustion), the patched run completes 6,000 attempts with no more
than 4 KiB of heap loss, and the patched loss is at least four times smaller.

## Configuration

Make variables provide reproducible defaults and optional overrides:

```sh
make qemu-test \
  FIRMWARE_CONTAINER_ENGINE=docker \
  QEMU_BASELINE_REF=origin/master \
  FIRMWARE_JOBS=2
```

Useful variables include:

| Variable                    | Default                                        |
| --------------------------- | ---------------------------------------------- |
| `QEMU_ROOT`                 | `build/qemu`                                   |
| `CALIB_REPOSITORY`          | `https://github.com/Ebiroll/calib.git`         |
| `CALIB_REF`                 | pinned tested commit                           |
| `QEMU_JOBS`                 | host CPU count                                 |
| `FIRMWARE_JOBS`             | CPU count visible inside the container         |
| `FIRMWARE_CONTAINER_ENGINE` | auto-detect Apple container, Docker, or Podman |
| `FIRMWARE_IMAGE`            | `espressif/idf:v4.4.8`                         |
| `QEMU_BASELINE_REF`         | detected branch merge base                     |
| `QEMU_PATCHED_REF`          | `HEAD`                                         |

Reducing `FIRMWARE_JOBS` can help when the container has limited memory. The
same container builder is independently available as `make firmware-container`.

## Optional HTTPS run

Once the emulated Wi-Fi access-point details are known, run the patched image
against a reachable TLS endpoint:

```sh
make qemu-test-https \
  QEMU_WIFI_SSID=SSID \
  QEMU_WIFI_PASSWORD=PASSWORD \
  QEMU_TLS_HOST=example.com \
  QEMU_TLS_PORT=443 \
  QEMU_TLS_PATH=/
```

This mode reports `SKIP` if Wi-Fi association times out. It supplements the
deterministic connection-failure regression test.

## Limitations

QEMU exercises the ESP32 certificate mapping and cleanup paths without a NINA
host board. It does not emulate the complete NINA SPI integration and does not
replace a hardware soak test.
