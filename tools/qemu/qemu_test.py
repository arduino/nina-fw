#!/usr/bin/env python3
"""Build and run the NINA TLS leak regression under Ebiroll's ESP32 QEMU."""

import argparse
import filecmp
import io
import os
import pathlib
import selectors
import shlex
import shutil
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
BUILD_FIRMWARE = ROOT / "tools/firmware/build_container.sh"
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
    archive = git("archive", "--format=tar", ref, stdout=subprocess.PIPE).stdout
    with tarfile.open(fileobj=io.BytesIO(archive), mode="r:") as bundle:
        destination_root = destination.resolve()
        for member in bundle.getmembers():
            target = (destination / member.name).resolve()
            if destination_root not in (target, *target.parents):
                raise RuntimeError(f"unsafe path in Git archive: {member.name}")
        bundle.extractall(destination)


def same_file(left, right):
    return (
        left.is_file()
        and not left.is_symlink()
        and filecmp.cmp(left, right, shallow=False)
    )


def remove_path(path):
    if path.is_dir() and not path.is_symlink():
        shutil.rmtree(path)
    elif path.exists() or path.is_symlink():
        path.unlink()


def synchronize(source, staged):
    preserved = {"build", "x509_crt_bundle", ".x509_crt_bundle.mk"}
    desired = {
        path.relative_to(staged)
        for path in staged.rglob("*")
    }

    if source.exists():
        managed_paths = []
        for child in source.iterdir():
            if child.name in preserved or child.name.startswith("NINA_W102"):
                continue
            managed_paths.append(child.relative_to(source))
            if child.is_dir() and not child.is_symlink():
                managed_paths.extend(path.relative_to(source) for path in child.rglob("*"))
        existing = sorted(managed_paths, key=lambda path: len(path.parts), reverse=True)
        for relative in existing:
            target = source / relative
            if relative not in desired:
                if target.is_dir() and not target.is_symlink():
                    target.rmdir()
                else:
                    target.unlink()
    else:
        source.mkdir(parents=True)

    for relative in sorted(desired, key=lambda path: len(path.parts)):
        incoming = staged / relative
        target = source / relative
        if incoming.is_dir() and not incoming.is_symlink():
            if target.exists() and (not target.is_dir() or target.is_symlink()):
                remove_path(target)
            target.mkdir(exist_ok=True)
        elif incoming.is_symlink():
            link_target = os.readlink(incoming)
            if not target.is_symlink() or os.readlink(target) != link_target:
                remove_path(target)
                target.symlink_to(link_target)
        elif not same_file(target, incoming):
            if target.is_dir() or target.is_symlink():
                remove_path(target)
            target.parent.mkdir(parents=True, exist_ok=True)
            # Changed inputs need a fresh timestamp so Make sees them as newer
            # than preserved objects and generated files.
            shutil.copyfile(incoming, target)
            shutil.copymode(incoming, target)


def c_string(value):
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def config_text(args, https=False):
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
    return "\n".join(lines) + "\n"


def build(ref, name, args, https=False):
    source = WORK / name / "source"
    variant = WORK / name
    variant.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="source-", dir=variant) as temporary:
        staged = pathlib.Path(temporary)
        extract(ref, staged)
        (staged / "main/sketch.ino.cpp").write_bytes(
            (ROOT / "tests/qemu/sketch.ino.cpp").read_bytes()
        )
        (staged / "main/QemuTestConfig.h").write_text(config_text(args, https))
        synchronize(source, staged)
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


def ensure_work():
    WORK.mkdir(exist_ok=True)


def command_prepare(args):
    preflight(args.qemu, args.bios, need_container=False)
    print(f"Ready: QEMU={args.qemu}; BIOS={args.bios}")


def command_test(args):
    ensure_work()
    preflight(args.qemu, args.bios)
    baseline_ref = resolve_ref(args.baseline_ref or find_baseline())
    patched_ref = resolve_ref(args.patched_ref)
    print(f"Comparing baseline {baseline_ref} with {patched_ref}")
    baseline_flash = build(baseline_ref, "baseline", args)
    patched_flash = build(patched_ref, "patched", args)
    baseline_log = run_qemu("baseline", baseline_flash, args)
    patched_log = run_qemu("patched", patched_flash, args)
    result = subprocess.run([
        sys.executable, ROOT / "tools/qemu/analyze.py", baseline_log, patched_log,
        "--iterations", str(args.iterations),
    ])
    return result.returncode


def command_https(args):
    ensure_work()
    preflight(args.qemu, args.bios)
    flash = build(resolve_ref(args.patched_ref), "https", args, https=True)
    log = run_qemu("https", flash, args)
    print(f"HTTPS run log: {log}")


def command_clean(_args):
    if WORK.exists():
        shutil.rmtree(WORK)
        print(f"Removed {WORK}")


def resolve_ref(ref):
    result = subprocess.run(
        ["git", "-C", ROOT, "rev-parse", "--verify", f"{ref}^{{commit}}"],
        check=True, stdout=subprocess.PIPE, text=True,
    )
    return result.stdout.strip()


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
    commands.add_parser("clean").set_defaults(function=command_clean)
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
