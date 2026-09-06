#!/usr/bin/env python3
"""Resolve user-supplied Zelda ROMs and expose N64 extraction input safely."""

from __future__ import annotations

import argparse
import os
import shlex
import sys
from collections.abc import Mapping, Sequence
from pathlib import Path

from repo_environment import apply_repo_environment
from rom_identity import (
    is_oot3d_rom,
    is_oot_master_quest_n64_rom,
    is_oot_n64_rom,
    is_oot_normal_n64_rom,
)

OOT3D_NAME = "ZELDA3D_OOT3D_ROM"
OOT3D_ROMFS_NAME = "ZELDA3D_OOT3D_ROMFS"
OOT_NAME = "ZELDA3D_OOT_ROM"
OOT_MQ_NAME = "ZELDA3D_OOT_MQ_ROM"
OOT3D_PATTERNS = ("oot3d.3ds", "*.3ds")
OOT_PATTERNS = ("oot.z64", "*.z64", "*.n64", "*.v64")
OOT_MQ_PATTERNS = (
    "oot-mq.z64",
    "oot-mq.n64",
    "oot-mq.v64",
    "*.z64",
    "*.n64",
    "*.v64",
)
N64_ROM_PATTERNS = ("*.z64", "*.n64", "*.v64")


class RomProvisionError(RuntimeError):
    """Raised when a ROM input cannot be exposed without overwriting local data."""


def _first_file(
    repo: Path,
    patterns: Sequence[str],
    predicate=lambda _path: True,
) -> Path | None:
    seen: set[Path] = set()
    matches: list[Path] = []
    for pattern in patterns:
        for candidate in sorted(repo.glob(pattern)):
            if candidate in seen:
                continue
            seen.add(candidate)
            if candidate.is_file() and predicate(candidate):
                matches.append(candidate.resolve())
        if matches:
            break
    if len(matches) > 1:
        raise RomProvisionError(
            "multiple Ocarina of Time ROM drop-ins match; set the corresponding "
            "ZELDA3D_OOT* environment variable explicitly"
        )
    return matches[0] if matches else None


def resolve_rom_environment(
    repo: Path, environment: Mapping[str, str] | None = None
) -> dict[str, str]:
    """Apply caller > .env > canonical/drop-in priority without executing `.env`."""
    repo = repo.resolve()
    resolved = dict(os.environ if environment is None else environment)
    names = (OOT3D_ROMFS_NAME, OOT3D_NAME, OOT_NAME, OOT_MQ_NAME)
    caller_values = {name for name in names if resolved.get(name)}
    apply_repo_environment(repo, resolved)
    for name in names:
        if name in caller_values or not resolved.get(name):
            continue
        path = Path(resolved[name]).expanduser()
        resolved[name] = str(path if path.is_absolute() else (repo / path).resolve())
    if not resolved.get(OOT3D_ROMFS_NAME) and not resolved.get(OOT3D_NAME):
        romfs = repo / "oot3d-romfs"
        if romfs.is_dir():
            resolved[OOT3D_ROMFS_NAME] = str(romfs.resolve())
    if not resolved.get(OOT3D_ROMFS_NAME) and not resolved.get(OOT3D_NAME):
        oot3d = _first_file(repo, OOT3D_PATTERNS, is_oot3d_rom)
        if oot3d is not None:
            resolved[OOT3D_NAME] = str(oot3d)
    if not resolved.get(OOT_NAME):
        oot = _first_file(repo, OOT_PATTERNS, is_oot_normal_n64_rom)
        if oot is not None:
            resolved[OOT_NAME] = str(oot)
    if not resolved.get(OOT_MQ_NAME):
        oot_mq = _first_file(repo, OOT_MQ_PATTERNS, is_oot_master_quest_n64_rom)
        if oot_mq is not None:
            resolved[OOT_MQ_NAME] = str(oot_mq)
    return resolved


