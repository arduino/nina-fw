#!/usr/bin/env python3
"""Analyze structured output from the NINA QEMU TLS stress harness."""

import argparse
import pathlib
import re
import sys
from dataclasses import dataclass

FIELD = re.compile(r"([a-z]+)=([^,\r\n]+)")


@dataclass
class Result:
    name: str
    state: str
    attempts: int = 0
    start: int = 0
    free: int = 0
    minimum: int = 0
    loss: int = 0
    detail: str = ""


def parse(name: str, text: str) -> Result:
    records = [line.strip()
               for line in text.splitlines() if "QEMU_TLS," in line]
    records = [line[line.index("QEMU_TLS,"):] for line in records]
    if not records:
        return Result(name, "INFRA_ERROR", detail="no harness output")

    for line in records:
        if line.startswith("QEMU_TLS,INFRA_ERROR"):
            return Result(name, "INFRA_ERROR", detail=line)
        if line.startswith("QEMU_TLS,SKIP"):
            return Result(name, "SKIP", detail=line)

    done = next((line for line in reversed(records)
                if line.startswith("QEMU_TLS,DONE")), None)
    if done is None:
        samples = [
            line for line in records if ",SAMPLE," in line or ",WARMUP_DONE," in line]
        attempts = 0
        if samples:
            attempts = int(dict(FIELD.findall(samples[-1])).get("attempt", 0))
        return Result(name, "INCOMPLETE", attempts=attempts, detail="missing DONE record")

    fields = dict(FIELD.findall(done))
    return Result(
        name=name,
        state="DONE",
        attempts=int(fields.get("attempt", 0)),
        start=int(fields.get("start", 0)),
        free=int(fields.get("free", 0)),
        minimum=int(fields.get("min", 0)),
        loss=int(fields.get("loss", 0)),
    )


def evaluate(baseline: Result, patched: Result, iterations: int, baseline_loss: int, patched_loss: int) -> tuple[bool, str]:
    if patched.state != "DONE":
        return False, f"patched run did not complete: {patched.state} ({patched.detail})"
    if patched.attempts < iterations:
        return False, f"patched run completed only {patched.attempts}/{iterations} attempts"
    if patched.loss > patched_loss:
        return False, f"patched heap loss {patched.loss} exceeds {patched_loss} bytes"

    baseline_reproduced = (
        baseline.state == "DONE" and baseline.loss >= baseline_loss
    ) or (
        baseline.state == "INCOMPLETE" and baseline.attempts >= iterations // 2
    )
    if not baseline_reproduced:
        return False, "baseline did not reproduce the expected leak or sustained exhaustion"

    if baseline.state == "DONE" and patched.loss * 4 > baseline.loss:
        return False, "patched heap loss is not at least four times smaller than baseline"
    return True, "baseline leak reproduced and patched heap remained stable"


def print_table(results: list[Result]) -> None:
    print("run       state        attempts   start_heap   final_heap   min_heap   loss")
    for result in results:
        print(
            f"{result.name:<9} {result.state:<12} {result.attempts:>8} "
            f"{result.start:>12} {result.free:>12} {result.minimum:>10} {result.loss:>7}"
        )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("baseline", type=pathlib.Path)
    parser.add_argument("patched", type=pathlib.Path)
    parser.add_argument("--iterations", type=int, default=6000)
    parser.add_argument("--baseline-loss", type=int, default=16384)
    parser.add_argument("--patched-loss", type=int, default=4096)
    args = parser.parse_args()

    baseline = parse("baseline", args.baseline.read_text(errors="replace"))
    patched = parse("patched", args.patched.read_text(errors="replace"))
    print_table([baseline, patched])
    passed, reason = evaluate(
        baseline, patched, args.iterations, args.baseline_loss, args.patched_loss
    )
    print(("PASS: " if passed else "FAIL: ") + reason)
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
