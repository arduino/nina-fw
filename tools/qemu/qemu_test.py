#!/usr/bin/env python3
"""Build and run the NINA TLS leak regression under Ebiroll's ESP32 QEMU."""

import argparse
import io
import os
import pathlib
import selectors
import shlex
import signal
import subprocess
import sys
import tarfile
import tempfile
import time

ROOT = pathlib.Path(__file__).resolve().parents[2]
WORK = ROOT / ".qemu-work"
QEMU_ROOT = pathlib.Path(os.environ.get("QEMU_ROOT", ROOT / "build/qemu"))
DEFAULT_QEMU = QEMU_ROOT / "build/qemu-system-xtensa"
DEFAULT_BIOS = QEMU_ROOT / "source/pc-bios"
BUILD_FIRMWARE = ROOT / "tools/qemu/build_firmware.sh"
FLASH_SIZE = 4 * 1024 * 1024


def run(command, **kwargs):
    print("+", shlex.join(str(item) for item in command), flush=True)
    return subprocess.run([str(item) for item in command], check=True, **kwargs)


def git(*args, **kwargs):
    return run(["git", "-C", ROOT, *args], **kwargs)


def require_file(path, description):
    if not path.is_file():
        raise RuntimeError(f"{description} not found: {path}")


def probe_qemu(qemu, bios):
    require_file(qemu, "QEMU executable")
    with tempfile.NamedTemporaryFile(suffix=".bin") as flash:
        flash.truncate(FLASH_SIZE)
        process = subprocess.Popen(
            [str(qemu), "-M", "esp32", "-nographic", "-S", "-L", str(bios),
             "-drive", f"file={flash.name},if=mtd,format=raw"],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        try:
            output, _ = process.communicate(timeout=2)
        except subprocess.TimeoutExpired:
            process.terminate()
            output, _ = process.communicate(timeout=5)
            return
    if "unknown type 'misc.esp32.rsa'" in output:
        raise RuntimeError(
            "QEMU's ESP32 machine is incomplete because calib was built without "
            "libgcrypt. Install it with `brew install libgcrypt`, then re-run "
            "configure with `--enable-gcrypt` and make in a clean calib build "
            "directory."
        )
    if process.returncode:
        raise RuntimeError(f"QEMU ESP32 preflight failed:\n{output.strip()}")


def preflight(qemu, bios, need_container=True):
    require_file(bios / "esp32-v3-rom.bin", "ESP32 V3 ROM")
    require_file(bios / "esp32-v3-rom-app.bin", "ESP32 V3 app ROM")
    probe_qemu(qemu, bios)
    if need_container:
        run(["sh", BUILD_FIRMWARE, "--check"])


def extract(ref, destination):
    destination.mkdir(parents=True, exist_ok=True)
    archive = git("archive", "--format=tar", ref, stdout=subprocess.PIPE).stdout
    with tarfile.open(fileobj=io.BytesIO(archive), mode="r:") as bundle:
        destination_root = destination.resolve()
        for member in bundle.getmembers():
            target = (destination / member.name).resolve()
            if destination_root not in (target, *target.parents):
                raise RuntimeError(f"unsafe path in Git archive: {member.name}")
        bundle.extractall(destination)
    (destination / "main/sketch.ino.cpp").write_bytes(
        (ROOT / "tests/qemu/sketch.ino.cpp").read_bytes()
    )


def c_string(value):
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def write_config(source, args, https=False):
    lines = [
        "#pragma once",
        f"#define QEMU_TLS_ITERATIONS {args.iterations}",
        f"#define QEMU_TLS_WARMUP {args.warmup}",
        f"#define QEMU_TLS_TIMEOUT_MS {args.connect_timeout_ms}",
        f"#define QEMU_TLS_HOST {c_string(args.host)}",
        f"#define QEMU_TLS_PORT {args.port}",
    ]
    if https:
        lines.extend([
            "#define QEMU_TLS_HTTPS 1",
            f"#define QEMU_WIFI_SSID {c_string(args.ssid)}",
            f"#define QEMU_WIFI_PASSWORD {c_string(args.password)}",
            f"#define QEMU_TLS_PATH {c_string(args.path)}",
        ])
    (source / "main/QemuTestConfig.h").write_text("\n".join(lines) + "\n")


def build(ref, name, args, https=False):
    source = WORK / name / "source"
    if source.exists():
        raise RuntimeError(f"stale work directory exists: {source}; run prepare first")
    extract(ref, source)
    write_config(source, args, https)
    run(["sh", BUILD_FIRMWARE, source])
    firmware = source / "NINA_W102_ALL.bin"
    require_file(firmware, "combined firmware")
    flash = WORK / name / "flash.bin"
    size = firmware.stat().st_size
    if size > FLASH_SIZE:
        raise RuntimeError(f"firmware is {size} bytes and does not fit a 4 MiB flash")
    with firmware.open("rb") as incoming, flash.open("wb") as outgoing:
        outgoing.write(incoming.read())
        outgoing.write(b"\xff" * (FLASH_SIZE - size))
    return flash


def run_qemu(name, flash, args):
    log = WORK / name / "qemu.log"
    command = [
        str(args.qemu), "-M", "esp32",
        "-drive", f"file={flash},if=mtd,format=raw",
        "-nographic", "-monitor", "none", "-no-reboot",
        "-nic", "user,model=esp32_wifi", "-L", str(args.bios),
        "-global", "driver=timer.esp32.timg,property=wdt_disable,value=true",
    ]
    print("+", shlex.join(command), flush=True)
    process = subprocess.Popen(
        command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        start_new_session=True,
    )
    deadline = time.monotonic() + args.run_timeout
    terminal = False
    with log.open("w") as output:
        selector = selectors.DefaultSelector()
        selector.register(process.stdout, selectors.EVENT_READ)
        transcript = ""
        while time.monotonic() < deadline:
            ready = selector.select(timeout=min(1, max(0, deadline - time.monotonic())))
            if ready:
                chunk = os.read(process.stdout.fileno(), 4096).decode(errors="replace")
            else:
                chunk = ""
            if chunk:
                print(chunk, end="", flush=True)
                output.write(chunk)
                output.flush()
                transcript = (transcript + chunk)[-8192:]
                if any(marker in transcript for marker in (
                    "QEMU_TLS,DONE", "QEMU_TLS,INFRA_ERROR", "QEMU_TLS,SKIP"
                )):
                    terminal = True
                    break
            if process.poll() is not None:
                break
        selector.close()
        if not terminal and process.poll() is None:
            output.write("QEMU_RUNNER,TIMEOUT\n")
    if process.poll() is None:
        os.killpg(process.pid, signal.SIGTERM)
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            process.wait()
    return log


def clean_work():
    # Keep deletion tightly scoped to the ignored harness directory.
    if WORK.exists():
        import shutil
        shutil.rmtree(WORK)
    WORK.mkdir()


def command_prepare(args):
    clean_work()
    preflight(args.qemu, args.bios)
    print(f"Ready: QEMU={args.qemu}; BIOS={args.bios}")


def command_test(args):
    clean_work()
    preflight(args.qemu, args.bios)
    baseline_ref = args.baseline_ref or find_baseline()
    print(f"Comparing baseline {baseline_ref} with {args.patched_ref}")
    baseline_flash = build(baseline_ref, "baseline", args)
    patched_flash = build(args.patched_ref, "patched", args)
    baseline_log = run_qemu("baseline", baseline_flash, args)
    patched_log = run_qemu("patched", patched_flash, args)
    result = subprocess.run([
        sys.executable, ROOT / "tools/qemu/analyze.py", baseline_log, patched_log,
        "--iterations", str(args.iterations),
    ])
    return result.returncode


def command_https(args):
    clean_work()
    preflight(args.qemu, args.bios)
    flash = build(args.patched_ref, "https", args, https=True)
    log = run_qemu("https", flash, args)
    print(f"HTTPS run log: {log}")


def find_baseline():
    for candidate in ("origin/master", "master"):
        result = subprocess.run(
            ["git", "-C", ROOT, "merge-base", "HEAD", candidate],
            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True,
        )
        if result.returncode == 0 and result.stdout.strip():
            return result.stdout.strip()
    raise RuntimeError(
        "cannot determine a baseline; pass --baseline-ref or set QEMU_BASELINE_REF"
    )


def parser():
    common = argparse.ArgumentParser(add_help=False)
    common.add_argument("--qemu", type=pathlib.Path, default=DEFAULT_QEMU)
    common.add_argument("--bios", type=pathlib.Path, default=DEFAULT_BIOS)
    common.add_argument("--patched-ref", default="HEAD")
    common.add_argument("--iterations", type=int, default=6000)
    common.add_argument("--warmup", type=int, default=32)
    common.add_argument("--host", default="127.0.0.1")
    common.add_argument("--port", type=int, default=9)
    common.add_argument("--connect-timeout-ms", type=int, default=25)
    common.add_argument("--run-timeout", type=int, default=600)

    result = argparse.ArgumentParser(description=__doc__)
    commands = result.add_subparsers(dest="command", required=True)
    commands.add_parser("prepare", parents=[common]).set_defaults(function=command_prepare)
    test = commands.add_parser("test", parents=[common])
    test.add_argument("--baseline-ref")
    test.set_defaults(function=command_test)
    https = commands.add_parser("https", parents=[common])
    https.add_argument("--ssid", required=True)
    https.add_argument("--password", default="")
    https.add_argument("--path", default="/")
    https.set_defaults(function=command_https)
    return result


def main():
    args = parser().parse_args()
    try:
        return args.function(args) or 0
    except (RuntimeError, subprocess.CalledProcessError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
