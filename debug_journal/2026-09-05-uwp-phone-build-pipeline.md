# 2026-09-05 — ROM-free UWP phone build pipeline

## User constraint

The owner works from Android only. The project must perform the Windows SDK compile, AppX assembly,
signing, dependency collection, and artifact validation remotely. The owner's unavoidable device-side
work is limited to downloading the result, uploading it through Xbox Device Portal, supplying legally
owned private game files through Device Portal, launching, and reporting the hardware result.

## Package boundary

The previous wrapper required `oot.o2r`, `oot-mq.o2r`, and the OoT3D source at configure time and
copied all of them into AppX. That mixed private data with a reusable CI artifact. The new boundary is:

- AppX: code, public runtime dependencies, runtime UI assets, and source-generated `soh.o2r`;
- `LocalState\soh`: owner-supplied `oot.o2r`, optional `oot-mq.o2r`, and decrypted `oot3d.3ds` or an
  extracted `oot3d-romfs` directory;
- Actions artifact: signed AppX, public `.cer`, framework dependencies, hashes, notices, and the
  Android install/private-data guides;
- never uploaded: ROMs, derived private O2Rs, saves, HD packs, `.pfx`, `.p12`, or another private key.

This matches the existing runtime lookup. `Context::GetAppDirectoryPath("soh")` uses
`SDL_GetPrefPath`; SDL2's WinRT implementation maps an empty organization plus app name `soh` to the
package local folder followed by `\soh`. `LocateFileAcrossAppDirs` checks that writable directory
before the package bundle. The OoT3D model resolver uses the same app-directory helper.

## Split Windows core / WindowsStore wrapper

The first implementation incorrectly configured the entire dependency graph as
`CMAKE_SYSTEM_NAME=WindowsStore`. The established SoH UWP architecture has a narrower boundary: the
large engine/game renderer is a normal x64 Windows DLL with UWP-aware source gates, while only the
small `uwp/` entry point and AppX deployment project use the WindowsStore toolchain. The root build
now rejects a full-core WindowsStore configure and instead requires `ZELDA3D_UWP_CORE=ON` on a normal
Windows configure. That profile preserves SDK 10.0.19041.0, forces SDL2/OpenGL and the OoT-only
build, enables `NON_PORTABLE`, disables the desktop launcher, scripting, and in-process ROM
extraction, and defines both `ZELDA3D_UWP` and `_UWP` for the app-container seams.

The core consumes SDL2 plus the Windows OpenGL ABI from the pinned public UWP dependency depot and
uses supported `x64-windows-static` vcpkg libraries for the rest. `libultraship` is folded into
`soh_core.dll`, all static libraries share the `/MT` runtime, GLEW supplies the Windows extension
entry points, the GCC-only CityHash byte swap is now portable C++, and the UWP profile excludes the
desktop WASAPI implementation in favor of SDL/WinRT audio. It also disables legacy MPQ/`.otr`
support so StormLib cannot add desktop Win32/WinINet imports; the package requires the generated
`.o2r` archive format. ImGui and RmlUi both select
`LoadPackagedLibrary` for the app-local Mesa OpenGL DLL. The missing-archive path reports that
data must be uploaded to `LocalState\soh`; it never scans for or extracts ROMs inside the package.

The remote workflow pins:

- `SternXD/uwp-dep` at `f5dbd58ee06a4b439bf260d641585dbc8bef3b86`;
- vcpkg at `9e593bb18ea69cc5095e012465dcd675a822ed0d`.

A Linux job builds only the redistributable `soh.o2r`. A Windows 2022 core job installs static x64
Windows libraries, builds the self-contained `soh_core.dll`, validates Shipwright's exact
`x64/Release` output, and preserves it as a short-lived ROM-free artifact. A separate WindowsStore
job consumes the core plus `soh.o2r`, validates the pinned UWP runtime DLL set, and builds the small
WinRT wrapper. It creates a one-run self-signed code-signing certificate, signs and verifies the
AppX, exports the public certificate, audits every required runtime file plus private/build-only
exclusions in the unpacked AppX, deletes the PFX in `finally`, and scans the final tree again before
upload. This job boundary lets a failed wrapper/package retry reuse the successful costly core.

## First remote run: unpublished ZAPDTR gitlink

