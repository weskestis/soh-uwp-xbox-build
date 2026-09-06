#!/usr/bin/env python3
"""Host-verifiable contracts for the Android-only Xbox/UWP delivery path."""

from __future__ import annotations

import base64
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile
import xml.etree.ElementTree as ET
import zipfile

import uwp_runtime_stage as runtime_stage


ROOT = Path(__file__).resolve().parents[1]
UWP = ROOT / "uwp"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    tests = 0

    manifest_text = (UWP / "Package.appxmanifest.in").read_text(encoding="utf-8")
    manifest = ET.fromstring(manifest_text)
    ns = {"f": "http://schemas.microsoft.com/appx/manifest/foundation/windows10"}
    identity = manifest.find("f:Identity", ns)
    require(identity is not None, "manifest identity missing")
    require(identity.attrib["Name"] != "Shipwright", "old V3 package identity reused")
    require("@ZELDA3D_UWP_PACKAGE_NAME@" in identity.attrib["Name"], "identity is not configured")
    tests += 1

    capability_names = {
        element.attrib["Name"]
        for element in manifest.iter()
        if element.tag.endswith("Capability") and "Name" in element.attrib
    }
    require("internetClient" not in capability_names, "online capability added despite local-only scope")
    require("internetClientServer" not in capability_names, "server capability added despite local-only scope")
    require("privateNetworkClientServer" not in capability_names, "LAN capability added despite local-only scope")
    tests += 1

    cmake = (UWP / "CMakeLists.txt").read_text(encoding="utf-8")
    for token in (
        "ZELDA3D_UWP_ALLOW_INCOMPLETE_RENDERER",
        "ZELDA3D_UWP_REQUIRE_SIGNING",
        "ZELDA3D_UWP_SIGNING_CERTIFICATE",
        "soh.o2r",
        "runtimeobject.lib",
        'target_link_options(zelda3d-uwp PRIVATE "/WINMD:NO")',
        "SDL_WinRTRunApp",
    ):
        if token == "SDL_WinRTRunApp":
            require(token in (UWP / "main.cpp").read_text(encoding="utf-8"), f"missing {token}")
        else:
            require(token in cmake, f"missing {token}")
    for forbidden in (
        "ZELDA3D_UWP_PRIVATE_DATA_DIR",
        "ZELDA3D_UWP_OOT3D_SOURCE",
        'set(private_o2rs',
        'zelda3d_uwp_deploy("${runtime_file}" ".")',
        'zelda3d_uwp_deploy("${ZELDA3D_UWP_RUNTIME_DIR}/soh.o2r" ".")',
        'zelda3d_uwp_deploy("${ZELDA3D_UWP_RUNTIME_DIR}/oot.o2r"',
        'zelda3d_uwp_deploy("${ZELDA3D_UWP_RUNTIME_DIR}/oot-mq.o2r"',
    ):
        require(forbidden not in cmake, f"private input remains in AppX graph: {forbidden}")
    require(
        'if(NOT destination STREQUAL "")' in cmake,
        "root AppX payloads can still emit an invalid dot directory",
    )
    require("soh_core.lib" not in cmake, "wrapper still has a loader-time import of the game core")
    boot_diagnostics = (UWP / "boot_diagnostics.cpp").read_text(encoding="utf-8")
    for token in (
        'SDL_GetPrefPath(nullptr, "soh")',
        'kLogName[] = "uwp-boot.log"',
        'LoadPackagedLibrary(L"soh_core.dll", 0)',
        "GetProcAddress(coreModule, ZELDA3D_CORE_ENTRY_SYMBOL)",
        'runtime.module=%s state=failed win32=%lu',
        'L"opengl32.dll"',
        "__try",
        "GetExceptionInformation()",
        "std::fflush(file)",
    ):
        require(token in boot_diagnostics, f"durable Xbox boot diagnostic missing: {token}")
    main_cpp = (UWP / "main.cpp").read_text(encoding="utf-8")
    require(
        main_cpp.index("    Zelda3DUwp_BootLogStart();")
        < main_cpp.index("    uwp_GetWindowReference();"),
        "boot report no longer starts before the window/core boundary",
    )
    tests += 1

    forbidden_suffixes = {".3ds", ".cia", ".z64", ".n64", ".v64", ".o2r", ".otr", ".pfx", ".p12"}
    committed_private = [
        path
        for path in UWP.rglob("*")
        if path.is_file() and path.suffix.lower() in forbidden_suffixes
    ]
    require(not committed_private, f"private package data committed: {committed_private}")
    tests += 1

    workflow = (ROOT / ".github/workflows/build-xbox-uwp.yml").read_text(encoding="utf-8")
    for token in (
        "f5dbd58ee06a4b439bf260d641585dbc8bef3b86",
        "9e593bb18ea69cc5095e012465dcd675a822ed0d",
        "zipcmp zipmerge ziptool",
        '"-DCMAKE_SYSTEM_VERSION=10.0.19041.0"',
        "--triplet x64-windows-static",
        "VCPKG_TARGET_TRIPLET=x64-windows-static",
        "ZELDA3D_UWP_CORE=ON",
        "libogg libvorbis opus opusfile zlib bzip2 glew",
        "Configure Windows core DLL",
        "ZELDA3D_ENABLE_ROM_EXTRACTION=OFF",
        "$signTool verify",
        "Get-AuthenticodeSignature",
        "SignerCertificate.Thumbprint",
        '$signatureStatus -notin @("Valid", "NotTrusted", "UnknownError")',
        "[regex]::Matches($verifyText, '(?i)SignTool Error:')",
        "$verifyComparable = $verifyText -replace '\\s+', ' '",
        "verifyErrors.Count -ne 1",
        "root certificate.+not trusted",
        "Export-Certificate",
        "Remove-Item $pfx",
        "SOH-CURSOR-FPS-V3-OOT3D-FULL-XBOX",
        "PRIVATE-DATA-NOT-INCLUDED.txt",
        "Publish core build diagnostics",
        "Publish wrapper build diagnostics",
        "Tee-Object -FilePath",
        "::error title=Windows core diagnostic",
        "::error title=UWP package diagnostic",
        '".appx", ".msix"',
        "Visual Studio produced no AppX/MSIX",
        "SOH-CURSOR-FPS-V3-OOT3D-FULL-x64$packageExtension",
    ):
        require(token in workflow, f"remote build contract missing: {token}")
    require(
        '"-DCMAKE_SYSTEM_NAME=WindowsStore"' not in workflow,
        "the full core is still configured as WindowsStore instead of a normal Windows DLL",
    )
    require("--allow-unsupported" not in workflow, "supported Windows dependencies still use the UWP bypass")
    require("-DSTORM_USE_BUNDLED_LIBRARIES" not in workflow, "UWP core still configures desktop StormLib")
    require(
        workflow.count("Shipwright/libultraship/extern/StormLib") == 1,
        "StormLib checkout must remain host-only for soh.o2r generation",
    )
    require("libultraship.dll" not in workflow, "static UWP engine is still staged as a second DLL")
    require("secrets." not in workflow, "phone-only workflow unexpectedly requires a repository secret")
    require("*.pfx,*.p12" in workflow, "artifact private-key rejection is missing")
    require("Import-Certificate" not in workflow, "CI still mutates a trusted certificate store")
    require(
        "StatusMessage -match" not in workflow,
        "workflow still couples SignTool trust detection to provider-localized status text",
    )
    zapdtr_bundle = base64.b64decode(
        "".join(
            (ROOT / "cmake/Zelda3DZAPDTR-5f37af8.bundle.b64")
            .read_text(encoding="ascii")
            .splitlines()
        )
    )
    require(
        hashlib.sha256(zapdtr_bundle).hexdigest()
        == "f2be655739afe616fd56fb4ac88ee0f4309beb9d7da54fe5345e1d6dcdd1adb6",
        "recoverable ZAPDTR commit bundle changed",
    )
    require(
        "git submodule update --init --depth 1 Shipwright/ZAPDTR" not in workflow,
        "workflow still directly fetches the unpublished ZAPDTR gitlink",
    )
    for token in (
        "7d1bdbc4e582ef48c34b28d5cea9602040d0971e",
        "5f37af814f4c558d20de3c21d5e336d8d1cc99bf",
        "Zelda3DZAPDTR-5f37af8.bundle.b64",
    ):
        require(token in workflow, f"recoverable ZAPDTR checkout missing: {token}")
    tests += 1

    # The first successful PE/COFF link proved that Shipwright's Visual Studio property sheet
    # writes soh_core beside the source tree, not below CMAKE_BINARY_DIR. Keep that exact layout
    # and the expensive-core/package retry boundary executable in CI rather than rediscovering it
    # after another hour-long link.
    default_cxx = (ROOT / "Shipwright/CMake/DefaultCXX.cmake").read_text(encoding="utf-8")
    require(
        '"${CMAKE_SOURCE_DIR}$<$<NOT:$<STREQUAL:${CMAKE_VS_PLATFORM_NAME},Win32>>:'
        '/${CMAKE_VS_PLATFORM_NAME}>/${PROPS_CONFIG}"' in default_cxx,
        "Visual Studio output layout contract changed",
    )
    for token in (
        "windows-core:",
        "windows-package:",
        "needs: [host-assets, windows-core]",
        "actions: read",
        "CORE_BUILD_KEY: soh-core-a00a4d86-f5dbd58e-9e593bb1-v1",
        "CORE_RUNTIME_ARTIFACT: INTERNAL-windows-core-runtime-a00a4d86-v1",
        "LEGACY_CORE_RUNTIME_ARTIFACT: INTERNAL-windows-core-runtime",
        "uses: actions/cache@v4",
        "Find latest preserved Windows core",
        "Download latest preserved Windows core",
        "Validate reusable Windows core",
        "steps.reusable-core.outputs.ready != 'true'",
        "tools/uwp_runtime_stage.py stage-core",
        "tools/uwp_runtime_stage.py verify-core",
        "--source-root .",
        "--archive scratch/windows-core-runtime.zip",
        "tools/uwp_runtime_stage.py assemble",
        "tools/uwp_runtime_stage.py audit-appx",
        "Dependency AppX signature verification failed",
        "Preserve Windows core for package retries",
        "Assemble and validate wrapper runtime",
        "/p:GenerateAppxPackageOnBuild=true",
        "^\\d+\\.\\d+\\.\\d+\\.\\d+$",
        'New-Item -ItemType Directory -Force "$dist\\ThirdPartyNotices"',
        '"source_commit=$sourceCommit"',
        "[IO.Path]::GetRelativePath",
        '"workflow_attempt=${{ github.run_attempt }}"',
    ):
        require(token in workflow, f"hardened package retry contract missing: {token}")
    require("Copy-UniqueBuildFile" not in workflow, "workflow restored the incorrect recursive build-tree search")
    mobile_helper = (ROOT / "tools/ci/uwp_windows.ps1").read_text(encoding="utf-8")
    for token in (
        '".appx", ".msix"',
        "Visual Studio produced no AppX/MSIX",
        "SOH-CURSOR-FPS-V3-OOT3D-FULL-x64$packageExtension",
        '".appx", ".msix", ".cer"',
        "Assert-EphemeralPackageSignature",
        "Get-AuthenticodeSignature",
        "SignerCertificate.Thumbprint",
        '$status -notin @("Valid", "NotTrusted", "UnknownError")',
        "[regex]::Matches($verifyText, '(?i)SignTool Error:')",
        "$verifyComparable = $verifyText -replace '\\s+', ' '",
        "verifyErrors.Count -ne 1",
        "root certificate.+not trusted",
    ):
        require(token in mobile_helper, f"mobile MSIX packaging contract missing: {token}")
    require("Import-Certificate" not in mobile_helper, "mobile helper still mutates a trusted certificate store")
    require(
        "StatusMessage -match" not in mobile_helper,
        "mobile helper still couples SignTool trust detection to provider-localized status text",
    )
    require(workflow.count("overwrite: true") == 3, "workflow artifacts are not safe to replace on reruns")
    package_job = workflow[workflow.index("  windows-package:") :]
    require("Check out pinned vcpkg" not in package_job, "package retry still reinstalls the core toolchain")
    require("Compile Windows core DLL" not in package_job, "package retry still recompiles the expensive core")
    tests += 1

    with tempfile.TemporaryDirectory(prefix="zelda3d-uwp-runtime-") as temp:
        fixture = Path(temp)
        source_root = fixture / "source"
        core_output = source_root / "x64" / "Release"
        for relative in (*runtime_stage.CORE_FILES, *runtime_stage.ASSET_SENTINELS):
            output = core_output / relative
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_bytes(f"fixture:{relative}".encode())

        core_stage = fixture / "core-stage"
        core_archive = fixture / "core-runtime.zip"
        runtime_stage.stage_core(source_root, core_stage, core_archive)
        require(core_archive.is_file(), "host-testable core staging did not create its archive")
        runtime_stage.verify_core_archive(core_archive)

        unsafe_archive = fixture / "unsafe-core-runtime.zip"
        with zipfile.ZipFile(unsafe_archive, mode="w") as output:
            for relative in (*runtime_stage.CORE_FILES, *runtime_stage.ASSET_SENTINELS):
                output.writestr(relative, f"fixture:{relative}")
            output.writestr("../escaped.dll", b"must-be-rejected")
        try:
            runtime_stage.verify_core_archive(unsafe_archive)
        except RuntimeError:
            pass
        else:
            raise AssertionError("core archive verification accepted traversal")

        port_archive = fixture / "soh.o2r"
        port_archive.write_bytes(b"redistributable-port-archive")
        deps_root = fixture / "uwp-dep"
        for relative in runtime_stage.UWP_DEPENDENCIES:
            dependency = deps_root / relative
            dependency.parent.mkdir(parents=True, exist_ok=True)
            dependency.write_bytes(f"fixture:{relative}".encode())
        assembled = fixture / "assembled-runtime"
        runtime_stage.assemble_runtime(core_archive, port_archive, deps_root, assembled)
        require((assembled / "soh_core.dll").is_file(), "assembled runtime lost the core DLL")

        unpacked = fixture / "unpacked-appx"
        for relative in runtime_stage.PACKAGED_RUNTIME_FILES:
            packaged = unpacked / relative
            packaged.parent.mkdir(parents=True, exist_ok=True)
            packaged.write_bytes(f"fixture:{relative}".encode())
        for relative in runtime_stage.PACKAGE_ICONS:
            packaged = unpacked / relative
            packaged.parent.mkdir(parents=True, exist_ok=True)
            packaged.write_bytes((UWP / relative).read_bytes())
        (unpacked / "AppxManifest.xml").write_text(
            '<?xml version="1.0" encoding="utf-8"?>\n'
            '<Package xmlns="http://schemas.microsoft.com/appx/manifest/foundation/windows10">\n'
            '  <Identity Name="WesKestis.SOHCursorFPSV3.OoT3DFull" '
            'Publisher="CN=SOHCursorFPSV3OoT3DFull" Version="1.0.1.1" />\n'
            '</Package>\n',
            encoding="utf-8",
        )
        runtime_stage.audit_appx(unpacked, "CN=SOHCursorFPSV3OoT3DFull")

        icon = unpacked / "Assets/Square150x150Logo.scale-200.png"
        approved_icon = icon.read_bytes()
        icon.write_bytes(b"stale-stock-icon")
        try:
            runtime_stage.audit_appx(unpacked, "CN=SOHCursorFPSV3OoT3DFull")
        except RuntimeError:
            pass
        else:
            raise AssertionError("AppX audit accepted an unapproved package icon")
        icon.write_bytes(approved_icon)

        private_archive = unpacked / "oot.o2r"
        private_archive.write_bytes(b"must-be-rejected")
        try:
            runtime_stage.audit_appx(unpacked, "CN=SOHCursorFPSV3OoT3DFull")
        except RuntimeError:
            pass
        else:
            raise AssertionError("AppX audit accepted a private owner archive")
    tests += 1

    install_text = (UWP / "ANDROID-INSTALL.txt").read_text(encoding="utf-8")
    private_notice = (UWP / "PRIVATE-DATA-NOT-INCLUDED.txt").read_text(encoding="utf-8")
    for token in (
        "Android",
        "Xbox Device Portal",
        "LocalState\\soh",
        "oot.o2r",
        "oot-mq.o2r",
        "oot3d.3ds",
    ):
        require(token in install_text, f"Android install guide missing: {token}")
        if token.startswith("oot") or token.startswith("LocalState"):
            require(token in private_notice, f"private-data notice missing: {token}")
    require("command line" in install_text, "guide does not explicitly preserve phone-only setup")
    tests += 1

    root_cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    for token in (
        'option(ZELDA3D_UWP_CORE',
        "if(ZELDA3D_UWP_CORE)",
        'ZELDA3D_UWP_CORE requires a normal Windows toolchain',
        'Do not compile the full core as WindowsStore',
        "ZELDA3D_SDL2_OPENGL ON",
        "ZELDA3D_ENABLE_ROM_EXTRACTION OFF",
        "NON_PORTABLE ON",
        "set(INCLUDE_MPQ_SUPPORT OFF)",
        "add_compile_definitions(ZELDA3D_UWP=1 _UWP=1 _CRT_SECURE_NO_WARNINGS NOMINMAX)",
        "cmake/Zelda3DUwpDeps.cmake",
    ):
        require(token in root_cmake, f"UWP core boundary missing: {token}")
    require(
        "if(INCLUDE_MPQ_SUPPORT)\n    target_compile_definitions(libultraship PUBLIC INCLUDE_MPQ_SUPPORT)" in root_cmake,
        "legacy MPQ support is still forced into the UWP core",
    )
    soh_cmake = (ROOT / "Shipwright/soh/CMakeLists.txt").read_text(encoding="utf-8")
    for token in (
        "if(ZELDA3D_UWP_CORE)",
        "add_library(soh_lib OBJECT ${ALL_FILES})",
        'soh_lib_type STREQUAL "OBJECT_LIBRARY"',
        "a static archive exceeds 4 GiB",
        "SOH_MSVC_DEBUG_INFORMATION_FORMAT",
        "/O2;",
    ):
        require(token in soh_cmake, f"oversized Xbox core archive regression: {token}")
    require(
        soh_cmake.index("add_library(soh_lib OBJECT ${ALL_FILES})")
        < soh_cmake.index("add_library(soh_lib STATIC ${ALL_FILES})"),
        "Xbox object-library branch no longer precedes the desktop static fallback",
    )
    tests += 1
    lus_cmake = (ROOT / "Shipwright/libultraship/src/CMakeLists.txt").read_text(encoding="utf-8")
    require("if(ZELDA3D_UWP)" in lus_cmake, "UWP engine linkage mode is not explicit")
    require("add_library(libultraship STATIC)" in lus_cmake, "UWP engine is not folded into the core DLL")
    require("GLEW::GLEW" in lus_cmake, "Windows OpenGL extension loader is not linked")
    require("libultraship.dll" not in cmake, "UWP wrapper still requires a separate engine DLL")
    tests += 1

    ship_cmake = (ROOT / "Shipwright/libultraship/src/ship/CMakeLists.txt").read_text(encoding="utf-8")
    audio_cpp = (ROOT / "Shipwright/libultraship/src/ship/audio/Audio.cpp").read_text(encoding="utf-8")
    audio_header = (ROOT / "Shipwright/libultraship/include/ship/audio/AudioPlayer.h").read_text(encoding="utf-8")
    require(
        'NOT CMAKE_SYSTEM_NAME STREQUAL "Windows" OR ZELDA3D_UWP' in ship_cmake,
        "UWP core still compiles the desktop WASAPI implementation",
    )
    require(
        audio_cpp.count("defined(_WIN32) && !defined(ZELDA3D_UWP)") >= 3,
        "UWP audio selection still exposes desktop WASAPI",
    )
    require(
        "defined(_WIN32) && !defined(ZELDA3D_UWP)" in audio_header,
        "UWP audio header still imports desktop WASAPI",
    )
    require(
        'SetString("Window.AudioBackend", "sdl")' in audio_cpp,
        "UWP audio does not migrate a saved desktop backend to SDL",
    )
    tests += 1

    dependency_cmake = (
        ROOT / "Shipwright/libultraship/cmake/dependencies/common.cmake"
    ).read_text(encoding="utf-8")
    imgui_uwp_patch = (
        ROOT
        / "Shipwright/libultraship/cmake/dependencies/patches/imgui-opengl-loader-uwp.patch"
    ).read_text(encoding="utf-8")
    require("if(ZELDA3D_UWP)" in dependency_cmake, "ImGui loader patch is not scoped to UWP")
    require("imgui-opengl-loader-uwp.patch" in dependency_cmake, "ImGui UWP loader patch is not applied")
    require("LoadPackagedLibrary" in imgui_uwp_patch, "ImGui still uses a desktop DLL loader in UWP")
    require("GLAD_PLATFORM_UWP=1" in lus_cmake, "RmlUi GL loader does not select its UWP branch")
    tests += 1

    cityhash = (ROOT / "Shipwright/cmb3d/asset/cityhash.cpp").read_text(encoding="utf-8")
    require("ByteSwap64" in cityhash, "portable CityHash byte swap is missing")
    require("__builtin_bswap64" not in cityhash, "GCC-only byte swap remains in MSVC source")
    tests += 1

    frame_timing = (
        ROOT / "Shipwright/soh/soh/host/frame_timing.cpp"
    ).read_text(encoding="utf-8")
    require(
        "std::chrono::steady_clock" in frame_timing,
        "present FPS timing does not use the portable monotonic clock",
    )
    require("clock_gettime" not in frame_timing, "POSIX-only clock_gettime remains in MSVC source")
    require("CLOCK_MONOTONIC" not in frame_timing, "POSIX-only clock constant remains in MSVC source")
    tests += 1

    repl_fps = (
        ROOT / "Shipwright/soh/src/zelda3d/repl/repl_fps.cpp"
    ).read_text(encoding="utf-8")
    require(
        "std::chrono::steady_clock" in repl_fps,
        "REPL FPS timing does not use the portable monotonic clock",
    )
    require("clock_gettime" not in repl_fps, "POSIX-only clock_gettime remains in REPL FPS source")
    require("CLOCK_MONOTONIC" not in repl_fps, "POSIX-only clock constant remains in REPL FPS source")
    repl_transport = (
        ROOT / "Shipwright/soh/src/zelda3d/repl/repl_transport.cpp"
    ).read_text(encoding="utf-8")
    require("#ifdef _WIN32" in repl_transport, "Windows REPL transport boundary is missing")
    require(
        repl_transport.index("#ifdef _WIN32") < repl_transport.index("#include <unistd.h>"),
        "POSIX REPL transport headers are still imported before the Windows boundary",
    )
    require(
        "void PollTransport(PlayState*)" in repl_transport,
        "Windows core does not retain the no-op REPL transport ABI",
    )
    tests += 1

    hud_tex = (
        ROOT / "Shipwright/soh/src/zelda3d/hud/zelda3d_hud_tex.cpp"
    ).read_text(encoding="utf-8")
    require(
        hud_tex.count('extern "C" {') == 1,
        "HUD texture C linkage still wraps C++ implementation helpers",
    )
    require(
        max(hud_tex.index(helper) for helper in ("cropAndBoxDownsample", "stretchCap", "heartPackVariant"))
        < hud_tex.index('extern "C" {'),
        "HUD texture C++ helpers remain inside a broad C-linkage block",
    )
    for export in (
        "Zelda3D_XboxGlyphTex",
        "Zelda3D_KeyCapTex",
        "Zelda3D_KeyCapAlphabet",
        "Zelda3D_HeartTex",
        "Zelda3D_ButtonBgTex",
        "Zelda3D_CounterIconTex",
        "Zelda3D_DigitTex",
    ):
        require(f'extern "C" const' in hud_tex[hud_tex.rfind("\n", 0, hud_tex.index(export)) + 1 : hud_tex.index(export)],
                f"HUD export lost C ABI: {export}")
    require(
        'extern "C" int Zelda3D_HudTexEnabled' in hud_tex,
        "HUD enabled query lost its C ABI",
    )
    tests += 1

    # The first direct object-to-DLL remote link exposed nine unresolved symbols. Five Cucco
    # globals and the selected Z-target had definitions, but their C++ users had not included the
    # headers that establish C linkage. The logger also used two POSIX-only CRT names, while the
    # optional harness hook relied on ELF weak-undefined behavior that PE/COFF cannot represent.
    cucco_override = (
        ROOT / "Shipwright/soh/src/zelda3d/behaviors/actor/cucco_wing_override.cpp"
    ).read_text(encoding="utf-8")
    require('#include "cucco_control.h"' in cucco_override, "Cucco globals lost their C ABI declaration")
    require(
        cucco_override.index('#include "cucco_control.h"')
        < cucco_override.index("int gZelda3dForceCuccoAgitate"),
        "Cucco C ABI declaration no longer precedes its definitions",
    )
    input_cpp = (
        ROOT / "Shipwright/soh/src/zelda3d/input/zelda3d_input.cpp"
    ).read_text(encoding="utf-8")
    require(
        '#include "../diagnostics/actor_selection.h"' in input_cpp,
        "Z-target consumer no longer includes its C ABI owner",
    )
    require(
        "extern Actor* gZelda3dZTargetActor" not in input_cpp,
        "Z-target consumer restored a C++-linkage shadow declaration",
    )
    logger = (
        ROOT / "Shipwright/soh/src/zelda3d/core/zelda3d_log.c"
    ).read_text(encoding="utf-8")
    require("asciiCaseCompareN" in logger and "asciiCaseCompare" in logger, "portable ASCII logger comparison missing")
    require("strncasecmp(" not in logger and "strcasecmp(" not in logger, "POSIX-only logger comparison restored")
    graph = (ROOT / "Shipwright/soh/src/code/graph.c").read_text(encoding="utf-8")
    require(
        "#if !defined(_MSC_VER)\n    { extern int SohState_ApplyInputOverride" in graph,
        "MSVC still references the ELF-only weak harness hook",
    )
    tests += 1

    compiler = os.environ.get("CXX", "c++")
    with tempfile.TemporaryDirectory(prefix="zelda3d-uwp-bootstrap-") as temp:
        executable = Path(temp) / "uwp_core_bootstrap_test"
        subprocess.run(
            [
                compiler,
                "-std=c++20",
                "-Wall",
                "-Wextra",
                "-Werror",
                f"-I{UWP}",
                f"-I{ROOT / 'Shipwright/libultraship/include'}",
                str(UWP / "core_bootstrap.cpp"),
                str(ROOT / "tools/uwp_core_bootstrap_test.cpp"),
                "-o",
                str(executable),
            ],
            check=True,
        )
        subprocess.run([str(executable)], check=True)
    tests += 1

    print(f"UWP package contracts: {tests}/{tests} PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
