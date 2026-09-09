#!/usr/bin/env python3

import unittest

from analyze import evaluate, parse


def completed(loss: int, attempts: int = 6000) -> str:
    return (
        "QEMU_TLS,START,iterations=6000,warmup=32\n"
        f"QEMU_TLS,DONE,attempt={attempts},successes=0,failures={attempts},"
        f"start=100000,free={100000 - loss},min={100000 - loss},largest=50000,loss={loss}\n"
    )


class AnalyzeTests(unittest.TestCase):
    def test_accepts_leaking_baseline_and_stable_patch(self):
        baseline = parse("baseline", completed(50000))
        patched = parse("patched", completed(512))
        self.assertTrue(evaluate(baseline, patched, 6000, 16384, 4096)[0])

    def test_rejects_patched_leak(self):
        baseline = parse("baseline", completed(50000))
        patched = parse("patched", completed(5000))
        self.assertFalse(evaluate(baseline, patched, 6000, 16384, 4096)[0])

    def test_accepts_baseline_exhaustion_after_progress(self):
        baseline = parse(
            "baseline",
            "QEMU_TLS,START,iterations=6000,warmup=32\n"
            "QEMU_TLS,SAMPLE,attempt=4200,successes=0,failures=4200,free=100,min=100,largest=40\n",
        )
        patched = parse("patched", completed(0))
        self.assertTrue(evaluate(baseline, patched, 6000, 16384, 4096)[0])

    def test_rejects_missing_harness_output(self):
        baseline = parse("baseline", "qemu: fatal error\n")
        patched = parse("patched", completed(0))
        self.assertFalse(evaluate(baseline, patched, 6000, 16384, 4096)[0])

    def test_rejects_early_incomplete_baseline(self):
        baseline = parse(
            "baseline",
            "QEMU_TLS,SAMPLE,attempt=100,successes=0,failures=100,free=100,min=100,largest=40\n",
        )
        patched = parse("patched", completed(0))
        self.assertFalse(evaluate(baseline, patched, 6000, 16384, 4096)[0])

    def test_propagates_skip(self):
        result = parse("https", "QEMU_TLS,SKIP,reason=wifi-association-timeout\n")
        self.assertEqual("SKIP", result.state)


if __name__ == "__main__":
    unittest.main()
