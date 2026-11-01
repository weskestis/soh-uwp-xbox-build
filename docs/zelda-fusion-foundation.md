# Zelda Fusion: Xbox foundation

Working app name: **Zelda Fusion**. User requirement: install alongside all previous builds.

## Package isolation

| Field | Fusion value |
|---|---|
| Package Name | `WesKestis.ZeldaFusion.UWP` |
| Publisher / certificate subject | `CN=WesKestisZeldaFusion` |
| Display name | `Zelda Fusion` |
| Application ID | `ZeldaFusion` |
| Executable | `zelda-fusion.exe` |
| Distribution folder | `ZELDA-FUSION-XBOX` |
| User data | Fusion package's own `LocalState\soh` |

Windows derives package-family identity from Name and Publisher. Both remain fixed for future
Fusion updates; the version increases within Fusion's own family. The existing SDL preference
path is package-scoped, and saves, settings, and boot logs derive from it. The internal `soh`
directory does not point at another installation. No automatic save migration is added.
CMake refuses identity overrides, the package helper rejects a legacy publisher/artifact name,
and the final unpacked-package audit requires the exact Fusion name and signing publisher.

## Agreed combined-game scope

- Import supported OoT, OoT MQ, and MM ROMs through Xbox Device Portal, validating and extracting
  on Xbox.
- Each seed selects exactly **OoT + MM** or **OoT MQ + MM**. Keep that edition locked to the
  seed/save, with its matching checks, entrances, layouts, and key requirements.
- Use MM's child Link model across both worlds.
- Make equipment and transformations functional across both worlds.
- Randomize enemies and enabled item/entrance pools across the selected pair, with logic for
  age, forms, time, equipment, combat, terrain, room completion, and return connections.

## Current checkpoint

Native Store run `41239071818`, source `3c4559d3a934cbb3598a4ab2c03122c3be1a5ffa`,
passes both game cores: native C++20, SDK 19041, dynamic Store CRT, AppContainer,
Mesa import ABI, and the corrected FreeType environment-import gate. This is
compile/link/import evidence; physical Xbox rendering and execution are unverified.

Packaging review found MM's inherited 3DS scene default could hide original N64
backgrounds without replacement assets. Commit `4bec8b52bdeb14f0a226c6fbea409fb6521f8e25`
keeps the UWP scene, actor and skeleton draw paths original. Actual extracted UWP
functions compiled and returned the N64 fallback without dereferencing deliberately
invalid pointers; archive/version/FIFO tests and all three MM CMake profiles pass.
The MM job in native follow-up run `41246667942` passes this correction; its DLL
SHA-256 is `27cc727fab543f68bcae5e9998a65fc277bee23b4909aa57b5008b816f3fd918`.
OoT already defaults
to Original SoH in a fresh profile; importing previous 3DS settings is outside this test.

The new `Fusion Xbox test installer` workflow reuses the exact passed OoT artifact
and the corrected MM artifact after its individual native job succeeds. Each core's
source, native audit and hash are recorded separately. It generates the ROM-free
OoT port archive at the pinned asset source and reuses the verified matching MM
port archive. It does not use the old static-CRT cache or newest-artifact fallback.

The wrapper reads a bounded `LocalState/soh/fusion-startup.txt` with four allowed
pair/game routes, checks the selected private archive is readable, and launches
one core. Missing selection defaults to OoT; invalid/unreadable selection stops
before core loading. MQ layout enforcement, pair-bound saves, raw-ROM import,
cross-core handoff and the combined randomizer remain unimplemented.