The Android-triggered GitHub run reconstructed source commit `70b94235` and verified its bundle, then
failed before compilation because the parent gitlink names ZAPDTR commit `5f37af8`, which exists in
the source checkpoint but was never published by `SomeoneIsWorking/ZAPDTR`. GitHub correctly returned
`upload-pack: not our ref`; this was a source-provenance failure rather than a compiler failure.

The workflow now clones the public `zelda3d` base `7d1bdbc`, verifies it, imports a checked-in 1.6 KiB
incremental Git bundle, and verifies the recovered HEAD is exactly `5f37af8`. The bundle SHA-256 is
`f2be655739afe616fd56fb4ac88ee0f4309beb9d7da54fe5345e1d6dcdd1adb6`. A clean replay fetched the
increment against a depth-one clone and produced the exact commit with a clean worktree. StormLib's
required `429fe7e` remains published on its public `zelda3d` branch.

The next Android-triggered run passed the ZAPDTR recovery and dependency-install steps, then reached
host CMake configuration. Ubuntu Noble's `libzip-dev` config imports the separate `zipcmp`,
`zipmerge`, and `ziptool` executable targets, but the runner did not have those three binary packages;
configuration stopped first at missing `/usr/bin/zipcmp`. The workflow now installs all three Noble
packages explicitly so the complete exported libzip target set is valid rather than fixing only the
first missing executable.

The following run completed the entire Linux asset job, including generation and validation of
`soh.o2r`, and entered the Windows job. Source reconstruction, the StormLib checkout, pinned UWP
dependency checkout, pinned vcpkg checkout, and vcpkg bootstrap all passed. Dependency planning then
stopped because the pinned `opusfile` manifest declares `supports: !uwp`. SoH cannot simply discard
that library: `mixer.c` uses `op_open_memory`, `op_read`, and `op_free` for streamed Opus samples.
The port already disables its optional HTTP layer and otherwise builds a static decoder against the
UWP-capable Ogg/Opus ports, so CI now uses vcpkg's explicit `--allow-unsupported` trial rather than
silently removing audio.

The next run proved that trial: vcpkg compiled and installed every requested x64-uwp dependency,
including `opusfile`, in 1.9 minutes. CMake then rejected the WindowsStore configure invocation before
project generation because PowerShell split the unquoted
`-DCMAKE_SYSTEM_VERSION=10.0.19041.0` native argument into `10` and an extra `0.19041.0` path. CMake
therefore saw unsupported Windows Store version `10`. Both WindowsStore identity arguments are now
quoted as complete PowerShell native arguments, while the exact SDK version remains 10.0.19041.0.

The following run reached the first full WindowsStore core compile and exposed the architectural
error rather than one isolated XAML problem. Its complete log contained about 80 diagnostics rooted
in five classes: desktop C runtime calls rejected by the app toolchain (LunaSVG/cmb3d), ImGui's
desktop `LoadLibraryA`, MSVC's lack of `__builtin_bswap64`, Prism's explicit refusal of the UWP static
runtime, and StormLib's desktop Win32/WinINet implementation. `MSB4181: CompileXaml returned false`
was only the final wrapper message. Moving the full core back to normal Windows eliminates those
WindowsStore-only compile restrictions at their source. StormLib is additionally omitted from the
UWP core because its WinINet imports are not an app-container runtime boundary and `.o2r` already
supplies the required archive path; the portable CityHash replacement remains necessary on MSVC.
Run 7 validated source replay, all pinned dependencies, and configuration of this corrected split.
It compiled nearly the complete core before MSVC reported `C2065` for `CLOCK_MONOTONIC` and `C3861`
for `clock_gettime` in `Shipwright/soh/soh/host/frame_timing.cpp`. The platform-specific performance
counter functions were already guarded, but the separate presented-FPS ring had retained an
unguarded POSIX timestamp. Both the presentation ring and the non-Windows millisecond counter now
use `std::chrono::steady_clock`; Windows keeps its existing `QueryPerformanceCounter` path. The
workflow also captures core/wrapper output and emits the first unique compiler/linker failures as
run annotations so a future failure is inspectable without another mobile log hunt.

The remote rerun reconstructed commit `5eb90001`, passed the corrected presented-FPS source, and
continued past 2,200 compile-log lines. Its next first errors were two more authored REPL portability
leaks: `src/zelda3d/repl/repl_fps.cpp` independently used
`clock_gettime(CLOCK_MONOTONIC)`, and `repl_transport.cpp` unconditionally included `unistd.h` for a
Linux validation FIFO. The former now uses `std::chrono::steady_clock`. The latter preserves the
POSIX FIFO on non-Windows hosts but exposes no-op `PollTransport`/`ResetTransport` hooks on Windows;
the packaged application never requests `ZELDA3D_REPL`, so no product runtime behavior is replaced.
A full source scan found no other unguarded POSIX timing/FIFO imports in the authored core path.

