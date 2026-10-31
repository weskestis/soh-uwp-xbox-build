#!/usr/bin/env python3
"""Require evidence that the isolated native probe actually selected Store CRT."""

import argparse
import json
from pathlib import Path
import re
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from uwp_core_imports import parse_imports


def audit(directory: Path) -> dict:
    imports = parse_imports((directory / "imports.txt").read_text(encoding="utf-8-sig"))
    modules = set(imports)
    expected = {"msvcp140_app.dll", "vcruntime140_app.dll"}
    if not expected <= modules:
        raise ValueError(f"Store runtime imports missing: {sorted(expected - modules)}")
    unexpected = {name for name in modules if not (
        name.startswith("api-ms-win-") or name.startswith("ext-ms-win-") or
        re.fullmatch(r"(?:msvcp|vcruntime)140(?:_\d+)?_app\.dll", name))}
    if unexpected:
        raise ValueError(f"Unexpected non-Store import modules: {sorted(unexpected)}")
    exports = (directory / "exports.txt").read_text(encoding="utf-8-sig")
    if "FusionUwpRuntimeProbe" not in exports:
        raise ValueError("Probe C entry is missing")
    headers = (directory / "headers.txt").read_text(encoding="utf-8-sig")
    if "App Container" not in headers:
        raise ValueError("DLL lacks the AppContainer flag")
    build = (directory / "build.log").read_text(encoding="utf-8-sig")
    if not re.search(r"[\\/]store[\\/].*(?:msvcprt|msvcrt|vcruntime)\.lib", build, re.I):
        raise ValueError("Verbose link did not demonstrate Store CRT library selection")
    return {
        "store_crt_link_verified": True,
        "windows_runtime_execution_verified": False,
        "xbox_runtime_verified": False,
        "game_dependencies_migrated": False,
        "modules": sorted(modules),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("diagnostics", type=Path)
    args = parser.parse_args()
    try:
        result = audit(args.diagnostics)
    except ValueError as error:
        parser.exit(1, f"Runtime probe audit failed: {error}\n")
    (args.diagnostics / "audit.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print("Store CRT link verified for isolated DLL; game migration and Xbox execution remain pending")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
