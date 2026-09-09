# ESP32 QEMU TLS regression harness

This harness compares the parent of the certificate-mapping fix with the
patched tree. It repeatedly takes the TLS connection-failure path and verifies
that the baseline loses heap while the patched firmware remains stable.

It uses Apple `container` only to build the old ESP-IDF firmware. The emulator
runs natively on Apple silicon.

## Prerequisites

- Ebiroll/calib built for `xtensa-softmmu` at `~/src/public/calib`. The harness
  was prepared against commit `73bee9bab5a78b0aaa584610b1564d533e7879ce`.
- A QEMU build with libgcrypt. The current binary reports
  `unknown type 'misc.esp32.rsa'`; fix that with `brew install libgcrypt`, then
  configure calib with `--enable-gcrypt` and build it again from a clean build
  directory:

  ```sh
  ../configure \
    --target-list=xtensa-softmmu \
    --enable-gtk \
    --enable-slirp \
    --enable-gcrypt \
    --disable-werror \
    --disable-fuse-lseek \
    --disable-fuse
  make -j10
  ```
- The bundled `esp32-v3-rom.bin` and `esp32-v3-rom-app.bin` in
  `~/src/public/calib/pc-bios`. The downloaded `esp32-r0-rom.bin` is not the ROM
  selected by this machine model.
- Apple `container` running (`container system start`). The build image is
  `espressif/idf:v4.4.8` under Rosetta because that tag is amd64-only.

## Run

First check the emulator, ROM files, and container service:

```sh
make qemu-prepare
```

Then build and run the baseline and patch:

```sh
make qemu-test
```

Artifacts and serial logs are written below `.qemu-work/`. The test passes
when the baseline loses at least 16 KiB (or exhausts memory after producing
samples), the patched run completes 6,000 attempts, loses no more than 4 KiB,
and improves on the baseline by at least 4x.

Paths and thresholds can be overridden directly:

```sh
python3 tools/qemu/qemu_test.py test \
  --qemu ~/src/public/calib/build/qemu-system-xtensa \
  --bios ~/src/public/calib/pc-bios \
  --iterations 6000
```

## Optional emulated-network HTTPS run

Once the QEMU Wi-Fi access point details are known, run the patched firmware
against a reachable TLS endpoint:

```sh
python3 tools/qemu/qemu_test.py https \
  --ssid SSID --password PASSWORD \
  --host example.com --port 443 --path /
```

This mode reports `SKIP` if Wi-Fi association times out. It is supplemental:
the deterministic connection-failure test is the regression gate.

## Scope

QEMU exercises the ESP32 firmware and the certificate partition mapping and
cleanup paths without a NINA host board. It does not emulate the complete NINA
SPI integration and is not a replacement for the hardware procedure in
`HW_TESTING.md`.
