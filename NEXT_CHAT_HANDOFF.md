# SOH Cursor FPS V3 + OoT3D Full Build — Next-Chat Handoff

## Source of truth

- Repository branch: `cursor-fps-v3-xbox-boot-diagnostics`
- Checkpoint 02 base: `4c22bda9edd834ebaa894caae3be7d6ff33239e1`
- UWP pipeline implementation commit: `1d9f74fd` (`build: add ROM-free Xbox UWP pipeline`)
- ZAPDTR CI recovery commit: `5241c1a1` (`build: recover unpublished ZAPDTR revision in UWP CI`)
- Presented-FPS MSVC portability commit: `5eb90001` (`fix: make Xbox frame timing portable`)
- REPL MSVC portability commit: `098f6ed8` (`fix: make diagnostic REPL portable on Windows`)
- PE/COFF link-contract commit: `c098123f` (`fix: resolve Xbox core DLL link contracts`)
- Core-runtime/package recovery commit: `e244309b` (`build: preserve and audit Xbox core runtime`)
- AppX-path/core-reuse commit: `d7204138` (`fix: reuse Xbox core and repair AppX paths`)
- Nonblocking signature-audit commit: `ac8e72f9` (`fix: bound ephemeral package signature audit`)
- Self-signed diagnostic acceptance commit: `e2d4058f` (`fix: accept the sole self-signed trust diagnostic`)
- Xbox boot diagnostics/custom icon commit: use the exact bundle HEAD recorded in the mobile workflow
  metadata delivered with this handoff.
- The source bundle metadata delivered beside this handoff names and hashes the exact checkpoint
  commit containing the corrected split Windows-core/UWP-wrapper architecture.
- Use the checkpoint ZIP delivered with this handoff as the sole source of truth.
- Do not add ROMs, extracted RomFS data, `.o2r`/`.otr` archives, certificates,
  AppX/MSIX packages, signing keys, or other private/copyrighted game assets to
  source control or a shared archive.

## User goal

Produce a separate, genuinely installable Xbox/UWP Device Portal app for the
user's SOH Cursor FPS V3 build with:

- the exact V3 cursor/FPS behavior;
- a toggle between original Ship of Harkinian presentation and the OoT3D
  presentation;
- normal Ocarina of Time and Master Quest support;
- the complete local randomizer, including MQ dungeon logic (online multiworld
  is not required);
- working resolution, FPS, controller, mouse, cursor, audio, menu, and all other
  SOH settings;
- save progress isolated from and preserved alongside the existing V3
  installation;
- optional user-installed Henriko Magnifico OoT3D 4K textures later;
- a user-owned OoT3D `.3ds`/RomFS import path, never redistributed in the
  package.

Do not call an AppX usable or complete until it is built, signed, installed,
launched, and runtime-tested on Xbox/UWP. Do not hide missing rendering behind
no-op stubs.

## Completed in this checkpoint

- Implemented a real current-schema SDL2/OpenGL OoT3D native renderer rather
  than restoring the obsolete queued GL renderer.
- Added model resource upload, authored/generated mip chains, an isolated VAO,
  shared uniform buffers, white-texture fallback, and deferred eviction.
- Added synchronous model drawing at the interpreter emission point, preserving
  native/Fast3D ordering and overlay-depth clears.
- Mirrored the fixed SDL3 GPU material contract: 64 bones, UV0/UV1/UV2, three
  textures, six generic TEV stages, material constants, UV overrides, scene and
  group lighting, sphere mapping, fog/3DS fog, tint, alpha, culling, blending,
  and depth behavior.
- Added a real OpenGL native-HUD renderer with ordered immediate batches,
  texture caching, clamp/repeat sampling, ENV modes 0/1/2, and teardown.
- Added comprehensive OpenGL state save/restore so native draws do not corrupt
  Fast3D state: programs, VAO/buffers, indexed UBO bindings, texture units,
  blend/depth/cull/scissor/viewport/polygon state, masks, and pixel-unpack
  alignment.
- Moved shared instrumentation/isolation globals out of the SDL3-only backend,
  completing symbol ownership in both renderer profiles.
- Made the authoritative fixed-shader source builder backend-neutral while
  leaving glslang/SPIR-V compilation exclusive to SDL3 GPU.
- Added a redistributable surfaceless EGL/Mesa smoke fixture at
  `tools/zelda3d_opengl_smoke.cpp`. It exercises production model-shader
  compilation, model rendering, state restoration, HUD rendering, teardown,
  and `GL_NO_ERROR`.
- Preserved SDL3 GPU as the normal/default renderer and SDL2/OpenGL as the
  opt-in Xbox/UWP-oriented profile.
