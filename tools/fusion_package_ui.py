"""Stage and verify Fusion UI files independently of the pinned game archives."""
from __future__ import annotations

import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "Shipwright/libultraship/assets/rml"
FILES = ("fusion_launcher.rml", "fusion_menu.rml", "fusion.rcss")


def stage_ui(runtime: Path) -> dict[str, str]:
    target = runtime / "assets/rml"
    target.mkdir(parents=True, exist_ok=True)
    for name in FILES:
        (target / name).write_bytes((SOURCE / name).read_bytes())
    return verify_ui(runtime)


def verify_ui(runtime: Path) -> dict[str, str]:
    hashes = {}
    for name in FILES:
        relative = f"assets/rml/{name}"
        target = runtime / relative
        expected = (SOURCE / name).read_bytes()
        if not target.is_file() or target.read_bytes() != expected:
            raise RuntimeError(f"Missing or outdated Fusion UI: {relative}")
        hashes[relative] = hashlib.sha256(expected).hexdigest()
    return hashes
