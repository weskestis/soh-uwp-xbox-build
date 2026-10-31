#!/usr/bin/env python3
"""Generate and validate ROM-free 2ship.o2r with a prebuilt GAME_MM exporter.

Build ZAPD separately with GAME_STR=MM. This command never discovers or extracts
ROMs, and only replaces the destination after checking every expected member.
"""

from __future__ import annotations

import argparse
from collections import Counter
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
# Fast::TextureType values and packed bits per pixel used by ZAPD's exporter.
TEXTURE_FORMATS = {"rgba32": (1, 32), "rgb5a1": (2, 16), "ci4": (3, 4),
                   "ci8": (4, 8), "i4": (5, 4), "i8": (6, 8),
                   "ia4": (7, 4), "ia8": (8, 8), "ia16": (9, 16)}


class ArchiveError(RuntimeError):
    """The exporter failed or its archive does not match the package inputs."""


def texture_dimensions(source: Path) -> tuple[int, int]:
    png = source.read_bytes()
    if len(png) < 24 or png[:8] != b"\x89PNG\r\n\x1a\n":
        raise ArchiveError(f"Invalid source PNG: {source}")
    width, height = struct.unpack_from(">2I", png, 16)
    if not (0 < width <= 65535 and 0 < height <= 65535):
        raise ArchiveError(f"Texture dimensions do not fit the runtime: {source}")
    _, bits = TEXTURE_FORMATS[source.name.rsplit(".", 2)[-2]]
    if bits == 4 and width % 2:
        # ZAPD packs pairs within each row, reading x and x+1 together.
        raise ArchiveError(f"4bpp texture needs an even width: {source}")
    return width, height


def project_version(repo: Path) -> tuple[int, int, int]:
    source = (repo / "CMakeLists.txt").read_text(encoding="utf-8")
    match = re.search(r"\bproject\s*\(\s*Ship\s+VERSION\s+(\d+)\.(\d+)\.(\d+)(?=\s|\))",
                      source, re.IGNORECASE)
    if match is None:
        raise ArchiveError("Expected a major.minor.patch Ship version in root CMakeLists.txt")
    version = tuple(int(part) for part in match.groups())
    if any(part > 65535 for part in version):
        raise ArchiveError("Port archive version fields must fit uint16")
    return version


def asset_manifest(custom: Path) -> dict[str, tuple[Path, bool]]:
    """Match the exporter's texture-name conversion and accessibility filter."""
    if not custom.is_dir():
        raise ArchiveError(f"Custom asset directory is missing: {custom}")
    members: dict[str, tuple[Path, bool]] = {}
    for path in sorted(custom.rglob("*")):
        if not path.is_file():
            continue
        name = path.relative_to(custom).as_posix()
        parts = path.name.rsplit(".", 2)
        texture = len(parts) == 3 and parts[-1] == "png" and parts[-2] in TEXTURE_FORMATS
        if texture:
            texture_dimensions(path)
            name = name.rsplit(".", 2)[0]
        elif "accessibility" in path.as_posix() and path.suffix != ".json":
            continue
        if name == "portVersion" or name in members:
            raise ArchiveError(f"Custom assets collide at archive member {name!r}")
        members[name] = (path, texture)
    if not members:
        raise ArchiveError("The custom asset manifest is empty")
    return members