Remote run 9 passed both REPL corrections and compiled the final authored source batch. Its public
diagnostic annotations contained five MSVC errors, all cascading from
`src/zelda3d/hud/zelda3d_hud_tex.cpp:29`: the file's broad `extern "C"` block gave the private
`cropAndBoxDownsample` helper C linkage even though it returns `std::vector<uint8_t>`. MSVC rejects a
C-linkage function returning a C++ class, then treated the invalid helper as `void` at its call sites.
The file-wide block is removed. Public `Zelda3D_*` HUD entry points retain explicit C linkage, while
private vector/string/cache helpers use normal C++ linkage. The actual source passes strict C++20
syntax compilation; its three pre-existing misleading one-line conditionals were expanded so the
same translation unit also passes `-Wall -Wextra -Werror`.

## Post-source compile: oversized redundant archive

The next remote run passed every prior source correction and compiled the entire authored OoT core.
It did not fail at a source line despite GitHub showing line 2216. That number was the build-log row
where MSVC's librarian reported:

`soh_lib.lib : fatal error LNK1248: image size (100B5D295) exceeds maximum allowable size (FFFFFFFF)`

`0x100B5D295` is 4,306,883,221 bytes, 11,915,926 bytes above the 32-bit archive-size ceiling. The
project historically declared `soh_lib` as STATIC and then placed `$<TARGET_OBJECTS:soh_lib>` into
`soh_core`; consequently the UWP build first archived every object even though the final DLL needed
the object set directly. At this source scale the unnecessary intermediate `.lib` is no longer
representable.

For `ZELDA3D_UWP_CORE` only, `soh_lib` is now an OBJECT library. It still owns and compiles the exact
same `ALL_FILES` list and `ZELDA3D_CORE_BUILD` definition, while `soh_core` still consumes
`$<TARGET_OBJECTS:soh_lib>`; only the doomed librarian operation disappears. Desktop builds retain
their historical static target. A configure-time `TYPE == OBJECT_LIBRARY` assertion turns any
future regression back to STATIC into an immediate configure error rather than another 38-minute
failure.

The same audit found that the source-owning x64 target had no `/O2` after the project cleared CMake's
default MSVC flags, retained `/Zi` in a Release package whose PDB is not shipped, and enabled IPO on
the non-source-owning DLL target. The UWP Release profile now compiles with `/O2`, omits compiler debug
information and link `/DEBUG`, and skips that ineffective IPO property. Existing `/MP` parallel
compilation, `/Gy`, `/OPT:REF`, `/OPT:ICF`, and `/INCREMENTAL:NO` remain. This reduces generated data
and link work without removing game code or weakening a diagnostic.

## Direct DLL link: nine unresolved contracts

The post-archive rerun confirmed that the OBJECT-library correction works: `soh_lib` completed and
MSVC entered the real `soh_core.dll` link. `LNK1120` reported nine unique unresolved names. Every
authored source had already compiled: six names had definitions in the core object set, and the
optional hook had its definition in the separate harness executable. This was not evidence that
files had been omitted.

Five Cucco globals are defined in `cucco_wing_override.cpp` but declared with C linkage by
`cucco_control.h`. The defining C++ translation unit included only `cucco_wing_override.h`, so those
five definitions received C++-mangled names while the C actor overlay requested the unmangled
names. Including the owner header before the definitions makes the existing definitions inherit the
declared C ABI. The same error existed at the use site for `gZelda3dZTargetActor`: the C++ input
module had a private `extern Actor*` declaration instead of including `actor_selection.h`; it now
includes that owner and removes the shadow declaration.

The remaining names came from two portability boundaries. `zelda3d_log.c` called the POSIX
`strcasecmp` and `strncasecmp` extensions, which MSVC's CRT does not export. Its channel grammar is
ASCII, so a small local ASCII-only comparator now handles both bounded tokens and complete names
without locale or platform dependencies. `graph.c` also referenced the harness executable's
`SohState_ApplyInputOverride` through a GNU weak undefined symbol. ELF can leave that optional and
resolve it from the harness process; an MSVC PE DLL must resolve it while the DLL itself links. The
hook remains unchanged on ELF hosts and is compiled out only under `_MSC_VER`; the packaged UWP
runtime never enables the development harness path.

