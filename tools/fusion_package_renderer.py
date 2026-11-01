"""Restore the raw OpenGL shader required by Fusion's pinned SDL2 cores.

The retained port archives were exported from an SDL3 asset tree. Append the
matching renderer resource without rebuilding or changing any game asset.
"""
from __future__ import annotations

import hashlib
from pathlib import Path
import shutil
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "uwp/renderer-assets"
SHADER = "shaders/opengl/default.shader.glsl"
NOTICE = "shaders/opengl/LICENSE.txt"
SOURCE = "Kenix3/libultraship@7cfca877e64d70e19552eb47ab2b3540f53bbe9a"
SHADER_SHA256 = "6cf7e3ccb94b41f85c974b0caec38350ea10cc4667e367c95919072075b4ac5e"


def renderer_members() -> dict[str, bytes]:
    members = {name: (ASSETS / name).read_bytes() for name in (SHADER, NOTICE)}
    if hashlib.sha256(members[SHADER]).hexdigest() != SHADER_SHA256:
        raise RuntimeError("Fusion OpenGL template differs from its pinned source")
    return members


def verify_renderer_archive(path: Path) -> dict[str, str]:
    """Check the actual, case-sensitive resource path and byte-zero source format."""
    with zipfile.ZipFile(path) as archive:
        names = archive.namelist()
        if len(names) != len({name.casefold() for name in names}):
            raise RuntimeError(f"Ambiguous renderer archive members: {path.name}")
        for name, expected in renderer_members().items():
            if name not in names or archive.read(name) != expected:
                raise RuntimeError(f"Missing or incompatible OpenGL resource in {path.name}: {name}")
        return {name: hashlib.sha256(data).hexdigest() for name, data in renderer_members().items()}


def prepare_renderer_archive(source: Path, destination: Path) -> dict[str, str]:
    """Copy an audited base archive and append only the missing renderer files."""
    if source.resolve() == destination.resolve():
        raise RuntimeError("The retained port archive must remain unchanged")
    additions = renderer_members()
    with zipfile.ZipFile(source) as archive:
        original = archive.namelist()
        folded = {name.casefold() for name in original}
        if len(original) != len(folded) or archive.testzip() is not None:
            raise RuntimeError("Invalid retained port archive")
        if folded.intersection(name.casefold() for name in additions):
            raise RuntimeError("Retained port archive already contains renderer resources; review its provenance")
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="fusion-renderer-", dir=destination.parent) as temporary:
        candidate = Path(temporary) / destination.name
        shutil.copy2(source, candidate)
        with zipfile.ZipFile(candidate, "a", compression=zipfile.ZIP_DEFLATED) as archive:
            for name, data in additions.items():
                info = zipfile.ZipInfo(name, date_time=(2026, 4, 29, 0, 0, 0))
                info.compress_type = zipfile.ZIP_DEFLATED
                info.external_attr = 0o100644 << 16
                archive.writestr(info, data)
        result = verify_renderer_archive(candidate)
        with zipfile.ZipFile(source) as before, zipfile.ZipFile(candidate) as after:
            if set(after.namelist()) != set(original) | set(additions) or after.testzip() is not None:
                raise RuntimeError("Renderer repair changed the port archive inventory")
            for name in original:
                if before.read(name) != after.read(name):
                    raise RuntimeError(f"Renderer repair changed a retained game asset: {name}")
        candidate.replace(destination)
    return result