- Updated UWP safety text accurately: Linux OpenGL is implemented and verified,
  while WindowsStore/Xbox remains unverified. The safety override still
  defaults OFF.
- Corrected the UWP architecture after the first full compile: the root now
  rejects a full-core WindowsStore build and instead builds a normal x64
  Windows core with `ZELDA3D_UWP_CORE=ON`; only `uwp/` owns the WindowsStore
  wrapper and AppX graph.
- Added a pinned public SDL2/libuwp/Mesa import boundary plus supported
  `x64-windows-static` vcpkg libraries. The UWP profile folds libultraship into
  `soh_core.dll`, aligns `/MT`, links GLEW, replaces the GCC-only CityHash byte
  swap, selects SDL/WinRT audio instead of desktop WASAPI, excludes legacy
  StormLib/WinINet (the package requires `.o2r`), and makes both OpenGL loaders
  use the packaged Mesa DLL API.
- Made AppX ROM-free: only the redistributable `soh.o2r` is packaged. Private
  `oot.o2r`, optional `oot-mq.o2r`, and `oot3d.3ds` are resolved from
  `LocalState\soh` after owner upload through Device Portal.
- Added a three-job GitHub Actions build: host `soh.o2r` generation and the costly normal-Windows
  core compile run independently, then a small WindowsStore wrapper/package job consumes their
  short-lived ROM-free artifacts. It builds AppX, signs with a one-run certificate, publishes only
  the public `.cer`, and audits required runtime contents plus private/build-only exclusions.
- The first post-staging run proved that core preservation, transfer, runtime assembly, wrapper
  configuration, and wrapper compilation all work. MakeAppx then rejected the generated package map
  with `0x8007007B` because root payloads explicitly used deployment location `.`. Root files now
  omit `VS_DEPLOYMENT_LOCATION`; nested assets use normalized nonempty package directories.
- Packaging-only runs now restore the versioned core cache or automatically import the latest
  unexpired core artifact, verify its ZIP members and required DLL/assets, and republish it for 30
  days. Dependency installation and the 75-minute core build execute only when no valid preserved
  core exists.
- Added `tools/uwp_runtime_stage.py`, a host-testable exact-output/staging/archive/AppX-audit owner.
  It uses Shipwright's real `x64/Release` Visual Studio output, validates the pinned SDL2/libuwp/Mesa
  set, safely extracts the preserved core, and prevents a package retry from rebuilding the core.
- Added phone-only install and private-data guides under `uwp/`.
- GitHub Actions run 21 completed all three jobs and produced the signed phone-ready package. It
  installed successfully on the physical Xbox under the independent package identity.
- The owner placed `oot.o2r`, `oot-mq.o2r`, and decrypted `oot3d.3ds` in the correct
  `LocalState\soh` directory. The first two launches exited immediately, and Device Portal captured
  no crash dump.
- Removed the wrapper's loader-time import of `soh_core.dll`. The wrapper now creates and flushes
  `LocalState\soh\uwp-boot.log`, loads the core through `LoadPackagedLibrary`, resolves the single
  ABI export explicitly, probes every packaged runtime module after a load failure, and records a
  caught Windows exception code/address.
- Replaced all five stock SoH package icons with deterministic sizes of the approved
  `Legend of Zelda: Master Quest Flames.png` artwork. The AppX audit now verifies their exact size
  and SHA-256, and its fixture rejects a stale/substituted icon.
- Folded run 21's proven PowerShell line-wrap normalization into both package drivers; the mobile
  workflow no longer needs to hot-patch signature verification after reconstructing the source.

## Verification completed

- SDL2/OpenGL full OoT core links successfully:
  `build-linux-full/soh/libsoh_core.so`.
- Default SDL3 GPU full OoT core links successfully from a clean offline build:
  `scratch/build-linux-sdl3/soh/libsoh_core.so` (2,205 initial Ninja steps,
  followed by a successful incremental rebuild).
- The software-GL production smoke test passes on Mesa 4.5:
  `Zelda3D OpenGL model/HUD smoke: PASS`.
- Both SDL2 shared libraries pass `ldd -r` without unresolved runtime symbols.
- The launcher loads and probes both OoT and MM cores with ABI 1 and proper
  symbol isolation: `loaded 2/2`.
- Full-build contracts pass: 18 tests.
- UWP/Android package contracts pass: 18/18, including the complete runtime staging/archive/
  assembly/AppX-audit fixture and private-archive rejection.
- GitHub Actions YAML parses and contains host-asset, preserved Windows-core, and Windows package
  jobs with pinned dependency commits and the correct dependency graph.
- `git diff --check` passes.
- `tools/codemap.py check` has a pre-existing repository-wide failure on four
  unrelated removed script paths; the Xbox/UWP renderer row is updated.

