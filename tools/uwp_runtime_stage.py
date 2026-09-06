#!/usr/bin/env python3
"""Stage and audit the ROM-free Xbox/UWP runtime with host-testable contracts."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path, PurePosixPath
import shutil
import sys
import xml.etree.ElementTree as ET
import zipfile


CORE_FILES = ("soh_core.dll", "soh_core.lib")
ASSET_SENTINELS = (
    "assets/Config_N64_NTSC_10.xml",
    "assets/xml/GC_MQ_D/audio/Audio.xml",
    "assets/rml/zelda3d_launcher.rml",
)
UWP_DEPENDENCIES = (
    "x64/bin/SDL2.dll",
    "x64/bin/dxil.dll",
    "x64/bin/glfw3.dll",
    "x64/bin/libgallium_wgl.dll",
    "x64/bin/libuwp.dll",
    "x64/bin/opengl32.dll",
    "x64/bin/z-1.dll",
    "x64/lib/SDL2.lib",
    "x64/lib/libuwp.lib",
)
PACKAGED_RUNTIME_FILES = (
    "soh_core.dll",
    "soh.o2r",
    "SDL2.dll",
    "dxil.dll",
    "glfw3.dll",
    "libgallium_wgl.dll",
    "libuwp.dll",
    "opengl32.dll",
    "z-1.dll",
    "assets/rml/zelda3d_launcher.rml",
)
PACKAGE_ICONS = {
    "Assets/LockScreenLogo.scale-200.png": (
        (48, 48),
        "38b8f36a9aa15369204ea94808d9ce800bed273a2fca940b966ea1475dbc8b17",
    ),
    "Assets/Square150x150Logo.scale-200.png": (
        (300, 300),
        "a92c620259b1bfbf61d194a5c03344954f8368b682ceff3a115d6126ed878cd6",
    ),
    "Assets/Square44x44Logo.scale-200.png": (
        (88, 88),
        "0ba8d9c868cfa3bc707d252856b480d163ccc54d681c690b950e50a46b31df1e",
    ),
    "Assets/Square44x44Logo.targetsize-24_altform-unplated.png": (
        (24, 24),
        "20992449b4b855cb127fe27669c27f9eac5f84158719a39f341d0a8598fb2132",
    ),
    "Assets/StoreLogo.png": (
        (50, 50),
        "9f364abc09920fa42a12a2fcd3617bdf022ae756ed6f4ac9cec56ef9b2dbaae0",
    ),
}
PRIVATE_NAMES = frozenset(("oot.o2r", "oot-mq.o2r"))
PRIVATE_SUFFIXES = frozenset((".3ds", ".cia", ".z64", ".n64", ".v64", ".otr", ".pfx", ".p12"))
UNNEEDED_PACKAGE_SUFFIXES = frozenset((".lib", ".pdb", ".zip"))


def require_file(path: Path, description: str) -> None:
    if not path.is_file():
        raise RuntimeError(f"Missing {description}: {path}")
    if path.stat().st_size == 0:
        raise RuntimeError(f"Empty {description}: {path}")


def png_dimensions(path: Path) -> tuple[int, int]:
    header = path.read_bytes()[:24]
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise RuntimeError(f"Package icon is not a valid PNG: {path}")
    return int.from_bytes(header[16:20], "big"), int.from_bytes(header[20:24], "big")


def reject_private_files(root: Path, *, packaged: bool = False) -> None:
    for path in sorted(item for item in root.rglob("*") if item.is_file()):
        lowered_name = path.name.lower()
        if lowered_name in PRIVATE_NAMES or path.suffix.lower() in PRIVATE_SUFFIXES:
            raise RuntimeError(f"Private file entered ROM-free runtime: {path.relative_to(root)}")
        if packaged and path.suffix.lower() in UNNEEDED_PACKAGE_SUFFIXES:
            raise RuntimeError(f"Build-only file entered AppX: {path.relative_to(root)}")


def add_tree_to_zip(source: Path, archive: Path) -> None:
    archive.parent.mkdir(parents=True, exist_ok=True)
    if archive.exists():
        raise RuntimeError(f"Refusing to overwrite core runtime archive: {archive}")
    with zipfile.ZipFile(
        archive,
        mode="x",
        compression=zipfile.ZIP_DEFLATED,
        compresslevel=1,
        allowZip64=True,
    ) as output:
        for path in sorted(item for item in source.rglob("*") if item.is_file()):
            output.write(path, path.relative_to(source).as_posix())
    require_file(archive, "Windows core runtime archive")
    with zipfile.ZipFile(archive, mode="r") as check:
        corrupt = check.testzip()
        if corrupt is not None:
            raise RuntimeError(f"Corrupt Windows core runtime archive member: {corrupt}")


def verify_core_archive(archive: Path) -> None:
    require_file(archive, "preserved Windows core runtime archive")
    with zipfile.ZipFile(archive, mode="r") as source:
        corrupt = source.testzip()
        if corrupt is not None:
            raise RuntimeError(f"Corrupt Windows core runtime archive member: {corrupt}")

        members: dict[str, zipfile.ZipInfo] = {}
        for info in source.infolist():
            relative = PurePosixPath(info.filename)
            if relative.is_absolute() or ".." in relative.parts or "\\" in info.filename:
                raise RuntimeError(f"Unsafe Windows core runtime archive member: {info.filename}")
            normalized = relative.as_posix()
            if normalized in members:
                raise RuntimeError(f"Duplicate Windows core runtime archive member: {normalized}")
            members[normalized] = info
            if info.is_dir():
                continue
            if relative.name.lower() in PRIVATE_NAMES or relative.suffix.lower() in PRIVATE_SUFFIXES:
                raise RuntimeError(f"Private file entered Windows core archive: {normalized}")

        for relative in (*CORE_FILES, *ASSET_SENTINELS):
            info = members.get(relative)
            if info is None or info.is_dir():
                raise RuntimeError(f"Missing Windows core archive member: {relative}")
            if info.file_size == 0:
                raise RuntimeError(f"Empty Windows core archive member: {relative}")


def extract_zip_safely(archive: Path, destination: Path) -> None:
    if destination.exists():
        raise RuntimeError(f"Refusing to overwrite runtime directory: {destination}")
    destination.mkdir(parents=True)
    with zipfile.ZipFile(archive, mode="r") as source:
        corrupt = source.testzip()
        if corrupt is not None:
            raise RuntimeError(f"Corrupt Windows core runtime archive member: {corrupt}")
        for info in source.infolist():
            relative = PurePosixPath(info.filename)
            if relative.is_absolute() or ".." in relative.parts:
                raise RuntimeError(f"Unsafe Windows core runtime archive member: {info.filename}")
            target = destination.joinpath(*relative.parts)
            if info.is_dir():
                target.mkdir(parents=True, exist_ok=True)
                continue
            target.parent.mkdir(parents=True, exist_ok=True)
            with source.open(info, mode="r") as input_file, target.open("wb") as output_file:
                shutil.copyfileobj(input_file, output_file)


def stage_core(source_root: Path, stage_dir: Path, archive: Path) -> None:
    # Shipwright/CMake/DefaultCXX.cmake sets all Release products here. This is
    # intentionally outside the CMake binary directory.
    core_output = source_root.resolve() / "x64" / "Release"
    for relative in (*CORE_FILES, *ASSET_SENTINELS):
        require_file(core_output / relative, "exact Windows core output")
    if stage_dir.exists():
        raise RuntimeError(f"Refusing to overwrite core runtime stage: {stage_dir}")

    stage_dir.mkdir(parents=True)
    for filename in CORE_FILES:
        shutil.copy2(core_output / filename, stage_dir / filename)
    shutil.copytree(core_output / "assets", stage_dir / "assets")
    (stage_dir / "deps").mkdir()
    reject_private_files(stage_dir)
    add_tree_to_zip(stage_dir, archive)


def assemble_runtime(archive: Path, port_archive: Path, deps_root: Path, runtime_dir: Path) -> None:
    verify_core_archive(archive)
    require_file(port_archive, "redistributable soh.o2r")
    extract_zip_safely(archive, runtime_dir)
    (runtime_dir / "deps").mkdir(exist_ok=True)
    shutil.copy2(port_archive, runtime_dir / "soh.o2r")

    for relative in (*CORE_FILES, "soh.o2r", *ASSET_SENTINELS):
        require_file(runtime_dir / relative, "wrapper runtime input")
    for relative in UWP_DEPENDENCIES:
        require_file(deps_root / relative, "pinned UWP dependency")
    reject_private_files(runtime_dir)


def audit_appx(unpacked_dir: Path, publisher: str) -> None:
    if not unpacked_dir.is_dir():
        raise RuntimeError(f"Unpacked AppX directory is missing: {unpacked_dir}")
    for relative in PACKAGED_RUNTIME_FILES:
        require_file(unpacked_dir / relative, "packed AppX runtime file")
    for relative, (dimensions, expected_hash) in PACKAGE_ICONS.items():
        icon = unpacked_dir / relative
        require_file(icon, "packed custom app icon")
        if png_dimensions(icon) != dimensions:
            raise RuntimeError(f"Packed app icon has the wrong dimensions: {relative}")
        if hashlib.sha256(icon.read_bytes()).hexdigest() != expected_hash:
            raise RuntimeError(f"Packed app icon is not the approved Master Quest Flames artwork: {relative}")
    reject_private_files(unpacked_dir, packaged=True)

    manifest_path = unpacked_dir / "AppxManifest.xml"
    require_file(manifest_path, "packed AppX manifest")
    manifest = ET.parse(manifest_path).getroot()
    namespace = {"f": "http://schemas.microsoft.com/appx/manifest/foundation/windows10"}
    identity = manifest.find("f:Identity", namespace)
    if identity is None:
        raise RuntimeError("Packed AppX manifest has no Identity")
    if identity.attrib.get("Publisher") != publisher:
        raise RuntimeError("Packed AppX publisher does not match signing certificate subject")
    if identity.attrib.get("Name") == "Shipwright":
        raise RuntimeError("Packed AppX reused the original V3 package identity")


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)

    stage = commands.add_parser("stage-core", help="Stage and archive the exact core output")
    stage.add_argument("--source-root", type=Path, required=True)
    stage.add_argument("--stage-dir", type=Path, required=True)
    stage.add_argument("--archive", type=Path, required=True)

    verify = commands.add_parser("verify-core", help="Verify a preserved core archive")
    verify.add_argument("--archive", type=Path, required=True)

    assemble = commands.add_parser("assemble", help="Assemble and validate wrapper inputs")
    assemble.add_argument("--archive", type=Path, required=True)
    assemble.add_argument("--port-archive", type=Path, required=True)
    assemble.add_argument("--deps-root", type=Path, required=True)
    assemble.add_argument("--runtime-dir", type=Path, required=True)

    audit = commands.add_parser("audit-appx", help="Audit an unpacked AppX")
    audit.add_argument("--unpacked-dir", type=Path, required=True)
    audit.add_argument("--publisher", required=True)
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        if args.command == "stage-core":
            stage_core(args.source_root, args.stage_dir, args.archive)
        elif args.command == "verify-core":
            verify_core_archive(args.archive)
        elif args.command == "assemble":
            assemble_runtime(args.archive, args.port_archive, args.deps_root, args.runtime_dir)
        else:
            audit_appx(args.unpacked_dir, args.publisher)
    except (OSError, RuntimeError, ET.ParseError, zipfile.BadZipFile) as error:
        print(f"UWP runtime contract failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
