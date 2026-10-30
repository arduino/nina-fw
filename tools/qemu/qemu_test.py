#!/usr/bin/env python3
"""Run every prebuilt firmware test image using ESP32 QEMU."""

import argparse
import os
import pathlib
import re
import shlex
import shutil
import signal
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass

ROOT = pathlib.Path(__file__).resolve().parents[2]
IMAGE_ROOT = ROOT / "build/qemu-tests"
WORK = ROOT / ".qemu-work/tests"
QEMU_ROOT = pathlib.Path(os.environ.get("QEMU_ROOT", ROOT / "build/qemu"))
DEFAULT_QEMU = QEMU_ROOT / "build/qemu-system-xtensa"
DEFAULT_BIOS = QEMU_ROOT / "source/pc-bios"
FLASH_SIZE = 4 * 1024 * 1024
TRANSCRIPT_LIMIT = 64 * 1024
TERMINAL = re.compile(
    r"^QEMU_TEST,(PASS|FAIL|SKIP),name=([^,\r\n]+)(?:,.*)?$"
)


@dataclass
class TestResult:
    name: str
    status: str
    log: pathlib.Path | None = None
    detail: str = ""


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
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
        )
        try:
            output, _ = process.communicate(timeout=2)
        except subprocess.TimeoutExpired:
            process.terminate()
            process.communicate(timeout=5)
            return
    if "unknown type 'misc.esp32.rsa'" in output:
        raise RuntimeError("QEMU's ESP32 machine was built without libgcrypt")
    if process.returncode:
        raise RuntimeError(f"QEMU ESP32 preflight failed:\n{output.strip()}")


def preflight(qemu, bios):
    require_file(bios / "esp32-v3-rom.bin", "ESP32 V3 ROM")
    require_file(bios / "esp32-v3-rom-app.bin", "ESP32 V3 app ROM")
    probe_qemu(qemu, bios)


def discover_images(image_root=IMAGE_ROOT):
    images = [
        image for image in image_root.glob("*/*.flash.bin")
        if image.is_file()
    ] if image_root.is_dir() else []
    if not images:
        raise RuntimeError(f"no QEMU test images found under {image_root}")
    return sorted(images, key=lambda image: image.parent.name)


def classify_output(name, text, log=None, timed_out=False, returncode=0):
    terminal = None
    for line in text.splitlines(keepends=True):
        if not line.endswith(("\n", "\r")):
            continue
        record = line.rstrip("\r\n")
        match = TERMINAL.fullmatch(record)
        if match:
            terminal = (match.group(1), match.group(2), record)
    if terminal:
        status, record_name, detail = terminal
        if record_name != name:
            return TestResult(name, "ERROR", log, "terminal record has the wrong test name")
        return TestResult(name, status, log, detail)
    if timed_out:
        return TestResult(name, "ERROR", log, "QEMU timed out")
    return TestResult(name, "ERROR", log, f"QEMU exited with {returncode} without a terminal result")


def print_test_output(text):
    try:
        print(text, end="", flush=True)
    except BlockingIOError:
        # Some terminals use non-blocking output. The complete stream remains
        # available in the per-test log even if live output must be dropped.
        pass


def qemu_command(qemu, bios, flash, log):
    return [
        str(qemu), "-M", "esp32",
        "-drive", f"file={flash},if=mtd,format=raw,snapshot=on",
        "-display", "none", "-serial", f"file:{log}",
        "-monitor", "none", "-no-reboot",
        "-nic", "user,model=esp32_wifi", "-L", str(bios),
        "-global", "driver=timer.esp32.timg,property=wdt_disable,value=true",
    ]


def run_qemu(name, flash, args):
    log = WORK / name / "qemu.log"
    trace = WORK / name / "qemu.trace.log"
    log.parent.mkdir(parents=True, exist_ok=True)
    log.write_bytes(b"")
    command = qemu_command(args.qemu, args.bios, flash, log)
    print("+", shlex.join(command), flush=True)
    deadline = time.monotonic() + args.run_timeout
    transcript = ""
    pending_line = ""
    terminal_seen = False
    with trace.open("wb") as trace_output:
        process = subprocess.Popen(
            command, stdout=trace_output, stderr=subprocess.STDOUT,
            start_new_session=True,
        )
        try:
            with log.open("r", errors="replace") as serial:
                while time.monotonic() < deadline:
                    chunk = serial.read()
                    if chunk:
                        transcript = (transcript + chunk)[-TRANSCRIPT_LIMIT:]

                        pending_line += chunk
                        lines = pending_line.splitlines(keepends=True)
                        pending_line = ""
                        if lines and not lines[-1].endswith(("\n", "\r")):
                            pending_line = lines.pop()
                        for line in lines:
                            record = line.rstrip("\r\n")
                            if "QEMU_TEST," in record:
                                print_test_output(line)
                            if TERMINAL.fullmatch(record):
                                terminal_seen = True
                                break
                        if terminal_seen:
                            break
                    if process.poll() is not None:
                        break
                    time.sleep(0.05)
        finally:
            if process.poll() is None:
                os.killpg(process.pid, signal.SIGTERM)
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    os.killpg(process.pid, signal.SIGKILL)
                    process.wait()

    timed_out = not terminal_seen and time.monotonic() >= deadline
    return classify_output(name, transcript, log, timed_out, process.returncode or 0)


def command_prepare(args):
    preflight(args.qemu, args.bios)
    print(f"Ready: QEMU={args.qemu}; BIOS={args.bios}")


def command_test(args):
    WORK.mkdir(parents=True, exist_ok=True)
    preflight(args.qemu, args.bios)
    results = []
    for flash in discover_images():
        name = flash.parent.name
        print(f"\n=== {name} ===", flush=True)
        try:
            if flash.stat().st_size != FLASH_SIZE:
                raise RuntimeError(
                    f"QEMU flash image must be exactly 4 MiB: {flash}"
                )
            results.append(run_qemu(name, flash, args))
        except (RuntimeError, subprocess.CalledProcessError) as error:
            results.append(TestResult(name, "ERROR", detail=str(error)))

    print("\nQEMU test summary")
    for result in results:
        suffix = f": {result.detail}" if result.detail else ""
        if result.log:
            suffix += f" ({result.log})"
        print(f"  {result.status:5} {result.name}{suffix}")
    return 1 if any(result.status in ("FAIL", "ERROR") for result in results) else 0


def command_clean(_args):
    workspace = WORK.parent
    if workspace.exists():
        shutil.rmtree(workspace)
        print(f"Removed {workspace}")


def parser():
    common = argparse.ArgumentParser(add_help=False)
    common.add_argument("--qemu", type=pathlib.Path, default=DEFAULT_QEMU)
    common.add_argument("--bios", type=pathlib.Path, default=DEFAULT_BIOS)
    common.add_argument("--run-timeout", type=int, default=600)
    result = argparse.ArgumentParser(description=__doc__)
    commands = result.add_subparsers(dest="command", required=True)
    commands.add_parser("prepare", parents=[common]).set_defaults(function=command_prepare)
    commands.add_parser("test", parents=[common]).set_defaults(function=command_test)
    commands.add_parser("clean").set_defaults(function=command_clean)
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