The UWP gate now checks all three ownership/portability contracts together, so this exact nine-name
link failure cannot silently return.

## Successful core link, failed staging contract

Remote run 14 reconstructed `c098123f`, installed the pinned dependency sets, compiled the complete
normal-Windows game/renderer core, and linked `soh_core.dll` successfully. `Compile Windows core DLL`
passed after 1 hour 15 minutes 28 seconds. This remotely proves the object-library correction and
all nine PE/COFF symbol corrections; the source/link gate is no longer the current blocker.

The next one-second `Stage wrapper runtime` step reported `Missing Windows core build output:
soh_core.dll`. The workflow assumed every Visual Studio product remained somewhere under
`scratch/build-uwp-core`. That contradicts the active property sheet:
`Shipwright/CMake/DefaultCXX.cmake` assigns `ARCHIVE_OUTPUT_DIRECTORY`,
`LIBRARY_OUTPUT_DIRECTORY`, and `RUNTIME_OUTPUT_DIRECTORY` to
`${CMAKE_SOURCE_DIR}/${CMAKE_VS_PLATFORM_NAME}/${PROPS_CONFIG}`. For x64 Release, the authoritative
DLL, import library, and post-build asset tree are therefore `x64/Release/{soh_core.dll,
soh_core.lib,assets}`, outside the CMake binary directory. The previous recursive search could never
find a successful link product.

The correction does not replace that bad search with a broader guess. `tools/uwp_runtime_stage.py`
uses the exact property-sheet path and requires non-empty DLL/import-library outputs plus extractor,
XML, and RmlUi sentinels. It rejects private inputs, creates a ZIP64-capable internal core artifact,
extracts it with traversal checks in the package job, adds only the redistributable `soh.o2r`, and
requires every pinned SDL2/libuwp/Mesa binary and import library before CMake runs. The same tool
audits the unpacked AppX for all runtime DLLs, `soh.o2r`, RmlUi, the expected publisher, and absence
of ROM/private O2R/key/build-only files. A complete temporary-tree fixture executes stage, archive,
safe extraction, assembly, successful package audit, and a private-archive rejection locally.

The workflow is now three jobs. Host archive generation and the costly normal-Windows core compile
run independently. The wrapper/package/sign job depends on their short-lived artifacts but does not
contain vcpkg installation or core compilation, so a package-only retry cannot silently repeat that
work. The remaining PowerShell boundary now selects only version-numbered Windows SDK directories
that actually contain both SignTool and MakeAppx, forces AppX generation, cleans a partially-created
certificate safely, requires dependency notices, and records source/workflow/run identities.

Finally, the native wrapper was checked against SDL2's non-XAML WinRT entry contract and the public
libuwp consumer layout. It retains `WinMain -> SDL_WinRTRunApp`, links `runtimeobject.lib`, and passes
`/WINMD:NO`; the native wrapper does not ask link.exe to emit C++/CX metadata.

## Signed MSIX succeeded; trusted-root import blocked the hosted runner

The post-MSIX-discovery rerun generated and signed
`SOH-CURSOR-FPS-V3-OOT3D-FULL-x64.msix` successfully. It then remained inside the same package step
for more than 17 minutes without printing SignTool verification output. The only newly inserted
operation between the successful signing line and verification was an `Import-Certificate` call
targeting `Cert:\CurrentUser\Root`; that hosted-runner trust mutation is therefore removed.

The replacement does not weaken package-integrity checking. `Get-AuthenticodeSignature` must expose
the exact ephemeral signer thumbprint and must not report a hash or format failure. SignTool still
runs under the Authenticode policy. Because the build intentionally uses a one-run self-signed test
certificate, its single documented untrusted-root result is accepted only when it is the sole
SignTool error; any other verification error fails the build. CI no longer changes Trusted People
or Trusted Root stores. The package step also has a five-minute hard limit, so a future SDK or
certificate regression cannot consume another hour. These corrections are commit `ac8e72f9`.

