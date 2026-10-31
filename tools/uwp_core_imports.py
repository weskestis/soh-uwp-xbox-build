#!/usr/bin/env python3
"""Reject known desktop callback imports in Fusion core dumpbin reports.

This is a regression gate, not UWP certification. In particular, it does not
validate the CRT, every Windows API, transitive dependencies, or Xbox execution.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re


BLOCKED_DLLS = frozenset({"user32.dll", "shell32.dll", "imm32.dll", "dbghelp.dll", "sdl3.dll"})
BLOCKED_SYMBOLS = re.compile(
    r"(?:GetWindowLong(?:Ptr)?[AW]|SetWindowLong(?:Ptr)?[AW]|"
    r"CallWindowProc[AW]|DefWindowProc[AW]|ShowWindow|MessageBox[AW]|"
    r"OpenClipboard|CloseClipboard|EmptyClipboard|GetClipboardData|SetClipboardData|"
    r"ShellExecute(?:Ex)?[AW]|Imm\w+|Sym\w+|StackWalk\w*|LoadLibrary[AW])$"
)
DLL_HEADER = re.compile(r"^\s{4}([^\s]+\.dll)\s*$", re.IGNORECASE)
NAMED_IMPORT = re.compile(r"^\s+[0-9a-fA-F]+\s+([A-Za-z_?@$][\w?@$]*)\s*$")


def parse_imports(report: str) -> dict[str, set[str]]:
    imports: dict[str, set[str]] = {}
    current = None
    summary = False
    summary_sections = 0
    for line in report.splitlines():
        if line.strip() == "Summary":
            current = None
            summary = True
            summary_sections = 0
        elif summary and re.fullmatch(r"\s+[0-9a-fA-F]+\s+\.[\w$]+\s*", line):
            summary_sections += 1
        module = DLL_HEADER.fullmatch(line)
        if module:
            summary = False
            current = module[1].lower()
            imports.setdefault(current, set())
        elif current:
            symbol = NAMED_IMPORT.fullmatch(line)
            if symbol:
                imports[current].add(symbol[1])
    if not imports or any(not names for names in imports.values()) or not summary or not summary_sections:
        raise ValueError("Expected complete dumpbin /imports, named imports, and section summary")
    return imports


def audit_imports(report: str) -> dict:
    imports = parse_imports(report)
    problems = []
    for module, symbols in sorted(imports.items()):
        if module in BLOCKED_DLLS:
            problems.append(f"desktop/unsupported dependency: {module}")
        for symbol in sorted(symbols):
            if BLOCKED_SYMBOLS.fullmatch(symbol):
                problems.append(f"desktop callback: {module}!{symbol}")
    if "SDL_GetBasePath" not in imports.get("sdl2.dll", set()):
        problems.append("SDL2 installed-package path resolver is missing")
    if not any("LoadPackagedLibrary" in symbols for symbols in imports.values()):
        problems.append("packaged GL loader is missing")
    if problems:
        raise ValueError("\n".join(problems))
    return {
        "known_desktop_callbacks_absent": True,
        "uwp_api_certified": False,
        "crt_compatibility_verified": False,
        "xbox_runtime_verified": False,
        "modules": {module: sorted(symbols) for module, symbols in sorted(imports.items())},
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("imports", type=Path, help="dumpbin /imports output (UTF-8)")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        result = audit_imports(args.imports.read_text(encoding="utf-8-sig"))
    except ValueError as error:
        parser.exit(1, f"Core import regression audit failed:\n{error}\n")
    if args.output:
        args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print("Core import regression audit PASS; full UWP/CRT/Xbox validation remains pending")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