The detailed commands and evidence are in
`debug_journal/2026-09-05-opengl-oot3d-renderer.md` and
`debug_journal/2026-09-05-uwp-phone-build-pipeline.md`.

## Renderer ownership

- Dispatch/integration: `Shipwright/libultraship/src/fast/zelda3d_submission.cpp`
- OpenGL public ABI:
  `Shipwright/libultraship/include/fast/backends/zelda3d_opengl.h`
- OpenGL resource owner:
  `Shipwright/libultraship/src/fast/zelda3d_opengl_resources.cpp`
- OpenGL draw/pass owner:
  `Shipwright/libultraship/src/fast/zelda3d_opengl_pass.cpp`
- OpenGL state owner:
  `Shipwright/libultraship/src/fast/zelda3d_opengl_state.cpp`
- OpenGL HUD owner:
  `Shipwright/libultraship/src/fast/zelda3d_opengl_hud.cpp`
- Backend-neutral fixed shader templates:
  `Shipwright/libultraship/src/fast/zelda3d_sdl3gpu_shaders.cpp`
- Complete default renderer: `zelda3d_sdl3gpu*.cpp`
- Current model schema:
  `Shipwright/libultraship/include/fast/zelda3d_model_types.h`

## Historical build path

The user manually committed the phone workflow and exact source bundle to public
repo `weskestis/soh-uwp-xbox-build` from Android. Successive runs established the
source reconstruction, unpublished ZAPDTR recovery, Noble libzip tools, complete
host `soh.o2r` generation, pinned dependency checkout, vcpkg bootstrap, and exact
SDK invocation.

Run 6 reached the first full WindowsStore core compilation. The complete log had
about 80 diagnostics rooted in five classes: desktop C runtime calls rejected by
the app toolchain (LunaSVG/cmb3d), ImGui's desktop `LoadLibraryA`, MSVC's lack of
`__builtin_bswap64`, Prism's explicit refusal of the UWP static runtime, and
StormLib's desktop Win32/WinINet implementation. The visible
`MSB4181: CompileXaml returned false` line was secondary, not the root cause.

The local correction follows the established working SoH UWP boundary: build
the full core as normal x64 Windows with UWP-aware code paths and build only the
small wrapper as WindowsStore. It uses supported `x64-windows-static`
dependencies, keeps SDL2/Mesa from the UWP depot, folds libultraship into the
  core, aligns the static CRT, links GLEW, uses portable CityHash, removes
  desktop WASAPI and legacy StormLib/WinINet from the UWP profile, requires the
  modern `.o2r` archive, and loads Mesa with the app-container API.
  Run 7 validated this corrected split through configuration and nearly the
complete MSVC core compile. Its first real errors were the presented-FPS ring's
unguarded POSIX `clock_gettime(CLOCK_MONOTONIC)` calls. This checkpoint replaces
that ring with portable monotonic `std::chrono::steady_clock` timestamps and adds
public workflow annotations for future compiler/linker errors. The `5eb90001`
rerun passed that source and continued beyond 2,200 compile-log lines before
finding independent uses in the development REPL: `repl_fps.cpp` still used
`clock_gettime`, and `repl_transport.cpp` imported `unistd.h` for its Linux FIFO.
REPL FPS now uses the same portable clock. The FIFO remains functional on Linux
and compiles to ABI-preserving no-op hooks on Windows, where the packaged app
does not request the validation transport. Remote run 9 passed those fixes and
reached the final authored source batch. All five annotated compiler errors came
from one linkage mistake in `src/zelda3d/hud/zelda3d_hud_tex.cpp`: a broad
`extern "C"` block incorrectly applied C linkage to private helpers returning
`std::vector`. The file now gives explicit C linkage only to its public HUD ABI;
its private C++ helpers retain C++ linkage and the actual translation unit passes
strict host compilation. The following rerun compiled every authored source, then failed at the
redundant `soh_lib.lib` librarian step. Its measured `0x100B5D295`-byte archive was 11,915,926 bytes
over the `0xFFFFFFFF` MSVC limit; GitHub's displayed line 2216 was only the log row. The UWP core now
declares `soh_lib` as an OBJECT library, preserving the exact source/object set while feeding it
directly to `soh_core.dll`. A configure-time target-type assertion prevents recurrence. The UWP
Release source owner also regains `/O2`, omits unshipped `/Zi` data, and avoids ineffective IPO on
the non-source-owning shared target. The next rerun proved that correction by reaching the real DLL
link, where `LNK1120` named nine unresolved symbols. Five Cucco definitions had missed their
C-linkage owner header, the Z-target consumer used a C++ shadow declaration instead of its C owner,
the logger imported POSIX-only `strcasecmp`/`strncasecmp`, and the core relied on a GNU weak
undefined harness hook that PE/COFF cannot represent. Those contracts are now corrected: both C++
consumers include their C-ABI owners, the logger uses a local ASCII comparator, and only MSVC omits
the optional development harness hook. Remote run 14 proved every correction by compiling and
linking `soh_core.dll` successfully in 1 hour 15 minutes 28 seconds. The following one-second stage
failed only because the workflow searched `scratch/build-uwp-core`, while the active Visual Studio
property sheet deliberately writes the DLL, import library, and assets to `<source>/x64/Release`.

