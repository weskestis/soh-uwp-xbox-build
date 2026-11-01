#!/usr/bin/env python3
"""Exercise bootstrap selection and reject incorrect installer provenance."""
import copy
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET
import zipfile
import fusion_package_audit as audit
import fusion_package_inputs as inputs
import fusion_package_renderer as renderer


class PackageTests(unittest.TestCase):
    def setUp(self):
        self.scratch = inputs.ROOT / "scratch"
        self.scratch.mkdir(exist_ok=True)

    def test_renderer_assets_restore_the_actual_runtime_path(self):
        with tempfile.TemporaryDirectory(dir=self.scratch, prefix="fusion-shader-") as temp:
            root = Path(temp)
            for name in ("soh.o2r", "2ship.o2r"):
                old = root / ("base-" + name)
                fixed = root / name
                members = {"portVersion": b"\x00\x09\x00\x02\x00\x03\x00", "objects/retained": b"unchanged"}
                if name == "2ship.o2r":
                    members["shaders/opengl/default.shader.fs"] = b"legacy fragment template"
                    members["shaders/opengl/default.shader.vs"] = b"legacy vertex template"
                with zipfile.ZipFile(old, "w") as archive:
                    for key, value in members.items():
                        archive.writestr(key, value)
                before = old.read_bytes()
                with self.assertRaisesRegex(RuntimeError, "Missing or incompatible OpenGL resource"):
                    renderer.verify_renderer_archive(old)
                renderer.prepare_renderer_archive(old, fixed)
                self.assertEqual(old.read_bytes(), before)
                self.assertEqual(renderer.verify_renderer_archive(fixed)[renderer.SHADER], renderer.SHADER_SHA256)
                with zipfile.ZipFile(fixed) as archive:
                    self.assertEqual(set(archive.namelist()), set(members) | set(renderer.renderer_members()))
                    for key, value in members.items():
                        self.assertEqual(archive.read(key), value)
                    self.assertTrue(archive.read(renderer.SHADER).startswith(b"@prism("))
                with self.assertRaises(RuntimeError):
                    renderer.prepare_renderer_archive(fixed, root / "duplicate.o2r")
                with self.assertRaises(RuntimeError):
                    renderer.prepare_renderer_archive(old, old)

    def test_renderer_rejects_wrong_case_and_legacy_header(self):
        with tempfile.TemporaryDirectory(dir=self.scratch, prefix="fusion-shader-bad-") as temp:
            path = Path(temp) / "bad.o2r"
            members = renderer.renderer_members()
            for path_name, content in ((renderer.SHADER.upper(), members[renderer.SHADER]),
                                       (renderer.SHADER, bytes(64) + members[renderer.SHADER]),
                                       (renderer.SHADER, b"wrong template")):
                with zipfile.ZipFile(path, "w") as archive:
                    archive.writestr(path_name, content)
                    archive.writestr(renderer.NOTICE, members[renderer.NOTICE])
                with self.assertRaises(RuntimeError):
                    renderer.verify_renderer_archive(path)

    def test_real_startup_selection(self):
        with tempfile.TemporaryDirectory(dir=self.scratch, prefix="fusion-selection-") as temp:
            executable = Path(temp) / "startup-test"
            subprocess.run([os.environ.get("CXX", "c++"), "-std=c++20", "-Wall", "-Wextra", "-Werror",
                "-Iuwp", "-IShipwright/libultraship/include", "uwp/core_bootstrap.cpp",
                "uwp/startup_selection.cpp", "tools/uwp_startup_selection_test.cpp", "-o", str(executable)],
                cwd=inputs.ROOT, check=True)
            subprocess.run([str(executable), temp], check=True)

    def test_package_identity_and_dependency(self):
        text = (inputs.ROOT / "uwp/Package.appxmanifest.in").read_text()
        values = {"PACKAGE_NAME": audit.FUSION_PACKAGE_NAME, "PUBLISHER": audit.FUSION_PUBLISHER,
                  "VERSION": "1.0.7.1", "DISPLAY_NAME": "Zelda Fusion", "PUBLISHER_DISPLAY_NAME": "Wes Kestis"}
        for name, value in values.items():
            text = text.replace(f"@ZELDA3D_UWP_{name}@", value)
        manifest = ET.fromstring(text.replace('$targetnametoken$.exe', 'zelda-fusion.exe'))
        manifest.find("f:Identity", audit.NS).set("ProcessorArchitecture", "x64")
        audit.verify_manifest(manifest, "1.0.7.1")
        changes = [("f:Identity", "Name", "Shipwright"), ("f:Identity", "Publisher", "CN=Old"),
            ("f:Identity", "Version", "1.0.1.1"), ("f:Identity", "ProcessorArchitecture", "x86"),
            ("f:Applications/f:Application", "Executable", "old.exe"),
            ("f:Applications/f:Application", "EntryPoint", "windows.fullTrustApplication"),
            ("f:Dependencies/f:PackageDependency", "Name", "Microsoft.VCLibs.140.00.UWPDesktop"),
            ("f:Dependencies/f:PackageDependency", "MinVersion", "14.0.0.0")]
        for xpath, key, value in changes:
            changed = copy.deepcopy(manifest)
            changed.find(xpath, audit.NS).set(key, value)
            with self.assertRaises(RuntimeError):
                audit.verify_manifest(changed, "1.0.7.1")
        for version in ("1.0.70000.0", "1.0.-1.0", "1.0.1", "1.0.1.1.0"):
            with self.assertRaises(RuntimeError):
                audit.version_fields(version)
        changed = copy.deepcopy(manifest)
        dependencies = changed.find("f:Dependencies", audit.NS)
        dependencies[:] = list(reversed(list(dependencies)))
        with self.assertRaises(RuntimeError):
            audit.verify_manifest(changed, "1.0.7.1")

    def test_framework_variants(self):
        ns = audit.NS['f']
        manifest = ET.Element(f"{{{ns}}}Package")
        ET.SubElement(manifest, f"{{{ns}}}Identity", audit.FRAMEWORK)
        properties = ET.SubElement(manifest, f"{{{ns}}}Properties")
        ET.SubElement(properties, f"{{{ns}}}Framework").text = "true"
        audit.verify_framework_manifest(manifest)
        for key, value in (("Name", "Microsoft.VCLibs.140.00.Debug"), ("ProcessorArchitecture", "arm64"),
                           ("Publisher", "CN=Other"), ("Version", "14.0.0.0")):
            changed = copy.deepcopy(manifest)
            changed.find("f:Identity", audit.NS).set(key, value)
            with self.assertRaises(RuntimeError):
                audit.verify_framework_manifest(changed)
        manifest.find("f:Properties/f:Framework", audit.NS).text = "false"
        with self.assertRaises(RuntimeError):
            audit.verify_framework_manifest(manifest)

    def test_native_evidence_tamper(self):
        with tempfile.TemporaryDirectory(dir=self.scratch, prefix="fusion-provenance-") as temp:
            root = Path(temp)
            binary = root / "mm_core.dll"
            binary.write_bytes(b"native binary fixture")
            report = {key: True for key in ("known_desktop_callbacks_absent", "store_crt_link_verified",
                "mesa_import_abi_verified", "unsupported_crt_environment_absent")}
            (root / "store-audit.json").write_text(json.dumps(report))
            (root / "BUILD.txt").write_text("source_commit=expected\n")
            (root / "SHA256.txt").write_text(f"Hash : {inputs.sha256(binary)}\n")
            inputs.verify_core("mm", root, "expected")
            with self.assertRaises(RuntimeError):
                inputs.verify_core("mm", root, "wrong-source")
            for flag in report:
                (root / "store-audit.json").write_text(json.dumps({**report, flag: False}))
                with self.assertRaises(RuntimeError):
                    inputs.verify_core("mm", root, "expected")
            (root / "store-audit.json").write_text(json.dumps(report))
            binary.write_bytes(b"modified binary")
            with self.assertRaises(RuntimeError):
                inputs.verify_core("mm", root, "expected")


if __name__ == "__main__":
    unittest.main()
