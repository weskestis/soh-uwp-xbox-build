#!/usr/bin/env python3
"""Reject inconsistent Store projects/linkage and exercise the environment seam."""

import os
from pathlib import Path
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET

from test_uwp_core_imports import report
from uwp_store_build import audit_core, audit_projects, mesa_exports

ROOT = Path(__file__).resolve().parents[1]
MESA = """  ordinal hint RVA      name
        1    0 00001000 glGetString
        2    1          wglGetProcAddress = libgallium_wgl.wglGetProcAddress
        3    2 00001010 wglGetCurrentDC

  Summary

        1000 .text
"""
IMPORTS = report(
    ("SDL2.dll", ["SDL_GetBasePath"]),
    ("OPENGL32.dll", ["glGetString", "wglGetProcAddress"]),
    ("api-ms-win-core-libraryloader-l2-1-0.dll", ["LoadPackagedLibrary"]),
    ("MSVCP140_APP.dll", ["_Cnd_wait"]),
    ("VCRUNTIME140_APP.dll", ["memcpy"]),
)
NS = "http://schemas.microsoft.com/developer/msbuild/2003"
PROJECT = f"""<Project xmlns="{NS}">
  <PropertyGroup><WindowsTargetPlatformVersion>10.0.19041.0</WindowsTargetPlatformVersion>
    <AppContainerApplication>true</AppContainerApplication><ApplicationType>Windows Store</ApplicationType>
  </PropertyGroup>
  <ItemDefinitionGroup><ClCompile><RuntimeLibrary>MultiThreadedDLL</RuntimeLibrary>
    <AdditionalOptions>/EHsc</AdditionalOptions></ClCompile></ItemDefinitionGroup>
  <ItemGroup><ClCompile Include="core.cpp"><CompileAsWinRT>false</CompileAsWinRT></ClCompile></ItemGroup>
</Project>"""