The workflow now consumes that exact directory through `tools/uwp_runtime_stage.py`, preserves the
ROM-free core in its own job, and validates all wrapper inputs before CMake. The package job no longer
installs vcpkg or compiles the core. SDK-tool discovery accepts only versioned SDK directories that
contain both SignTool and MakeAppx; the native SDL WinRT wrapper links `runtimeobject.lib` with
`/WINMD:NO`; AppX generation, publisher identity, all runtime files, notices, private-file rejection,
certificate cleanup, and build provenance are explicit. Successful completion of the signed-package
audit and the physical Xbox run are still unproven.

The latest two remote runs have now proved both WindowsStore wrapper packaging and ephemeral signing:
Visual Studio generated the expected MSIX and SignTool reported `Successfully signed`. The first then
hung on a trusted-root import. Commit `ac8e72f9` removed all trusted-store mutation and capped the
package step at five minutes; its rerun completed in 1 minute 37 seconds and SignTool produced exactly
the one expected untrusted-root diagnostic. That run was rejected only because the new audit coupled
acceptance to provider-localized status text and an error label anchored at the beginning of a
PowerShell-wrapped native record. Commit `e2d4058f` keeps exact signer/status/error-count checks but
accepts that label anywhere in the captured record. Signature audit completion, artifact upload, Xbox
installation, and physical runtime behavior remain unproven.

The preceding paragraphs record the path to the first artifact and are now superseded: run 21
completed the signature audit and artifact upload, and the package installed.

## Current hardware blocker

The first installed package exits immediately before presenting a frame. The three owner-supplied
files are in the correct directory, and Device Portal produced no dump. The previous static core
import made a loader-time failure invisible because it happened before `WinMain`.

The next package is intentionally a wrapper-only diagnostic/hardening update. It keeps the proven
core cache key unchanged, so GitHub Actions should reuse the preserved 75-minute core. After one
launch, inspect `LocalState\soh\uwp-boot.log`:

- no file: failure is before the SDL callback, in the package/static SDL2-libuwp loader boundary;
- `core.load.failed`: use the Win32 code and per-module probe lines;
- `core.symbol.failed`: the preserved core export is wrong or missing;
- `core.run.enter` as the last line: the core terminated outside catchable SEH;
- `core.exception`: use its exception code/address;
- `core.run.return result=1`: normal boot validation rejected an input; inspect the SoH log folder.

Do not call the Xbox runtime fixed until the report is collected and a physical render succeeds.

`ZELDA3D_UWP_ALLOW_INCOMPLETE_RENDERER` intentionally defaults OFF. Keep it OFF
for any claimed package. Its override is only for explicit wrapper/link
experiments until the WindowsStore build and Xbox runtime checks below pass.

## Linux build references

- SDL2 development prefix:
  `/workspace/scratch/69c2326d4f52/work/deps-prefix-sdl2/usr`
- Other dependency prefix:
  `/workspace/scratch/69c2326d4f52/work/deps-prefix`
- SDL2 build directory:
  `/workspace/scratch/69c2326d4f52/work/build-linux-full`
- Default SDL3 build directory: repository-local `scratch/build-linux-sdl3`

The dependency/build directories are workspace conveniences only and are not in
the source checkpoint.

## Private inputs

The user supplied their own
`3DS0033 - The Legend of Zelda Ocarina of Time 3D (U).3ds.7z` in an earlier
chat. It is not included here. Use it only as a private build/test input after
obtaining it in the active workspace. Never package or redistribute it.

## Immediate next task

Give the Android-only user the new source `.bundle` and the complete copyable
`build-xbox-uwp-mobile.yml`. They upload/replace those two files and run the workflow once. Confirm
that `Reuse or compile Windows core DLL` restores the preserved artifact and does not compile. Then
install the new package version, launch once, and read `LocalState\soh\uwp-boot.log`. Use the final
stage to fix the identified boundary. After the title renders, test original/OoT3D switching,
Normal/MQ, randomizer, save isolation, input, audio, resolution, and frame pacing. Keep
`ZELDA3D_UWP_ALLOW_INCOMPLETE_RENDERER=OFF` for the first package described as usable.
