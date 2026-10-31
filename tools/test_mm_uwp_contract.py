#!/usr/bin/env python3
"""Host checks for MM's Xbox build graph and portable archive/transport boundaries.

CMake profiles use stub dependency targets and do not compile the game. The
separate Windows workflow is required evidence for MSVC compilation/linking.
"""

from __future__ import annotations

import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def run(args: list[str]) -> None:
    result = subprocess.run(args, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(f"Command failed: {args}\n{result.stdout[-8000:]}")


def check_profile(work: Path, name: str, *, xbox: bool, extraction: bool, sdl2: bool) -> None:
    source = work / name
    source.mkdir()
    packages = source / "packages"
    packages.mkdir()
    for package in ("Ogg", "Vorbis", "Opus", "OpusFile", "SDL3"):
        (packages / f"{package}Config.cmake").write_text(f"set({package}_FOUND TRUE)\n")
    definitions = "\n".join(f'set({key} {"ON" if value else "OFF"})' for key, value in {
        "ZELDA3D_UWP_CORE": xbox, "ZELDA3D_UWP": xbox,
        "ZELDA3D_ENABLE_ROM_EXTRACTION": extraction, "ZELDA3D_SDL2_OPENGL": sdl2,
        "MSVC": xbox,
    }.items())
    package_dirs = "\n".join(f'set({package}_DIR "{packages.as_posix()}")' for package in
                              ("Ogg", "Vorbis", "Opus", "OpusFile", "SDL3"))
    (source / "CMakeLists.txt").write_text(f'''
cmake_minimum_required(VERSION 3.26)
project(MMProfileFixture LANGUAGES C CXX)
set(CMAKE_BUILD_TYPE Release)
set(ZELDA3D_SHARED_DIR "{ROOT.as_posix()}/Shipwright/zelda3d_shared")
set(ZELDA3D_ENGINE_ASSETS_DIR "{ROOT.as_posix()}/Shipwright/libultraship/assets")
set(SPDLOG_MIN_CUTOFF 0)
set(SPDLOG_LEVEL_OFF 6)
{definitions}
{package_dirs}
foreach(dep libultraship cmb3d zelda3d_shared stb SDL2::SDL2 SDL3::SDL3
            Ogg::ogg Vorbis::vorbis Vorbis::vorbisenc Vorbis::vorbisfile
            Opus::opus OpusFile::opusfile Opusfile::Opusfile)
    add_library(${{dep}} INTERFACE IMPORTED GLOBAL)
endforeach()
if(ZELDA3D_ENABLE_ROM_EXTRACTION)
    add_library(ZAPDLib INTERFACE IMPORTED GLOBAL)
endif()
add_subdirectory("{ROOT.as_posix()}/2ship" mm)
get_target_property(kind mm_lib TYPE)
get_target_property(sources mm_lib SOURCES)
get_target_property(exports mm_lib COMPILE_DEFINITIONS)
get_target_property(options mm_settings INTERFACE_COMPILE_OPTIONS)
get_target_property(defines mm_settings INTERFACE_COMPILE_DEFINITIONS)
get_target_property(links mm_settings INTERFACE_LINK_LIBRARIES)
get_target_property(output mm_core RUNTIME_OUTPUT_DIRECTORY)
file(WRITE "${{CMAKE_BINARY_DIR}}/report.txt"
    "kind=${{kind}}\nsources=${{sources}}\nexports=${{exports}}\noptions=${{options}}\ndefines=${{defines}}\nlinks=${{links}}\noutput=${{output}}\n")
''')
    build = source / "build"
    run([os.environ.get("CMAKE", "cmake"), "-S", str(source), "-B", str(build)])
    report = dict(line.split("=", 1) for line in (build / "report.txt").read_text().splitlines())
    assert report["kind"] == ("OBJECT_LIBRARY" if xbox else "STATIC_LIBRARY")
    assert "ZELDA3D_CORE_BUILD" in report["exports"].split(";")
    assert ("SDL2::SDL2" if sdl2 else "SDL3::SDL3") in report["links"].split(";")
    assert ("SDL3::SDL3" if sdl2 else "SDL2::SDL2") not in report["links"].split(";")
    assert ("ZAPDLib" in report["links"].split(";")) == extraction
    assert ("ZELDA3D_MM_NO_ROM_EXTRACTION=1" in report["defines"].split(";")) != extraction
    assert ("host/rom_extraction.cpp" in report["sources"]) == extraction
    assert ("extractor/Extract.cpp" in report["sources"]) == extraction
    if xbox:
        assert "OpusFile::opusfile" in report["links"].split(";")
        assert "Opusfile::Opusfile" not in report["links"].split(";")
        assert "/bigobj" in report["options"].split(";")
        assert "-pthread" not in report["options"] and "-fpermissive" not in report["options"]
        assert report["output"] == f"{build.as_posix()}/mm/$<CONFIG>"


def main() -> int:
    scratch = ROOT / "scratch"
    scratch.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="mm-uwp-check-", dir=scratch) as temp:
        work = Path(temp)
        cxx = os.environ.get("CXX", "c++")
        version_test = work / "archive-version-test"
        run([cxx, "-std=c++20", "-Wall", "-Wextra", "-Werror",
             f"-I{ROOT / '2ship/2s2h'}", str(ROOT / "tools/mm_archive_version_test.cpp"),
             "-o", str(version_test)])
        run([str(version_test)])

        transport = ROOT / "2ship/2s2h/zelda3d/repl/mm3d_repl_transport.c"
        common = [os.environ.get("CC", "cc"), "-std=c17", "-Wall", "-Wextra", "-Werror",
                  f"-I{ROOT / '2ship'}", f"-I{ROOT / 'Shipwright/libultraship/include'}"]
        # Compile the real POSIX path as well as exercising the disabled Windows path.
        run([*common, "-c", str(transport), "-o", str(work / "posix-transport.o")])
        (work / "transport_test.c").write_text('''
#include "2s2h/zelda3d/repl/mm3d_repl_transport.h"
#include <assert.h>
static int called;
static void command(const char* c, Zelda3DMmReplReply r, void* u, void* v) {
    (void)c; (void)r; (void)u; (void)v; ++called;
}
int main(void) {
    Zelda3D_MmReplTransportReset();
    Zelda3D_MmReplTransportPoll(command, 0);
    Zelda3D_MmReplTransportPoll(0, 0);
    Zelda3D_MmReplTransportReset();
    assert(called == 0);
    return 0;
}
''')
        run([*common, "-D_WIN32", str(transport), str(work / "transport_test.c"),
             "-o", str(work / "transport-test")])
        run([str(work / "transport-test")])

        check_profile(work, "xbox-archive-only", xbox=True, extraction=False, sdl2=True)
        check_profile(work, "desktop-sdl2-archive-only", xbox=False, extraction=False, sdl2=True)
        check_profile(work, "desktop-sdl3-extractor", xbox=False, extraction=True, sdl2=False)
    print("MM UWP host contracts: archive policy, both FIFO paths, 3 CMake profiles PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
