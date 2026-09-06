#!/usr/bin/env python3
"""Executable acceptance test for folder/Zip64-capable OoT3D texture-pack loading."""

from __future__ import annotations

import binascii
import json
import os
import struct
import subprocess
import tempfile
import unittest
import zipfile
import zlib
from pathlib import Path


REPO = Path(__file__).resolve().parents[1]
CPP_TEST = REPO / "tools" / "texpack_loader_test.cpp"
TEXPACK_CPP = REPO / "Shipwright" / "cmb3d" / "asset" / "texpack.cpp"
TITLE_ID = "0004000000033500"
HASH = "0123456789ABCDEF"


def png_rgba_2x2() -> bytes:
    """Two red pixels above two blue pixels, with no external image dependency."""

    signature = b"\x89PNG\r\n\x1a\n"

    def chunk(kind: bytes, payload: bytes) -> bytes:
        body = kind + payload
        return struct.pack(">I", len(payload)) + body + struct.pack(">I", binascii.crc32(body) & 0xFFFFFFFF)

    ihdr = struct.pack(">IIBBBBB", 2, 2, 8, 6, 0, 0, 0)
    red = bytes((255, 0, 0, 255)) * 2
    blue = bytes((0, 0, 255, 255)) * 2
    pixels = b"\0" + red + b"\0" + blue
    return signature + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(pixels)) + chunk(b"IEND", b"")


def write_pack(root: Path, *, title_id: str = TITLE_ID, use_new_hash: bool = False,
               flip_png_files: bool = False, name: str = "Fixture Folder", version: str = "v1",
               mip: int = 0, invalid_png: bool = False) -> None:
    pack = root / title_id
    pack.mkdir(parents=True)
    (pack / "pack.json").write_text(
        json.dumps(
            {
                "name": name,
                "version": version,
                "use_new_hash": use_new_hash,
                "flip_png_files": flip_png_files,
            }
        ),
        encoding="utf-8",
    )
    encoded = b"not a png" if invalid_png else png_rgba_2x2()
    (pack / f"tex1_2x2_{HASH}_0_mip{mip}.png").write_bytes(encoded)


def find_stb_include() -> Path:
    configured = os.environ.get("ZELDA3D_BUILD_DIR")
    candidates = []
    if configured:
        candidates.append(Path(configured) / "_deps" / "stb")
    candidates.extend(
        (
            REPO.parent / "build-linux-full" / "_deps" / "stb",
            REPO / "build" / "_deps" / "stb",
        )
    )
    for candidate in candidates:
        if (candidate / "stb_image.h").is_file():
            return candidate
    raise RuntimeError("stb_image.h not found; configure the main build or set ZELDA3D_BUILD_DIR")


class TexturePackLoaderTest(unittest.TestCase):
    def test_directory_zip_validation_toggle_and_decode(self) -> None:
        scratch = REPO / "scratch"
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="texpack-test-", dir=scratch) as temporary:
            root = Path(temporary)
            valid_dir = root / "valid-dir"
            write_pack(valid_dir)
            legacy = valid_dir / TITLE_ID / f"tex1_2x2_FEDCBA9876543210_0.png"
            legacy.write_bytes(png_rgba_2x2())

            zip_source = root / "zip-source"
            write_pack(zip_source, flip_png_files=True, name="Fixture ZIP", version="v2")
            valid_zip = root / "fixture.zip"
            with zipfile.ZipFile(valid_zip, "w", compression=zipfile.ZIP_DEFLATED, allowZip64=True) as archive:
                for path in zip_source.rglob("*"):
                    if path.is_file():
                        archive_name = (Path("Henriko Fixture") / path.relative_to(zip_source)).as_posix()
                        if path.suffix.lower() == ".png":
                            # Force the Zip64 local-entry encoding even though this tiny fixture does
                            # not need it, so the path used by multi-gigabyte archives is executable.
                            with archive.open(archive_name, "w", force_zip64=True) as destination:
                                destination.write(path.read_bytes())
                        else:
                            archive.write(path, archive_name)

            new_hash = root / "new-hash"
            write_pack(new_hash, use_new_hash=True)
            foreign = root / "foreign"
            write_pack(foreign, title_id="0004000000033600")
            mip1 = root / "mip1"
            write_pack(mip1, mip=1)
            invalid_png = root / "invalid-png"
            write_pack(invalid_png, invalid_png=True)

            executable = root / "texpack_loader_test"
            compile_command = [
                os.environ.get("CXX", "c++"),
                "-std=c++20",
                "-O2",
                str(CPP_TEST),
                str(TEXPACK_CPP),
                "-I",
                str(REPO / "Shipwright" / "cmb3d"),
                "-I",
                str(find_stb_include()),
                "-lzip",
                "-pthread",
                "-o",
                str(executable),
            ]
            subprocess.run(compile_command, check=True, cwd=REPO)
            result = subprocess.run(
                [
                    str(executable),
                    str(valid_dir),
                    str(valid_zip),
                    str(new_hash),
                    str(foreign),
                    str(mip1),
                    str(invalid_png),
                ],
                check=True,
                cwd=REPO,
                capture_output=True,
                text=True,
            )
            self.assertIn("texpack_loader_test: PASS", result.stdout)


if __name__ == "__main__":
    unittest.main()