Packaging explicitly includes both cores, both port archives, the five pinned
SDL2/Mesa dependency DLLs and the exact Microsoft retail x64 Store framework.
Checks cover identity/version/architecture, signatures, every staged payload hash,
private-file exclusion, framework metadata and packaged import/export resolution.
The 20 existing host package checks and four new behavioral/provenance test groups
pass. [Installer run 41247694255](https://github.com/weskestis/soh-uwp-xbox-build/actions/runs/41247694255),
wrapper source `f7a886a3ab124f183181b30f8625f892855ba678`, passes all three jobs and
produces the signed **1.0.1.1** diagnostic MSIX, Microsoft-signed retail dependency,
public certificate, four startup selections and Android installation instructions.
The unsigned and signed audits each verify 118 staged files plus the native wrapper
and resolve 879 packaged/framework symbol references. Signature checks accept only
the matching signer and the expected untrusted self-signed root on the clean runner.
Artifact `13060137578` (`ZELDA-FUSION-XBOX-1.0.1.1`) has ZIP SHA-256
`cfd6e8ac239e39a1f07d12832a046e6384ad5151e44ecb1c479a68767dd774a7`.
The downloaded ZIP, extracted delivery hashes and all 119 MSIX payload files were
independently verified. The MSIX SHA-256 is
`e30c5e8e520bef7cab8e7b66c56c0c20ccb55b62fb6f469a4d47e79ae7c3457b`.
The delivery includes `BUILD.json`, `PACKAGE-AUDIT.json`, and `SHA256SUMS.txt`.
Physical Xbox installation, Mesa dynamic driver loading, operating-system imports,
rendering, controller input, audio and saving remain unverified. This package is
one original N64 game per launch; it does not close the combined-game scope above.

## Validation

### First Xbox launch report (2026-10-31)

The owner reports that Fusion closes at launch after uploading the converted game archives.
The supplied `uwp-boot.log.txt` identifies package 1.0.1.1, `callback.enter`,
`startup.selection.default-oot`, and `archive.missing.LocalState-soh-oot.o2r`.
This is the wrapper's intentional archive-preflight exit, before loading any game core.
The file is missing, unreadable, or empty at the path opened by that check; the log does
not distinguish those three causes or establish an engine/graphics failure.

Read-only checks of the delivered 1.0.1.1 MSIX confirm the audited wrapper/core/dependency
hashes and expected diagnostic markers. Its `soh.o2r` and the converted `oot.o2r` both carry
port version 9.2.3; the ROM identity is supported US OoT 1.0 (`EC7011B7`), and the packaged
port archive includes the five fonts loaded during OoT initialization. These checks do
not prove that the uploads are complete on the Xbox or that the app can render there.

The wrapper opens both the boot log and `oot.o2r` under the same
`SDL_GetPrefPath(nullptr, "soh")` directory. Disassembly of the exact shipped SDL2 DLL
(`d364f6d4bec5f7a82998ffebb75578c502001e1a20c1c257120bdd41f34cedbd`)
confirms that `SDL_GetPrefPath` appends its app argument and creates the subfolder;
it does not ignore `soh`. The next device step is to upload the exact `oot.o2r`
alongside the existing `uwp-boot.log`, checking the filename and completed transfer.
The delivered file is 33,140,948 bytes. The next launch overwrites the boot log.
Successful preflight, engine loading, and Xbox rendering remain unconfirmed (#217).

The subsequent `uwp-boot.log (1).txt` passes the readable-file preflight, loads
`soh_core.dll`, resolves the entry symbol, and reaches `core.run.enter`. It then
records `0xC0000005` at `00007FFFAA25CE77` and returns the caught-exception code 77.
This supersedes the missing-file gate above. `core.run.enter` precedes descriptor
acquisition, so it does not by itself prove that OoT's runner began. The old log
lacks the loaded module base, and the exact retained core has no PDB/CodeView
record; the absolute ASLR address cannot be assigned a reliable source location.

The wrapper-only follow-up keeps the same cores and adds descriptor/runner stage
markers, unbuffered `uwp-stdout.log` and `uwp-stderr.log` beside the boot log,
module/base/RVA and access-operation reporting, registers, and a bounded 24-frame
walk of the copied fault context. `uwp/boot_exception_report.cpp` owns protected
fault inspection. It uses UWP-supported `VirtualQuery`, `GetModuleFileNameW`,
`RtlLookupFunctionEntry`, and `RtlVirtualUnwind`; no DbgHelp or PSAPI is added.
Nested SEH contains a failed context/stack read while preserving the original
exception line. This is diagnostic instrumentation, not a claimed engine fix.

`tools/uwp_boot_diagnostics_host/` loads production diagnostics and a separate
fake core as Store-CRT DLLs in a native host. The new package gate exercises
normal return, read/write/execute faults, a descriptor fault, and recursion, then verifies
durable shared stdio/Win32-handle output, real module RVAs, caller frames, and
stage ordering. Local packaging contracts (20/20) and four package test groups
pass; native execution and the updated signed installer are pending.

The first native diagnostic attempt compiled the production driver but rejected
the fake core's inconsistent export declaration. The fixture now uses
`ZELDA3D_CORE_BUILD` and the shared `ZELDA3D_CORE_API` declaration. Diagnostic
packaging also reuses original OoT port artifact 13059977713 from passed run
41247694255, verifying its wrapper/source provenance, ZIP hash, port version,
and exact `soh.o2r` hash against the signed 1.0.1.1 payload. This avoids rebuilding
unchanged assets. Superseded wrapper builds cancel; full-core workflows retain
their independent compilation policy.

### WinRT startup audit follow-up (2026-10-31)

The shared SDL2 backend no longer subclasses WinRT's CoreWindow as a desktop HWND. Desktop
subclassing is retained only after checking SDL's window-manager result and subsystem. UWP builds
exclude desktop DbgHelp crash dialogs, file-drop symbol lookup, debug console allocation, and
ImGui's default Win32 clipboard/IME/shell helpers. The pinned ImGui patch also excludes desktop
SDL docking callbacks. The wrapper's existing SEH boot diagnostics remain responsible for the
core boundary; the engine no longer installs the desktop exception filter in UWP.

The installed asset directory now comes from WinRT SDL's `SDL_GetBasePath`, ahead of the
`NON_PORTABLE` build-machine prefix, retaining explicit bundle overrides. SDL owns allocation
and `SDL_free` releases it. MM creates its file logger before archive validation/window creation,
so the existing boot-error catch can record missing/incompatible archives under LocalState.
`ReleaseLogger.h` makes UWP logging synchronous; desktop Release keeps its asynchronous logger.
The native logger test writes through this factory and exits immediately without destructors,
requiring caller-thread delivery and the complete error on disk. Its native execution passed
in the startup follow-up run recorded below.
The package audit now rejects `mm.o2r`, including nested/case-varied names, alongside OoT data.

`tools/test_uwp_core_imports.py` passes nine tests. The real `dumpbin /imports` gate rejects known
desktop callback DLLs/symbols, requires SDL2's package path and packaged GL loading, and produces
an explicitly limited report. It deliberately does not reject `GetSecurityInfo` just because the
old DLL imports it from ADVAPI32: Microsoft includes that function in UWP's security API set.
The strengthened audit rejects the previously built MM DLL. Native recompilation and the
strengthened audit pass in the final run below. Local host/profile/package checks and applying
the patches to the exact pinned ImGui and RmlUi tags also pass.

Native startup run [41228266922](https://github.com/weskestis/soh-uwp-xbox-build/actions/runs/41228266922),
source `e6fd42d`, passed host checks and the real immediate-exit logger test: the full error remains
in `early-error.log` after exit 23. MM compilation exposed `Context.cpp` including `libloaderapi.h`
before `windows.h`, formerly masked by CrashHandler's transitive Windows include. The Windows SDK
umbrella header now precedes its specialized headers in Context and SDL2; the next run compiles.

Run [41229086328](https://github.com/weskestis/soh-uwp-xbox-build/actions/runs/41229086328), source
`20ff1fe`, passes the full MM compile/link and immediate-exit logger test. The stricter import audit
correctly fails on one remaining desktop function: `USER32!MessageBoxA`, in RmlUi 6.2's default
assertion logger. Its new UWP-only patch keeps `OutputDebugStringA` and excludes the dialog.
DbgHelp, Shell32 and Imm32 were already absent in that DLL.

Final native run [41230510373](https://github.com/weskestis/soh-uwp-xbox-build/actions/runs/41230510373),
source `8114e73`, passes all host checks, Windows compilation/linking, the immediate-exit logger
test, and the stricter import audit. The downloaded reports also pass the audit locally.
MM now imports only `opengl32.dll`, `advapi32.dll`, `bcrypt.dll`, `SDL2.dll`, and `kernel32.dll`;
User32, Shell32, Imm32 and DbgHelp are absent. The entry export, SDL2 base-path resolver and
packaged GL loader are present. MM DLL SHA-256:
`e0f3f5876a378afc32906077ef0818e485cd7ae37aea2a8a0c0b68ee9fa181b0`.
Diagnostics artifact: `13054066399`; experimental DLL artifact: `13053688670`.
The report explicitly leaves CRT/API certification, shared-engine handoff and Xbox execution
unverified. ROM-free asset regeneration also passed run 41228266926 at source `e6fd42d`.

**Newly confirmed delivery blocker:** the existing normal-Windows core and vcpkg profile use
`/MT`. Microsoft's [UWP CRT guidance](https://learn.microsoft.com/en-us/cpp/c-runtime-library/windows-store-apps-the-windows-runtime-and-the-c-run-time?view=msvc-170)
excludes UWP apps and DLLs statically linked to the CRT. The supported migration requires a
consistent dynamic CRT across both cores and dependencies, UWP-compatible dependency paths,
appropriate API-set linkage, and packaged framework dependencies. `WindowsApp.lib` alone does
not repair a desktop CRT or certify imports. The current compiler artifact must not be promoted
to a usable Xbox installer. This is separate from the remaining shared-engine ownership work.

The isolated `tools/uwp_runtime_probe/` checks the proposed runtime configuration before migration:
normal-Windows CMake, MSBuild Store project properties, `/MD`, `WINAPI_FAMILY_APP`, WindowsApp,
`/APPCONTAINER`, and `/WINMD:NO`. Its DLL exercises strings, exceptions, threading and absolute-path
filesystem/stream code. The separate `fusion-uwp-runtime-probe.yml` preserves verbose library paths,
imports, exports and PE headers; its audit requires actual Store CRT library paths and `_app` CRT
imports. The passing native rerun is recorded below. The two host audit tests reject six forms of missing/desktop
evidence and explicitly leave execution/game migration unverified.

The first native probe (run 41228533033, source `e04804b`) configured but exposed Store props
automatically enabling `/ZW`, incompatible with C++20. It also showed CMake's newer SDK selection
policy choosing 26100 despite `CMAKE_SYSTEM_VERSION=19041`. The probe now explicitly disables
C++/CX per source and selects/verifies SDK 19041 through the generator platform. Native rerun
[41228728878](https://github.com/weskestis/soh-uwp-xbox-build/actions/runs/41228728878), source
`a8d3859`, passed compilation/linking and the full probe audit. The downloaded diagnostics pass
the audit again: Store CRT paths, `msvcp140_app.dll`, `vcruntime140_app.dll`,
`vcruntime140_1_app.dll`, API-set imports, the C export, and AppContainer flag are present.
Probe DLL SHA-256: `8b95ae70d79faa19a6a867e746477ca6d8fba2c67180c008b7c91bd845572227`.
This establishes native link configuration, not execution or migration of the game dependencies.

Migration still needs reviewed UWP dependency builds: pinned OpusFile and GLEW/OpenGL ports declare
UWP unsupported; FreeType's existing UWP file mapping branch needs appropriate selection; libzip
must use its WindowsStore `CreateFile2` path. The shared texture interpreter also has a desktop-only
`GetModuleHandleExA` image-address check. Replacing `/MT` alone would leave those API issues intact.
The three archive headers also undefine `_DLL` before including STL headers; that must be removed
or replaced with the dependency's own static-link definitions during the `/MD` migration. Root,
libultraship, OoT and MM CMake files all select CRT settings, so a root-only change is insufficient.

`python3 tools/test_uwp_package_contract.py`: **19/19 PASS** on 2026-10-31. The tests populate the
checked-in manifest from its CMake defaults, accept Fusion and later Fusion versions, reject four
old package names, reject a matching legacy signing publisher, and reject a publisher mismatch.
Inherited staging, icon, private-file, and compiled bootstrap checks remain covered.

The compiled bootstrap test uses fake core descriptors/runners and passes **4 valid routes,
40 descriptor failures, and 7 invalid selections**. It checks DLL names, exact engine IDs, ABI
rejection, missing entry/descriptor/runner, argument forwarding, return values, and that invalid
selections never call the entry point. These are host checks of the actual bootstrap source; they
do not compile the Windows wrapper, load game DLLs, exercise MQ layouts, or run gameplay.

`python3 tools/test_mm_uwp_contract.py`: PASS for archive version decoding (both byte orders,
truncation and invalid byte-order rejection, exact port version and major-only ROM compatibility),
both POSIX and disabled Windows FIFO compilation, and three generated CMake profiles (Xbox SDL2
archive-only, desktop SDL2 archive-only, desktop SDL3 extraction). CMake uses stub dependency targets
for these host checks; only the separate native workflow can establish actual Windows compilation.

The changed-source size/no-growth checks pass; `BenPort.cpp` was reduced to 1,989 lines and its
legacy ceiling reduced with it. The broader source-structure unit suite has one unchanged baseline
failure (`SohRmlUi.cpp` is 1,228 lines against 1,200); it is not a regression from this port.

`tools/codemap.py check` reports the same 22 missing external/pre-existing paths as the unmodified
baseline (including optional decomp submodules); this change introduces zero missing references.

Physical acceptance is pending: install alongside a prior build, create distinct settings/saves
in each, restart both, and verify independent persistence. Source checks do not prove Xbox behavior.

Reference: [Microsoft package identity overview](https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/package-identity-overview).
