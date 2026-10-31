#!/usr/bin/env python3
"""Negative cases for Store CRT evidence; these fixtures are not native proof."""

from pathlib import Path
import tempfile
import unittest

from audit import audit


IMPORTS = """  Section contains the following imports:
    MSVCP140_APP.dll
        1 _SomeCppSymbol
    VCRUNTIME140_APP.dll
        2 _CxxThrowException
    api-ms-win-core-file-l1-2-0.dll
        3 CreateFile2
  Summary
        1000 .text
"""


class RuntimeProbeAuditTests(unittest.TestCase):
    def test_accepts_evidence_without_claiming_execution(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parents[2] / "scratch") as tmp:
            path = Path(tmp)
            self.populate(path)
            result = audit(path)
            self.assertTrue(result["store_crt_link_verified"])
            for key in ("windows_runtime_execution_verified", "xbox_runtime_verified", "game_dependencies_migrated"):
                self.assertFalse(result[key])

    @staticmethod
    def populate(path):
        for name, content in {
            "imports.txt": IMPORTS,
            "exports.txt": "1 0 1234 FusionUwpRuntimeProbe\n",
            "headers.txt": "            App Container\n",
            "build.log": "Searching C:\\VS\\lib\\x64\\store\\msvcprt.lib:\n",
        }.items():
            (path / name).write_text(content)

    def test_rejects_incomplete_or_desktop_evidence(self):
        cases = (
            ("imports.txt", IMPORTS.replace("MSVCP140_APP", "MSVCP140")),
            ("imports.txt", IMPORTS.replace("api-ms-win-core-file-l1-2-0", "USER32")),
            ("imports.txt", IMPORTS.split("  Summary")[0]),
            ("imports.txt", IMPORTS.replace("  Summary", "    api-ms-win-crt-environment-l1-1-0.dll\n        10 getenv\n  Summary")),
            ("exports.txt", "unrelated_export\n"),
            ("headers.txt", "DLL\n"),
            ("build.log", "Searching C:\\VS\\lib\\x64\\msvcprt.lib:\n"),
        )
        for name, content in cases:
            with self.subTest(file=name, content=content), tempfile.TemporaryDirectory(
                dir=Path(__file__).resolve().parents[2] / "scratch"
            ) as tmp:
                path = Path(tmp)
                self.populate(path)
                (path / name).write_text(content)
                with self.assertRaises(ValueError):
                    audit(path)

    def test_font_probe_requires_real_library_export_and_disabled_option(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parents[2] / "scratch") as tmp:
            path = Path(tmp)
            self.populate(path)
            files = {
                "exports.txt": "1 0 1234 FusionUwpRuntimeProbe\n2 1 1300 FusionUwpFreeTypeProbe\n",
                "build.log": "Searching C:/VS/lib/x64/store/msvcprt.lib:\n"
                             "Searching C:/vcpkg/installed/x64-uwp-static-md/lib/freetype.lib:\n",
                "freetype-options.h": "/* #undef FT_CONFIG_OPTION_ENVIRONMENT_PROPERTIES */\n",
            }
            for name, value in files.items():
                (path / name).write_text(value)
            result = audit(path, freetype=True)
            self.assertTrue(result["freetype_store_link_verified"])
            self.assertTrue(result["unsupported_crt_environment_absent"])
            self.assertFalse(result["windows_runtime_execution_verified"])
            cases = (
                ("exports.txt", files["exports.txt"].replace("FusionUwpFreeTypeProbe", "unused_entry")),
                ("build.log", files["build.log"].replace("x64-uwp-static-md", "x64-windows-static-md")),
                ("freetype-options.h", "#define FT_CONFIG_OPTION_ENVIRONMENT_PROPERTIES\n"),
                ("freetype-options.h", ""),
            )
            for name, value in cases:
                with self.subTest(name=name), self.assertRaises(ValueError):
                    (path / name).write_text(value)
                    audit(path, freetype=True)
                (path / name).write_text(files[name])


if __name__ == "__main__":
    unittest.main()
