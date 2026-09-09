#!/usr/bin/env python3

import pathlib
import tempfile
import time
import unittest

from qemu_test import synchronize


class SynchronizeTest(unittest.TestCase):
    def test_updates_sources_without_touching_unchanged_files_or_build(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            source = root / "source"
            staged = root / "staged"
            (source / "build").mkdir(parents=True)
            staged.mkdir()

            unchanged = source / "unchanged.cpp"
            unchanged.write_text("same")
            original_mtime = unchanged.stat().st_mtime_ns
            changed = source / "changed.cpp"
            changed.write_text("old")
            changed_mtime = changed.stat().st_mtime_ns
            (source / "removed.cpp").write_text("removed")
            (source / "build/object.o").write_text("cached")

            time.sleep(0.01)
            (staged / "unchanged.cpp").write_text("same")
            (staged / "changed.cpp").write_text("new")
            (staged / "added.cpp").write_text("added")

            synchronize(source, staged)

            self.assertEqual(original_mtime, unchanged.stat().st_mtime_ns)
            self.assertEqual("new", (source / "changed.cpp").read_text())
            self.assertGreater(changed.stat().st_mtime_ns, changed_mtime)
            self.assertEqual("added", (source / "added.cpp").read_text())
            self.assertFalse((source / "removed.cpp").exists())
            self.assertEqual("cached", (source / "build/object.o").read_text())

    def test_handles_file_type_changes(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            source = root / "source"
            staged = root / "staged"
            source.mkdir()
            (source / "entry").write_text("file")
            (staged / "entry").mkdir(parents=True)
            (staged / "entry/child").write_text("child")

            synchronize(source, staged)

            self.assertEqual("child", (source / "entry/child").read_text())


if __name__ == "__main__":
    unittest.main()