The `ac8e72f9` rerun confirmed the time fix: the entire package job reached the audit failure in
1 minute 37 seconds. It again generated and signed the expected MSIX, and SignTool emitted exactly
one error: the documented untrusted self-signed root. The acceptance code nevertheless threw because
it overconstrained presentation details from two different providers: the localized
`Get-AuthenticodeSignature.StatusMessage` and a `^SignTool Error:` line anchor after PowerShell had
wrapped native stderr. Commit `e2d4058f` keeps the exact embedded-signer thumbprint check, explicitly
rejects every Authenticode status other than `Valid`, `NotTrusted`, or `UnknownError`, and lets the
captured SignTool result decide the narrow exception. It counts `SignTool Error:` anywhere in the
captured record and still requires exactly one occurrence plus the untrusted-root text; hash,
format, signer, additional-error, and every other policy failure remain fatal.

## Preserved core succeeded; MakeAppx rejected a dot directory

The first run with exact `x64/Release` staging passed both producer jobs. The package job downloaded
the preserved core and public port archive, assembled and validated the complete wrapper runtime,
configured the WindowsStore project, and compiled/linked the native wrapper. Twelve seconds into
`Build unsigned AppX`, MakeAppx returned `0x8007007B` (`ERROR_INVALID_NAME`).

The deployment helper assigned root DLLs and `soh.o2r` the explicit location `.`. CMake therefore
gave Visual Studio a literal dot package directory. That is not equivalent to an omitted root link:
MakeAppx rejects the resulting package-map component as an invalid directory name. Root payloads now
use an empty destination and do not receive `VS_DEPLOYMENT_LOCATION`; nested assets use either
`assets` or a normalized `assets/<relative-directory>` path, never `assets/` or `.`.

This run also proved why a same-run retry boundary was insufficient for iterative packaging fixes:
changing the source bundle/YAML creates a new workflow run. The Windows job now restores a cache
keyed to the proven core inputs. On the first cache miss it queries the repository's unexpired
Actions artifacts, prefers the new versioned core name, and uses the prior run's legacy core as a
one-time bridge. Every restored archive is ZIP-tested and checked for safe unique members, nonempty
core DLL/import library, required runtime assets, and absence of private files before use. A valid
restore skips the UWP dependency checkout, vcpkg, configure, compile, and staging steps, then seeds
the stable cache and a 30-day versioned artifact. A missing or invalid archive still falls back to a
clean full build.

## Host evidence

- `python3 tools/test_cursor_fps_v3_full_build.py`: 18/18 pass.
- `python3 tools/test_uwp_package_contract.py`: 18/18 pass, including the full staging/archive/
  assembly/AppX-audit fixture and private-input rejection.
- Strict standalone compile of `frame_timing.cpp` with GCC C++20 and `-Werror`: pass.
- Strict standalone compile of `repl_fps.cpp` and simulated `_WIN32` syntax compile of
  `repl_transport.cpp`: pass.
- Strict standalone compile of `zelda3d_hud_tex.cpp` with its real project headers and a minimal
  declaration-only stb fixture: pass.
- Workflow YAML parse and three-job dependency shape: pass.
- Recoverable ZAPDTR bundle hash and exact-commit clean replay: pass.
- Ubuntu Noble libzip executable-package contract (`zipcmp`, `zipmerge`, `ziptool`): present.
- Streamed Opus remains linked through the supported `x64-windows-static` core dependency set.
- Workflow and contract tests require the normal-Windows core, separate WindowsStore wrapper, exact
  SDK argument, static engine boundary, GLEW, portable CityHash, SDL-only UWP audio, no
  StormLib/WinINet dependency, and packaged OpenGL loader paths.
- Optimized SDL3 `soh_core`: linked.
- Optimized SDL2/OpenGL `soh_core`: linked.
- Production OpenGL model/HUD smoke under Mesa 4.5 compatibility: pass, with expected model and HUD
  pixels and no GL error.
- `git diff --check`: pass.
- `tools/codemap.py check` still reports four pre-existing stale references outside this work
  (`../shared/re-harness/tools/info.py` and three removed AppImage/archive scripts); the touched UWP
  renderer row is current.

## Honest remaining gate

Remote execution now validates the MSVC DLL compile/link, exact core staging, cross-job runtime
assembly, and WindowsStore wrapper compile/link. Host checks still cannot validate corrected AppX
creation/signing or Mesa, controller input, and GPU output on Xbox. The next run should import the
successful core artifact rather than compiling it again, then retry AppX generation, signing,
signature verification, and the unpacked runtime/privacy audit. Only after that succeeds should the
public certificate and signed package be offered for Device Portal installation. Card #212 remains
in progress until the owner confirms the physical Xbox mode switch, renderer, menu, cursor, audio,
and save behavior.
