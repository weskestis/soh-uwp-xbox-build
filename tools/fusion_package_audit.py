#!/usr/bin/env python3
"""Audit the exact Fusion payload, Store framework and native import closure."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import shutil
import xml.etree.ElementTree as ET
import zipfile

from fusion_package_inputs import DEPOT_DLLS, ROOT, json_read, require, sha256
from fusion_package_renderer import verify_renderer_archive
from fusion_package_ui import verify_ui
from uwp_runtime_stage import FUSION_PACKAGE_NAME, FUSION_PUBLISHER, PACKAGE_ICONS, audit_appx

NS = {"f": "http://schemas.microsoft.com/appx/manifest/foundation/windows10"}
FRAMEWORK = {
    "Name": "Microsoft.VCLibs.140.00", "Version": "14.0.33519.0", "ProcessorArchitecture": "x64",
    "Publisher": "CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US",
}
FRAMEWORK_HASH = "9c17b521f9d690a1f504da5108ed6eec5669eb3a8fd1331eef43e40d84e74283"
SYSTEM_MODULES = set("kernel32 kernelbase ntdll advapi32 bcrypt combase crypt32 d3d12 d3d11 dxgi "
    "dwmapi gdi32 imm32 mmdevapi ole32 oleaut32 powrprof propsys psapi rpcrt4 secur32 setupapi "
    "shcore shell32 shlwapi user32 userenv version winmm ws2_32 winhttp xinput1_4 xaudio2_9 mfplat "
    "mf mfuuid avrt cfgmgr32 normaliz wintrust hid win32u".split())


def version_fields(version: str):
    parts = version.split(".")
    require(len(parts) == 4 and all(p.isascii() and p.isdecimal() and int(p) <= 65535 for p in parts),
            "Invalid four-part package version")
    return tuple(int(p) for p in parts)


def verify_framework_manifest(manifest):
    identity = manifest.find("f:Identity", NS)
    require(identity is not None and all(identity.get(k) == v for k, v in FRAMEWORK.items()),
            "Wrong Store framework identity, architecture, publisher or version")
    require(manifest.findtext("f:Properties/f:Framework", namespaces=NS) == "true", "Package is not a framework")


def framework(package: Path):
    require(sha256(package) == FRAMEWORK_HASH, "Store framework differs from the audited retail x64 package")
    target = ROOT / "scratch/fusion-framework"
    target.mkdir(exist_ok=False)
    with zipfile.ZipFile(package) as archive:
        verify_framework_manifest(ET.fromstring(archive.read("AppxManifest.xml")))
        archive.extractall(target)
    print("Retail x64 Store framework identity and SHA-256 verified")


def verify_manifest(manifest, version: str):
    version_fields(version)
    identity = manifest.find("f:Identity", NS)
    require(identity is not None and identity.get("Name") == FUSION_PACKAGE_NAME and
            identity.get("Publisher") == FUSION_PUBLISHER and identity.get("Version") == version and
            identity.get("ProcessorArchitecture") == "x64", "Fusion package identity/version/x64 mismatch")
    app = manifest.find("f:Applications/f:Application", NS)
    require(app is not None and app.get("Id") == "ZeldaFusion" and
            app.get("EntryPoint") == "uwp.App" and app.get("Executable") == "zelda-fusion.exe",
            "Unexpected Fusion activation or executable")
    dependencies = manifest.findall("f:Dependencies/f:PackageDependency", NS)
    children = list(manifest.find("f:Dependencies", NS))
    require(children and children[0].tag == f"{{{NS['f']}}}TargetDeviceFamily",
            "Manifest schema requires TargetDeviceFamily before PackageDependency")
    require(len(dependencies) == 1, "Unexpected framework dependency set")
    dep = dependencies[0]
    require(dep.get("Name") == FRAMEWORK["Name"] and dep.get("Publisher") == FRAMEWORK["Publisher"] and
            dep.get("MinVersion") == FRAMEWORK["Version"], "Wrong Store framework dependency")
    capabilities = {e.get("Name") for e in manifest.iter() if e.tag.endswith("Capability")}
    require(not capabilities.intersection({"runFullTrust", "broadFileSystemAccess"}), "Desktop-only capability")


def select_package(build: Path, version: str):
    candidates = []
    for path in build.rglob("*"):
        if path.suffix.lower() not in {".appx", ".msix"} or "Dependencies" in path.parts:
            continue
        with zipfile.ZipFile(path) as archive:
            manifest = ET.fromstring(archive.read("AppxManifest.xml"))
        try:
            verify_manifest(manifest, version)
        except RuntimeError:
            continue
        candidates.append(path)
    require(bool(candidates), "No generated package matches the exact Fusion identity/version/x64")
    require(len({sha256(p) for p in candidates}) == 1, "Ambiguous generated packages with different bytes")
    print(candidates[0].resolve())


def payload_hashes():
    runtime = ROOT / "scratch/fusion-runtime"
    hashes = {p.relative_to(runtime).as_posix(): sha256(p) for p in runtime.rglob("*") if p.is_file()}
    for name in DEPOT_DLLS:
        hashes[name] = sha256(ROOT / "scratch/uwp-dep/x64/bin" / name)
    for name in PACKAGE_ICONS:
        hashes[name] = sha256(ROOT / "uwp" / name)
    return hashes


def native_closure(unpacked: Path):
    import pefile
    framework_root = ROOT / "scratch/fusion-framework"
    binaries = [*unpacked.glob("*.dll"), *unpacked.glob("*.exe"), *framework_root.rglob("*.dll")]
    inventories = {}
    pending = {p.name.lower() for p in binaries if p.parent == unpacked}
    for path in binaries:
        pe = pefile.PE(str(path))
        require(pe.FILE_HEADER.Machine == 0x8664, f"Non-x64 binary: {path.name}")
        exports = {}
        for symbol in getattr(getattr(pe, "DIRECTORY_ENTRY_EXPORT", None), "symbols", []):
            exports[symbol.ordinal] = symbol.forwarder.decode() if symbol.forwarder else None
            if symbol.name:
                exports[symbol.name.decode()] = exports[symbol.ordinal]
        imports = []
        for directory in ("DIRECTORY_ENTRY_IMPORT", "DIRECTORY_ENTRY_DELAY_IMPORT"):
            for module in getattr(pe, directory, []):
                for symbol in module.imports:
                    imports.append((module.dll.decode().lower(), symbol.name.decode() if symbol.name else symbol.ordinal))
        if path.name in {"zelda-fusion.exe", "soh_core.dll", "mm_core.dll"}:
            require(pe.OPTIONAL_HEADER.DllCharacteristics & 0x1000, f"Missing AppContainer flag: {path.name}")
            require(any(m.endswith("_app.dll") for m, _ in imports), f"Missing dynamic Store CRT: {path.name}")
            require(not {m for m, _ in imports}.intersection({"user32.dll", "shell32.dll", "imm32.dll", "dbghelp.dll"}),
                    f"Desktop callback dependency in {path.name}")
            require(not {s for _, s in imports}.intersection({"getenv", "_wgetenv", "_putenv", "setenv"}),
                    f"Unsupported CRT environment API in {path.name}")
        inventories[path.name.lower()] = {"exports": exports, "imports": imports, "sha256": sha256(path)}
        pe.close()
    system = set()
    resolved = 0

    def resolve(module, symbol, chain=()):
        nonlocal resolved
        if module not in inventories:
            require(module.startswith(("api-ms-", "ext-ms-")) or module.removesuffix(".dll") in SYSTEM_MODULES,
                    f"Missing packaged dependency: {module}")
            system.add(module)
            return
        key = (module, symbol)
        require(key not in chain, "Cyclic export forwarder")
        exports = inventories[module]["exports"]
        require(symbol in exports, f"Missing imported symbol: {module}!{symbol}")
        resolved += 1
        pending.add(module)
        forward = exports[symbol]
        if forward:
            target, entry = forward.rsplit(".", 1)
            target = target.lower()
            if not target.endswith(".dll"):
                target += ".dll"
            resolve(target, int(entry[1:]) if entry.startswith("#") else entry, (*chain, key))
    # Framework packages also contain unused C++ AMP/OpenMP libraries. Follow
    # imports from our actual payload instead of requiring their unrelated APIs.
    visited = set()
    while pending:
        name = pending.pop()
        if name in visited:
            continue
        visited.add(name)
        inventory = inventories[name]
        for module, symbol in inventory["imports"]:
            resolve(module, symbol)
    return {"packaged_symbol_references_verified": resolved, "system_modules_unverified_on_xbox": sorted(system),
            "dynamic_driver_loading_verified": False, "xbox_execution_verified": False,
            "binaries": {name: inventories[name]["sha256"] for name in sorted(visited)}}


def audit(unpacked: Path, version: str):
    audit_appx(unpacked, FUSION_PUBLISHER)
    verify_manifest(ET.parse(unpacked / "AppxManifest.xml").getroot(), version)
    renderer = {name: verify_renderer_archive(unpacked / name) for name in ("soh.o2r", "2ship.o2r")}
    ui = verify_ui(unpacked)
    expected = payload_hashes()
    for name in ("mm_core.dll", "2ship.o2r", "soh_core.dll", "soh.o2r"):
        require(name in expected, f"Missing expected Fusion payload: {name}")
    expected_folded = {}
    for name, digest in expected.items():
        require(name.casefold() not in expected_folded or expected_folded[name.casefold()] == digest,
                f"Conflicting case-insensitive payload path: {name}")
        expected_folded[name.casefold()] = digest
    actual = {}
    for file in unpacked.rglob("*"):
        if file.is_file():
            name = file.relative_to(unpacked).as_posix().casefold()
            require(name not in actual, f"Duplicate case-insensitive package path: {name}")
            actual[name] = file
    for name, digest in expected_folded.items():
        require(name in actual and sha256(actual[name]) == digest, f"Packaged payload mismatch: {name}")
    metadata = {"appxmanifest.xml", "appxblockmap.xml", "appxsignature.p7x", "[content_types].xml",
                "resources.pri", "appxmetadata/codeintegrity.cat"}
    for file in unpacked.rglob("*"):
        if not file.is_file():
            continue
        name = file.relative_to(unpacked).as_posix()
        require(name.casefold() in expected_folded or name.casefold() == "zelda-fusion.exe" or name.casefold() in metadata,
                f"Unexpected packaged file: {name}")
    report = native_closure(unpacked)
    report["package_version"] = version
    report["opengl_resources"] = renderer
    report["fusion_ui"] = ui
    report["payload_sha256"] = {**expected, "zelda-fusion.exe": sha256(unpacked / "zelda-fusion.exe")}
    target = ROOT / "scratch/fusion-package-audit.json"
    target.write_text(json.dumps(report, indent=2) + "\n")
    print(f"Fusion package audit passed: {len(expected)} exact payload files, {report['packaged_symbol_references_verified']} symbols")


def finish(dist: Path, version: str):
    record = json_read(ROOT / "scratch/fusion-package-inputs/BUILD.json")
    record["package_version"] = version
    record["framework"] = {**FRAMEWORK, "sha256": FRAMEWORK_HASH}
    record["files"] = {p.relative_to(dist).as_posix(): sha256(p) for p in dist.rglob("*") if p.is_file()}
    require(not any(Path(p).suffix.lower() in {".pfx", ".p12"} for p in record["files"]), "Private signing key in delivery")
    (dist / "BUILD.json").write_text(json.dumps(record, indent=2) + "\n")
    shutil.copy2(ROOT / "scratch/fusion-package-audit.json", dist / "PACKAGE-AUDIT.json")
    (dist / "SHA256SUMS.txt").write_text("".join(f"{sha256(p)}  {p.relative_to(dist).as_posix()}\n"
        for p in sorted(dist.rglob("*")) if p.is_file()))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("framework", "select", "audit", "finish"))
    parser.add_argument("path", type=Path)
    parser.add_argument("--version", default=os.environ.get("UWP_VERSION", ""))
    args = parser.parse_args()
    if args.command == "framework":
        framework(args.path)
    elif args.command == "select":
        select_package(args.path, args.version)
    elif args.command == "audit":
        audit(args.path, args.version)
    else:
        finish(args.path, args.version)


if __name__ == "__main__":
    main()
