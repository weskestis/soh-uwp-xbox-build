#!/usr/bin/env python3
"""Exercise dumpbin parsing and rejection of the actual desktop callback classes."""

import unittest

from uwp_core_imports import audit_imports, parse_imports


def report(*modules):
    output = "Microsoft (R) COFF/PE Dumper\n\n  Section contains the following imports:\n"
    for name, symbols in modules:
        output += f"\n    {name}\n             181D884C8 Import Address Table\n"
        output += "             18116A108 Import Name Table\n                     0 time date stamp\n"
        output += "                     0 Index of first forwarder reference\n\n"
        output += "".join(f"                          {i:X} {symbol}\n" for i, symbol in enumerate(symbols))
    return output + "\n  Summary\n\n       D87000 .text\n"


BASE = (
    ("SDL2.dll", ["SDL_GetBasePath", "SDL_free", "SDL_GetWindowWMInfo"]),
    ("KERNEL32.dll", ["LoadPackagedLibrary", "GetProcAddress"]),
    ("OPENGL32.dll", ["wglGetProcAddress"]),
)


class CoreImportTests(unittest.TestCase):
    def test_report_parses_addresses_hints_and_summary(self):
        parsed = parse_imports(report(*BASE).replace("\n", "\r\n"))
        self.assertEqual(set(parsed), {"sdl2.dll", "kernel32.dll", "opengl32.dll"})
        self.assertEqual(parsed["kernel32.dll"], {"LoadPackagedLibrary", "GetProcAddress"})

    def test_acceptance_is_explicitly_not_runtime_or_crt_proof(self):
        result = audit_imports(report(*BASE))
        self.assertTrue(result["known_desktop_callbacks_absent"])
        self.assertFalse(result["uwp_api_certified"])
        self.assertFalse(result["crt_compatibility_verified"])
        self.assertFalse(result["xbox_runtime_verified"])

    def test_supported_security_function_is_not_blanket_rejected(self):
        audit_imports(report(*BASE, ("ADVAPI32.dll", ["GetSecurityInfo"])))

    def test_desktop_dependency_names_are_case_insensitive(self):
        for name in ("USER32.dll", "shell32.dll", "IMM32.DLL", "dbghelp.dll", "SDL3.dll"):
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, "dependency"):
                audit_imports(report(*BASE, (name, ["SomeFunction"])))

    def test_callback_symbols_cannot_hide_behind_another_import_module(self):
        for symbol in ("SetWindowLongPtrA", "GetWindowLongW", "CallWindowProcW", "DefWindowProcA",
                       "MessageBoxA", "OpenClipboard", "SetClipboardData", "ShellExecuteW",
                       "ImmGetContext", "SymInitialize", "StackWalk64", "LoadLibraryA"):
            with self.subTest(symbol=symbol), self.assertRaisesRegex(ValueError, "desktop callback"):
                audit_imports(report(*BASE, ("other.dll", [symbol])))

    def test_missing_path_resolver_fails(self):
        with self.assertRaisesRegex(ValueError, "path resolver"):
            audit_imports(report(*BASE[1:]))

    def test_missing_packaged_loader_fails(self):
        with self.assertRaisesRegex(ValueError, "GL loader"):
            audit_imports(report(BASE[0], BASE[2]))

    def test_empty_truncated_and_dependents_reports_fail(self):
        # A truncated dump can have both required APIs before the prohibited
        # dependency is printed. It must never turn that prefix into a pass.
        truncated = report(*BASE, ("SHELL32.dll", ["ShellExecuteW"])).split("    SHELL32.dll")[0]
        for content in ("", "error: dumpbin failed", report(("SDL2.dll", [])), "    SDL2.dll\n",
                        truncated, report(*BASE).split("       D87000")[0]):
            with self.subTest(content=content), self.assertRaises(ValueError):
                audit_imports(content)

    def test_repeated_and_delay_import_sections_are_combined(self):
        content = report(*BASE) + "  Section contains the following delay load imports:\n"
        content += report(("SDL2.dll", ["SDL_Init"]), ("SHELL32.dll", ["ShellExecuteW"]))
        self.assertIn("SDL_GetBasePath", parse_imports(content)["sdl2.dll"])
        self.assertIn("SDL_Init", parse_imports(content)["sdl2.dll"])
        with self.assertRaisesRegex(ValueError, "shell32"):
            audit_imports(content)


if __name__ == "__main__":
    unittest.main()