def _provision_n64_extraction_rom(
    app_dir: Path,
    environment: Mapping[str, str],
    *,
    environment_name: str,
    archive_names: Sequence[str],
    destination_stem: str,
    master_quest: bool,
) -> Path | None:
    rom_value = environment.get(environment_name)
    if not rom_value:
        return None
    rom = Path(rom_value).expanduser().resolve()
    if not rom.is_file():
        raise RomProvisionError(
            f"{environment_name} does not name an existing file: {rom}"
        )
    if not is_oot_n64_rom(rom):
        raise RomProvisionError(
            f"{environment_name} is not a recognized Ocarina of Time ROM: {rom}"
        )
    actual_master_quest = is_oot_master_quest_n64_rom(rom)
    if actual_master_quest != master_quest:
        expected = "a Master Quest" if master_quest else "a Normal"
        actual = "Master Quest" if actual_master_quest else "Normal"
        raise RomProvisionError(
            f"{environment_name} must name {expected} OoT ROM, but this is {actual}: {rom}"
        )

    app_dir.mkdir(parents=True, exist_ok=True)
    if any((app_dir / archive).is_file() for archive in archive_names):
        return None

    predicate = (
        is_oot_master_quest_n64_rom if master_quest else is_oot_normal_n64_rom
    )
    if any(
        candidate.is_file() and predicate(candidate)
        for pattern in N64_ROM_PATTERNS
        for candidate in app_dir.glob(pattern)
    ):
        return None

    destination = app_dir / f"{destination_stem}{rom.suffix.lower()}"
    if destination.exists() or destination.is_symlink():
        if destination.is_symlink() and destination.resolve() == rom:
            return None
        raise RomProvisionError(
            f"refusing to replace existing extraction input: {destination}"
        )
    destination.symlink_to(rom)
    return destination


def provision_n64_extraction_rom(
    app_dir: Path, environment: Mapping[str, str]
) -> Path | None:
    """Expose missing Normal and MQ inputs without replacing archives or staged ROMs.

    The return value retains the legacy single-path contract: it is the first new link, while both
    requested editions are provisioned independently as side effects.
    """
    created = [
        link
        for link in (
            _provision_n64_extraction_rom(
                app_dir,
                environment,
                environment_name=OOT_NAME,
                archive_names=("oot.o2r", "oot.otr"),
                destination_stem="zelda3d-source",
                master_quest=False,
            ),
            _provision_n64_extraction_rom(
                app_dir,
                environment,
                environment_name=OOT_MQ_NAME,
                archive_names=("oot-mq.o2r", "oot-mq.otr"),
                destination_stem="zelda3d-source-mq",
                master_quest=True,
            ),
        )
        if link is not None
    ]
    return created[0] if created else None


def require_oot3d_rom(repo: Path, environment: Mapping[str, str]) -> Path:
    value = environment.get(OOT3D_NAME)
    if not value:
        raise RomProvisionError(
            "no OoT3D .3ds found — set ZELDA3D_OOT3D_ROM, add ./.env, "
            f"or drop a *.3ds into {repo}"
        )
    rom = Path(value).expanduser()
    if not rom.is_file():
        raise RomProvisionError(f"{OOT3D_NAME} does not name an existing file: {rom}")
    if not is_oot3d_rom(rom):
        raise RomProvisionError(
            f"{OOT3D_NAME} is not a recognized decrypted Ocarina of Time 3D ROM: {rom}"
        )
    return rom.resolve()


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--repo", type=Path, default=Path(__file__).resolve().parents[1]
    )
    parser.add_argument("--app-dir", type=Path)
    parser.add_argument(
        "--shell", action="store_true", help="print shell-safe resolved assignments"
    )
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        environment = resolve_rom_environment(args.repo)
        require_oot3d_rom(args.repo, environment)
        if args.app_dir is not None:
            provision_n64_extraction_rom(args.app_dir, environment)
    except RomProvisionError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    if args.shell:
        for name in (OOT3D_ROMFS_NAME, OOT3D_NAME, OOT_NAME, OOT_MQ_NAME):
            if environment.get(name):
                print(f"export {name}={shlex.quote(environment[name])}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
