#!/usr/bin/env python3

import pathlib
import tempfile
import unittest

from qemu_test import classify_output, discover_images


class DiscoveryTest(unittest.TestCase):
    def test_discovers_built_test_images_in_order(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            for name in ("zeta", "alpha"):
                (root / name).mkdir()
                (root / name / f"{name}.flash.bin").write_bytes(b"image")
            (root / "documentation").mkdir()

            self.assertEqual(
                ["alpha", "zeta"],
                [image.parent.name for image in discover_images(root)],
            )

    def test_ignores_incomplete_test_outputs(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            (root / "complete").mkdir()
            (root / "complete/complete.flash.bin").write_bytes(b"image")
            (root / "incomplete").mkdir()
            (root / "incomplete/incomplete.bin").write_bytes(b"image")

            self.assertEqual(
                ["complete"],
                [image.parent.name for image in discover_images(root)],
            )

    def test_rejects_an_empty_test_root(self):
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaises(RuntimeError):
                discover_images(pathlib.Path(temporary))


class ResultTest(unittest.TestCase):
    def test_classifies_terminal_results(self):
        for status in ("PASS", "FAIL", "SKIP"):
            result = classify_output(
                "sample", f"boot\nQEMU_TEST,{status},name=sample,detail=value\n"
            )
            self.assertEqual(status, result.status)

    def test_rejects_wrong_name_and_missing_result(self):
        wrong = classify_output("sample", "QEMU_TEST,PASS,name=other\n")
        missing = classify_output("sample", "boot failed\n", returncode=1)
        timed_out = classify_output("sample", "still running\n", timed_out=True)
        self.assertEqual("ERROR", wrong.status)
        self.assertEqual("ERROR", missing.status)
        self.assertEqual("ERROR", timed_out.status)


if __name__ == "__main__":
    unittest.main()
