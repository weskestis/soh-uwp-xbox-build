#!/usr/bin/env python3
"""Generate the pinned Mesa import ABI and audit native Store build evidence."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import xml.etree.ElementTree as ET

from uwp_core_imports import audit_imports, parse_imports


# libzip links the public CNG provider directly. These APIs are in Microsoft's
# universal-device inventory (all moved to bcrypt.dll by Windows 10 14393):
# https://learn.microsoft.com/en-us/uwp/win32-and-com/win32-apis
CNG_IMPORTS = frozenset({
    "BCryptGetProperty", "BCryptSetProperty", "BCryptCloseAlgorithmProvider",
    "BCryptGenerateSymmetricKey", "BCryptEncrypt", "BCryptDestroyKey",
    "BCryptCreateHash", "BCryptHashData", "BCryptFinishHash", "BCryptDestroyHash",
    "BCryptGenRandom", "BCryptDeriveKeyPBKDF2", "BCryptOpenAlgorithmProvider",
})

# These CRT environment functions are explicitly unavailable to UWP. Match the
# symbols regardless of which API-set DLL exposes them; an api-ms-win-* name
# alone does not establish API compatibility. This is a focused regression gate.
# https://learn.microsoft.com/en-us/cpp/cppcx/crt-functions-not-supported-in-universal-windows-platform-apps
UNSUPPORTED_CRT_ENVIRONMENT = frozenset({
    "getenv", "getenv_s", "_wgetenv", "_wgetenv_s", "_dupenv_s", "_wdupenv_s",
    "putenv", "_putenv", "_putenv_s", "_wputenv", "_wputenv_s",
    "_environ", "_wenviron",
    "_searchenv", "_searchenv_s", "_wsearchenv", "_wsearchenv_s", "tzset",
})


def mesa_exports(report: str) -> set[str]:
    names = set()
    for line in report.splitlines():
        match = re.fullmatch(
            r"\s+\d+\s+[0-9A-Fa-f]+\s+(?:[0-9A-Fa-f]+\s+)?"
            r"((?:gl|wgl)[A-Za-z0-9_]+)(?:\s+=\s+\S+|\s+\(forwarded to [^)]+\))?\s*", line)
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
        if any(node.text != "false" for node in project.findall(
                "m:ItemDefinitionGroup/m:ClCompile/m:CompileAsWinRT", namespace)):
            raise ValueError(f"{name}: project configuration re-enables C++/CX")
        native_defaults = False
        metadata_disabled = False
        for item in project.findall(".//m:Import", namespace):
            path = item.attrib.get("Project", "")
            if path.replace("\\", "/").endswith("/UwpNativeCpp.props"):
                props = ET.parse(path).getroot()
                native_defaults = props.findtext(
                    "m:ItemDefinitionGroup/m:ClCompile/m:CompileAsWinRT", namespaces=namespace) == "false"
                metadata_disabled = props.findtext(
                    "m:ItemDefinitionGroup/m:Link/m:GenerateWindowsMetadata", namespaces=namespace) == "false"
        explicit_metadata = project.findall("m:ItemDefinitionGroup/m:Link/m:GenerateWindowsMetadata", namespace)
        if any(node.text != "false" for node in explicit_metadata) or not (metadata_disabled or explicit_metadata):
            raise ValueError(f"{name}: native library still expects WinRT metadata")
        for item in project.findall(".//m:ClCompile[@Include]", namespace):
            if Path(item.attrib["Include"]).suffix.lower() not in {".c", ".cc", ".cpp", ".cxx"}:
                continue
            winrt = item.findtext("m:CompileAsWinRT", namespaces=namespace)
            if winrt != "false" and not (winrt is None and native_defaults):
                raise ValueError(f"{name}: C++/CX remains enabled for {item.attrib['Include']}")
        if any(re.search(r"(?:^|\s)/(?:MTd?|ZW)(?:\s|$)", value or "")
               for value in values("AdditionalOptions")):
            raise ValueError(f"{name}: raw /MT or /ZW override")
    return len(paths)


def audit_core(directory: Path) -> dict:
    read = lambda name: (directory / name).read_text(encoding="utf-8-sig")
    regression = audit_imports(read("imports.txt"))
    imports = parse_imports(read("imports.txt"))
    unsupported = sorted(f"{module}!{symbol}" for module, symbols in imports.items()
                         for symbol in symbols & UNSUPPORTED_CRT_ENVIRONMENT)
    if unsupported:
        raise ValueError(f"Unsupported UWP CRT environment imports: {unsupported}")
    required = {"msvcp140_app.dll", "vcruntime140_app.dll"}
    if not required <= imports.keys():
        raise ValueError("Core does not import the dynamic Store C++ runtime")
    unexpected = {name for name in imports if not (
        name.startswith(("api-ms-win-", "ext-ms-win-")) or
        re.fullmatch(r"(?:msvcp|vcruntime)140(?:_\d+)?_app\.dll", name) or
        name in {"sdl2.dll", "opengl32.dll", "bcrypt.dll"})}
    if unexpected:
        raise ValueError(f"Unexpected non-Store core imports: {sorted(unexpected)}")
    if imports.get("bcrypt.dll", set()) - CNG_IMPORTS:
        raise ValueError("Core imports an unreviewed CNG function")
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
        "unsupported_crt_environment_absent": True,
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
