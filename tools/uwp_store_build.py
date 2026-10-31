#!/usr/bin/env python3
"""Generate the pinned Mesa import ABI and audit native Store build evidence."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import xml.etree.ElementTree as ET

from uwp_core_imports import audit_imports, parse_imports


def mesa_exports(report: str) -> set[str]:
    names = set()
    for line in report.splitlines():
        match = re.fullmatch(
            r"\s+\d+\s+[0-9A-Fa-f]+\s+(?:[0-9A-Fa-f]+\s+)?"
            r"((?:gl|wgl)[A-Za-z0-9_]+)(?:\s+=\s+\S+)?\s*", line)
        if match:
            names.add(match[1])
    if not {"glGetString", "wglGetProcAddress", "wglGetCurrentDC"} <= names:
        raise ValueError("Mesa export report is missing the required GL/WGL ABI")
    if not re.search(r"\n\s+Summary\s*\n\s*\n\s+[0-9A-Fa-f]+\s+\.", report):
        raise ValueError("Mesa export report is truncated")
    return names


def audit_projects(build: Path) -> int:
    paths = (build / "uwp-store-targets.txt").read_text(encoding="utf-8").splitlines()
    if not paths:
        raise ValueError("No Store targets were recorded")
    namespace = {"m": "http://schemas.microsoft.com/developer/msbuild/2003"}
    for name in paths:
        project = ET.parse(name).getroot()
        def values(tag):
            return [node.text for node in project.findall(f".//m:{tag}", namespace)]
        if values("WindowsTargetPlatformVersion") != ["10.0.19041.0"]:
            raise ValueError(f"{name}: wrong or ambiguous Windows SDK")
        if values("AppContainerApplication") != ["true"] or values("ApplicationType") != ["Windows Store"]:
            raise ValueError(f"{name}: missing Store project properties")
        runtimes = values("RuntimeLibrary")
        if not runtimes or set(runtimes) - {"MultiThreadedDLL", "MultiThreadedDebugDLL"}:
            raise ValueError(f"{name}: not consistently compiled with /MD[d]")
        for item in project.findall(".//m:ClCompile[@Include]", namespace):
            if Path(item.attrib["Include"]).suffix.lower() not in {".c", ".cc", ".cpp", ".cxx"}:
                continue
            if item.findtext("m:CompileAsWinRT", namespaces=namespace) != "false":
                raise ValueError(f"{name}: C++/CX remains enabled for {item.attrib['Include']}")
        if any(re.search(r"(?:^|\s)/(?:MTd?|ZW)(?:\s|$)", value or "")
               for value in values("AdditionalOptions")):
            raise ValueError(f"{name}: raw /MT or /ZW override")
    return len(paths)


def audit_core(directory: Path) -> dict:
    read = lambda name: (directory / name).read_text(encoding="utf-8-sig")
    regression = audit_imports(read("imports.txt"))
    imports = parse_imports(read("imports.txt"))
    required = {"msvcp140_app.dll", "vcruntime140_app.dll"}
    if not required <= imports.keys():
        raise ValueError("Core does not import the dynamic Store C++ runtime")
    unexpected = {name for name in imports if not (
        name.startswith(("api-ms-win-", "ext-ms-win-")) or
        re.fullmatch(r"(?:msvcp|vcruntime)140(?:_\d+)?_app\.dll", name) or
        name in {"sdl2.dll", "opengl32.dll"})}
    if unexpected:
        raise ValueError(f"Unexpected non-Store core imports: {sorted(unexpected)}")
    if "App Container" not in read("headers.txt"):
        raise ValueError("Core lacks the PE AppContainer flag")
    if not re.search(r"\bZelda3D_CoreEntry\b", read("exports.txt")):
        raise ValueError("Core ABI entry is missing")
    build_log = read("build.log")
    if not re.search(r"[\\/]store[\\/].*(?:msvcprt|msvcrt|vcruntime)\.lib", build_log, re.I):
        raise ValueError("Link log does not demonstrate Store CRT selection")
    if re.search(r"(?:Loaded|Found).*\blibcmtd?\.lib\b", build_log, re.I):
        raise ValueError("Link log contains static CRT objects")
    if "fusion-mesa-opengl32.lib" not in build_log.lower():
        raise ValueError("Link log lacks the pinned Mesa import library")
    missing = imports.get("opengl32.dll", set()) - mesa_exports(read("mesa-exports.txt"))
    if missing:
        raise ValueError(f"Packaged Mesa does not export: {sorted(missing)}")
    return {
        **regression,
        "store_crt_link_verified": True,
        "mesa_import_abi_verified": True,
        "dependency_runtime_execution_verified": False,
        "shared_engine_handoff": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    mesa = commands.add_parser("mesa-def")
    mesa.add_argument("exports", type=Path)
    mesa.add_argument("output", type=Path)
    projects = commands.add_parser("projects")
    projects.add_argument("build", type=Path)
    core = commands.add_parser("core")
    core.add_argument("diagnostics", type=Path)
    args = parser.parse_args()
    try:
        if args.command == "mesa-def":
            names = mesa_exports(args.exports.read_text(encoding="utf-8-sig"))
            args.output.write_text("LIBRARY opengl32.dll\nEXPORTS\n" +
                                   "".join(f"    {name}\n" for name in sorted(names)), encoding="ascii")
            print(f"Generated Mesa import definition for {len(names)} verified exports")
        elif args.command == "projects":
            print(f"Store project audit PASS: {audit_projects(args.build)} targets")
        else:
            result = audit_core(args.diagnostics)
            (args.diagnostics / "store-audit.json").write_text(json.dumps(result, indent=2) + "\n")
            print("Store CRT and Mesa link audit PASS; Xbox execution remains unverified")
    except (ValueError, OSError, ET.ParseError) as error:
        parser.exit(1, f"Store build audit failed: {error}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