def validate_archive(path: Path, members: dict[str, tuple[Path, bool]],
                     version: tuple[int, int, int]) -> None:
    try:
        with zipfile.ZipFile(path) as archive:
            names = archive.namelist()
            duplicates = [name for name, count in Counter(names).items() if count > 1]
            if duplicates:
                raise ArchiveError(f"Duplicate archive members: {duplicates[:5]}")
            expected = set(members) | {"portVersion"}
            if set(names) != expected:
                missing = sorted(expected - set(names))[:5]
                extra = sorted(set(names) - expected)[:5]
                raise ArchiveError(f"Archive manifest mismatch: missing={missing}, extra={extra}")
            corrupt = archive.testzip()
            if corrupt is not None:
                raise ArchiveError(f"Archive CRC check failed: {corrupt}")
            stamp = archive.read("portVersion")
            if len(stamp) != 7 or stamp[0] not in (0, 1):
                raise ArchiveError("Invalid portVersion encoding")
            if struct.unpack((">" if stamp[0] else "<") + "3H", stamp[1:]) != version:
                raise ArchiveError("Archive portVersion does not match the engine")
            for name, (source, texture) in members.items():
                data = archive.read(name)
                original = source.read_bytes()
                if not texture:
                    if data != original:
                        raise ArchiveError(f"Raw asset changed during export: {name}")
                    continue
                if (len(data) <= 0x50 or data[:4] != b"\0\0\0\0" or
                        struct.unpack_from("<2I", data, 4) != (0x4F544558, 0)):
                    raise ArchiveError(f"Texture was not converted to an OTEX resource: {name}")
                dimensions = texture_dimensions(source)
                if struct.unpack_from("<2I", data, 0x44) != dimensions:
                    raise ArchiveError(f"Texture dimensions changed during export: {name}")
                texture_type, bits = TEXTURE_FORMATS[source.name.rsplit(".", 2)[-2]]
                if struct.unpack_from("<I", data, 0x40)[0] != texture_type:
                    raise ArchiveError(f"Texture format changed during export: {name}")
                expected_size = (dimensions[0] * dimensions[1] * bits + 7) // 8
                declared_size = struct.unpack_from("<I", data, 0x4C)[0]
                padded_size = (declared_size + 7) & ~7
                if declared_size != expected_size or len(data) != 0x50 + padded_size:
                    raise ArchiveError(f"Texture payload size is invalid: {name}")
                if any(data[0x50 + declared_size:]):
                    raise ArchiveError(f"Texture padding is not zero: {name}")
    except (OSError, zipfile.BadZipFile, RuntimeError) as exc:
        if isinstance(exc, ArchiveError):
            raise
        raise ArchiveError(f"Cannot validate MM port archive: {exc}") from exc


def generate_archive(zapd: Path, output: Path, *, repo: Path = ROOT) -> Path:
    zapd = zapd.resolve()
    output = output.resolve()
    if not zapd.is_file():
        raise ArchiveError(f"GAME_MM ZAPD executable is missing: {zapd}")
    custom = repo.resolve() / "2ship/assets/custom"
    members = asset_manifest(custom)
    version = project_version(repo)
    output.parent.mkdir(parents=True, exist_ok=True)
    log = output.with_suffix(output.suffix + ".build.log")
    # The exporter silently skips an existing archive. Always use an empty,
    # private directory on the destination filesystem, then atomically publish.
    with tempfile.TemporaryDirectory(prefix="mm-port-", dir=output.parent) as temporary:
        staging = Path(temporary)
        generated = staging / "2ship.o2r"
        command = [str(zapd), "botr", "-se", "OTR", "--norom",
                   "--customAssetsPath", str(custom), "--customOtrFile", str(generated),
                   "--portVer", ".".join(map(str, version))]
        with log.open("wb") as diagnostics:
            try:
                result = subprocess.run(command, cwd=staging, stdout=diagnostics,
                                        stderr=subprocess.STDOUT, check=False)
            except OSError as exc:
                raise ArchiveError(f"Cannot run MM exporter: {exc}; log: {log}") from exc
        if result.returncode:
            raise ArchiveError(f"MM exporter exited {result.returncode}; log: {log}")
        validate_archive(generated, members, version)
        generated.replace(output)
    return output


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--zapd", required=True, type=Path, help="ZAPD built with GAME_STR=MM")
    parser.add_argument("--output", type=Path,
                        default=ROOT / "scratch/fusion-mm-assets/2ship.o2r")
    args = parser.parse_args()
    try:
        output = generate_archive(args.zapd, args.output)
    except (ArchiveError, OSError) as exc:
        parser.exit(1, f"{exc}\n")
    print(f"Validated MM port archive: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