class StoreBuildTests(unittest.TestCase):
    def setUp(self):
        (ROOT / "scratch").mkdir(exist_ok=True)
        self.work = tempfile.TemporaryDirectory(prefix="store-build-check-", dir=ROOT / "scratch")
        self.addCleanup(self.work.cleanup)
        self.path = Path(self.work.name)

    def test_mesa_names_exclude_forwarding_module(self):
        self.assertEqual(mesa_exports(MESA), {"glGetString", "wglGetProcAddress", "wglGetCurrentDC"})
        forwarded = MESA.replace("= libgallium_wgl.wglGetProcAddress",
                                 "(forwarded to libgallium_wgl.wglGetProcAddress)")
        self.assertEqual(mesa_exports(forwarded), mesa_exports(MESA))
        for invalid in (MESA.split("Summary")[0], MESA.replace("wglGetProcAddress", "badName"), ""):
            with self.assertRaises(ValueError):
                mesa_exports(invalid)

    def test_store_project_rejects_crt_cx_sdk_and_raw_overrides(self):
        project = self.path / "core.vcxproj"
        (self.path / "uwp-store-targets.txt").write_text(str(project) + "\n")
        project.write_text(PROJECT)
        self.assertEqual(audit_projects(self.path), 1)
        for old, new in (("MultiThreadedDLL", "MultiThreaded"),
                         ("CompileAsWinRT>false", "CompileAsWinRT>true"),
                         ("10.0.19041.0", "10.0.26100.0"),
                         ("/EHsc", "/EHsc /MT"), ("Windows Store", "Desktop")):
            with self.subTest(new=new), self.assertRaises(ValueError):
                project.write_text(PROJECT.replace(old, new))
                audit_projects(self.path)

    def test_generated_pch_requires_native_defaults_without_later_override(self):
        props = self.path / "UwpNativeCpp.props"
        props.write_bytes((ROOT / "cmake/UwpNativeCpp.props").read_bytes())
        project = self.path / "core.vcxproj"
        (self.path / "uwp-store-targets.txt").write_text(str(project) + "\n")
        xml = ET.fromstring(PROJECT)
        ET.SubElement(xml, f"{{{NS}}}Import", Project=str(props))
        group = ET.SubElement(xml, f"{{{NS}}}ItemGroup")
        ET.SubElement(group, f"{{{NS}}}ClCompile", Include="cmake_pch.hxx.cxx")
        ET.ElementTree(xml).write(project)
        self.assertEqual(audit_projects(self.path), 1)
        props.write_text(props.read_text().replace("false", "true"))
        with self.assertRaisesRegex(ValueError, r"C\+\+/CX"):
            audit_projects(self.path)
        props.write_text(props.read_text().replace("true", "false"))
        compile_group = xml.find(f"{{{NS}}}ItemDefinitionGroup/{{{NS}}}ClCompile")
        ET.SubElement(compile_group, f"{{{NS}}}CompileAsWinRT").text = "true"
        ET.ElementTree(xml).write(project)
        with self.assertRaisesRegex(ValueError, "re-enables"):
            audit_projects(self.path)

    def test_core_requires_independent_link_and_import_evidence(self):
        files = {"imports.txt": IMPORTS, "exports.txt": "1 0 1000 Zelda3D_CoreEntry",
                 "headers.txt": "App Container", "mesa-exports.txt": MESA,
                 "build.log": r"Loaded C:\MSVC\lib\x64\store\msvcprt.lib" +
                              "\nLoaded C:/depot/fusion-mesa-opengl32.lib"}
        for name, content in files.items():
            (self.path / name).write_text(content)
        result = audit_core(self.path)
        self.assertTrue(result["store_crt_link_verified"])
        self.assertFalse(result["xbox_runtime_verified"])
        self.assertFalse(result["shared_engine_handoff"])
        (self.path / "imports.txt").write_text(IMPORTS + report(("bcrypt.dll", ["BCryptGenRandom"])))
        self.assertTrue(audit_core(self.path)["store_crt_link_verified"])
        (self.path / "imports.txt").write_text(IMPORTS)
        cases = [("imports.txt", IMPORTS.replace("MSVCP140_APP.dll", "MSVCP140.dll")),
                 ("imports.txt", IMPORTS.replace("glGetString", "glAbsentFromMesa")),
                 ("imports.txt", IMPORTS + report(("KERNEL32.dll", ["GetModuleHandleExA"]))),
                 ("imports.txt", IMPORTS + report(("bcrypt.dll", ["UnreviewedApi"]))),
                 ("headers.txt", "DLL"), ("exports.txt", "OtherEntry"),
                 ("build.log", files["build.log"].replace("store", "desktop")),
                 ("build.log", files["build.log"].replace("fusion-mesa-opengl32.lib", "opengl32.lib")),
                 ("build.log", files["build.log"] + "\nLoaded libcmt.lib")]
        for name, invalid in cases:
            with self.subTest(name=name, invalid=invalid), self.assertRaises(ValueError):
                (self.path / name).write_text(invalid)
                audit_core(self.path)
            (self.path / name).write_text(files[name])

    def test_environment_defaults_and_desktop_reads_in_c_and_cpp(self):
        source = '''#include "ship/utils/Environment.h"
#include <string.h>
int main(void) {
    const char* value = ShipGetEnvironmentVariable("FUSION_TEST_ENV");
#ifdef ZELDA3D_UWP
    return value != NULL;
#else
    return value == NULL || strcmp(value, "desktop-value") != 0;
#endif
}
'''
        for extension, compiler in (("c", os.environ.get("CC", "cc")), ("cpp", os.environ.get("CXX", "c++"))):
            path = self.path / f"env.{extension}"
            path.write_text(source)
            for uwp in (False, True):
                executable = self.path / f"env-{extension}-{uwp}"
                command = [compiler, "-Wall", "-Wextra", "-Werror", str(path), "-o", str(executable),
                           f"-I{ROOT / 'Shipwright/libultraship/include'}"]
                if uwp:
                    command.append("-DZELDA3D_UWP=1")
                subprocess.run(command, check=True, capture_output=True)
                subprocess.run([str(executable)], check=True, env={**os.environ, "FUSION_TEST_ENV": "desktop-value"})


if __name__ == "__main__":
    unittest.main()
