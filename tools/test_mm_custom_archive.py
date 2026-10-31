#!/usr/bin/env python3
"""Exercise archive validation and atomic publication without game/ROM inputs."""

from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import warnings
import zipfile

from build_mm_custom_archive import ArchiveError, asset_manifest, generate_archive, validate_archive


class ArchiveTests(unittest.TestCase):
    def setUp(self):
        scratch = Path(__file__).resolve().parents[1] / "scratch"
        scratch.mkdir(exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(prefix="mm-archive-test-", dir=scratch)
        self.addCleanup(self.temporary.cleanup)
        self.repo = Path(self.temporary.name)
        (self.repo / "CMakeLists.txt").write_text("project(Ship VERSION 9.2.3 LANGUAGES C CXX)\n")
        custom = self.repo / "2ship/assets/custom"
        custom.mkdir(parents=True)
        (custom / "raw.bin").write_bytes(b"asset bytes")
        (custom / "icon.rgba32.png").write_bytes(
            b"\x89PNG\r\n\x1a\n" + b"\0\0\0\rIHDR" + struct.pack(">2I", 2, 3))
        self.members = asset_manifest(custom)
        self.stamp = b"\1" + struct.pack(">3H", 9, 2, 3)
        texture = bytearray(0x50 + 24)
        struct.pack_into("<I", texture, 4, 0x4F544558)
        struct.pack_into("<I", texture, 0x40, 1)
        struct.pack_into("<2I", texture, 0x44, 2, 3)
        struct.pack_into("<I", texture, 0x4C, 24)
        self.contents = {"portVersion": self.stamp, "raw.bin": b"asset bytes", "icon": bytes(texture)}
        self.output = self.repo / "2ship.o2r"
        self.zapd = self.repo / "ZAPD.out"
        self.zapd.touch()

    def write_archive(self, path=None, contents=None):
        path = path or self.output
        with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_STORED) as archive:
            for name, data in (self.contents if contents is None else contents).items():
                archive.writestr(name, data)
        return path

    def validate(self):
        validate_archive(self.output, self.members, (9, 2, 3))

    def test_valid_archive_and_both_version_endians(self):
        for stamp in (self.stamp, b"\0" + struct.pack("<3H", 9, 2, 3)):
            self.write_archive(contents={**self.contents, "portVersion": stamp})
            self.validate()

    def test_rejects_missing_extra_and_duplicate_members(self):
        cases = [
            {name: data for name, data in self.contents.items() if name != "icon"},
            {**self.contents, "version": b"unexpected ROM data"},
        ]
        for contents in cases:
            self.write_archive(contents=contents)
            with self.assertRaisesRegex(ArchiveError, "manifest mismatch"):
                self.validate()
        self.write_archive()
        with warnings.catch_warnings():
            warnings.simplefilter("ignore", UserWarning)
            with zipfile.ZipFile(self.output, "a") as archive:
                archive.writestr("icon", self.contents["icon"])
        with self.assertRaisesRegex(ArchiveError, "Duplicate"):
            self.validate()

    def test_rejects_wrong_version_encoding_and_version(self):
        for stamp in (b"", b"\2" + self.stamp[1:], self.stamp + b"\0",
                      b"\1" + struct.pack(">3H", 9, 2, 4)):
            self.write_archive(contents={**self.contents, "portVersion": stamp})
            with self.assertRaisesRegex(ArchiveError, "portVersion"):
                self.validate()

    def test_rejects_raw_asset_changes_and_unconverted_texture(self):
        for name, data in (("raw.bin", b"wrong"), ("icon", b"\x89PNG\r\n\x1a\n")):
            self.write_archive(contents={**self.contents, name: data})
            with self.assertRaises(ArchiveError):
                self.validate()

    def test_rejects_changed_texture_dimensions(self):
        texture = bytearray(self.contents["icon"])
        struct.pack_into("<I", texture, 0x44, 8)
        self.write_archive(contents={**self.contents, "icon": texture})
        with self.assertRaisesRegex(ArchiveError, "dimensions"):
            self.validate()

    def test_rejects_wrong_texture_type_and_resource_version(self):
        for offset, value in ((0x40, 0), (0x40, 2), (8, 1)):
            texture = bytearray(self.contents["icon"])
            struct.pack_into("<I", texture, offset, value)
            self.write_archive(contents={**self.contents, "icon": texture})
            with self.assertRaises(ArchiveError):
                self.validate()

    def test_rejects_truncated_texture_and_wrong_declared_size(self):
        texture = bytearray(self.contents["icon"])
        struct.pack_into("<I", texture, 0x4C, 1)
        for data in (texture, texture[:0x51], self.contents["icon"][:-1]):
            self.write_archive(contents={**self.contents, "icon": data})
            with self.assertRaisesRegex(ArchiveError, "payload size"):
                self.validate()

    def test_rejects_corrupt_crc_and_non_zip(self):
        self.write_archive()
        data = self.output.read_bytes().replace(b"asset bytes", b"asset byte!")
        self.output.write_bytes(data)
        with self.assertRaisesRegex(ArchiveError, "CRC"):
            self.validate()
        self.output.write_bytes(b"not a zip")
        with self.assertRaises(ArchiveError):
            self.validate()

    def test_rejects_export_name_collision(self):
        (self.repo / "2ship/assets/custom/icon").touch()
        with self.assertRaisesRegex(ArchiveError, "collide"):
            asset_manifest(self.repo / "2ship/assets/custom")

    def exporter(self, command, *, cwd, stdout, stderr, check):
        self.assertIn("--norom", command)
        destination = Path(command[command.index("--customOtrFile") + 1])
        self.assertEqual(destination.parent, cwd)
        self.assertFalse(destination.exists())
        self.assertEqual(self.output.read_bytes(), b"previous working archive")
        self.write_archive(destination)
        return subprocess.CompletedProcess(command, 0)

    def test_publishes_only_valid_output(self):
        self.output.write_bytes(b"previous working archive")
        with patch("build_mm_custom_archive.subprocess.run", side_effect=self.exporter):
            generate_archive(self.zapd, self.output, repo=self.repo)
        self.validate()

    def test_preserves_existing_archive_after_exporter_failure(self):
        self.output.write_bytes(b"previous working archive")
        def failed_exporter(*args, **kwargs):
            completed = self.exporter(*args, **kwargs)
            completed.returncode = 5
            return completed
        with patch("build_mm_custom_archive.subprocess.run", side_effect=failed_exporter):
            with self.assertRaisesRegex(ArchiveError, "exited 5"):
                generate_archive(self.zapd, self.output, repo=self.repo)
        self.assertEqual(self.output.read_bytes(), b"previous working archive")

    def test_preserves_existing_archive_after_invalid_success_output(self):
        self.output.write_bytes(b"previous working archive")
        with patch("build_mm_custom_archive.subprocess.run",
                   return_value=subprocess.CompletedProcess([], 0)):
            with self.assertRaises(ArchiveError):
                generate_archive(self.zapd, self.output, repo=self.repo)
        self.assertEqual(self.output.read_bytes(), b"previous working archive")


if __name__ == "__main__":
    unittest.main()
