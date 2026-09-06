# 2026-09-06 — Xbox first-boot diagnostics and package icon

Task: KANBAN #212, #213, #214

## Hardware result

GitHub Actions run 21 completed the three-job pipeline, including the signed package artifact. The
package installed on the physical Xbox under the intended independent identity. The owner uploaded
`oot.o2r`, `oot-mq.o2r`, and decrypted `oot3d.3ds` to the exact `LocalState\soh` directory and the
portal showed all three files. Two launch attempts exited immediately. Xbox Device Portal listed the
application under Crash Data but produced no downloadable dump, including after the title was no
longer running.

This closes packaging, signing, installation, identity, and private-file placement as explanations.
It does not prove the renderer or game boot path: neither reached observable output.

Run 21 used a mobile-workflow hotfix that collapsed PowerShell's wrapped SignTool whitespace before
recognizing the sole expected untrusted-root diagnostic. That proven correction now lives directly
in both package drivers, so the next mobile workflow does not rewrite checked-out source at runtime.

## Earliest unobservable boundary

The WindowsStore wrapper linked `soh_core.lib` directly. Consequently Windows had to load
`soh_core.dll` and all of its transitive imports before entering the wrapper's `WinMain`. A rejected
desktop/UWP import or missing dependency could therefore terminate the title before either SoH's
logger or a wrapper logger existed. With no portal dump, the package supplied no discriminator
between loader failure, SDL/CoreWindow failure, ABI handoff failure, and a core exception.

The wrapper no longer imports the game core. `uwp/boot_diagnostics.cpp` starts from the SDL WinRT
callback, rewrites `LocalState\soh\uwp-boot.log`, and closes the file after every line. It records:

1. package version and callback entry;
2. the `uwp_GetWindowReference` boundary;
3. `LoadPackagedLibrary(L"soh_core.dll")` and its Win32 error on failure; a failed load also probes
   every packaged SDL2/libuwp/Mesa module separately so a transitive failure is not another blind run;
4. `GetProcAddress("Zelda3D_CoreEntry")` and its Win32 error on failure;
5. entry into and return from the existing ABI validator/core runner; and
6. Windows exception code and address from a destructor-free SEH boundary around the core call.

If the revised package still produces no log, the failure is before the SDL callback and is narrowed
to the package loader or statically imported SDL2/libuwp boundary. If a log exists, its final line
selects the next correction without another blind core rebuild. The preserved core ABI and artifact
key are unchanged, so this wrapper-only experiment can reuse the proven 75-minute core build.

## Custom icon root cause and gate

The manifest already named the five UWP logo resources, but every corresponding file under
`uwp/Assets` was the stock Ship of Harkinian sailboat. No build step injected the owner's image, and
the unpacked-AppX audit did not inspect logo content.

All five resources are now deterministic Lanczos resizes of the approved square
`Legend of Zelda: Master Quest Flames.png` artwork. `tools/uwp_runtime_stage.py` requires each file,
parses its PNG dimensions, and verifies its exact SHA-256 during the unpacked-package audit. The test
fixture proves both the approved set and rejection of a substituted/stale icon.

## Host evidence

- `python3 tools/test_uwp_package_contract.py`: 18/18 pass.
- `python3 tools/test_cursor_fps_v3_full_build.py`: 18/18 pass after reconstructing the pinned
  recoverable ZAPDTR submodule exactly as CI does.
- `python3 -m py_compile tools/uwp_runtime_stage.py tools/test_uwp_package_contract.py`: pass.
- `git diff --check`: pass.
- `python3 tools/codemap.py check`: still fails on pre-existing missing decomp/submodule references
  and retired scripts in this sparse reconstruction; the changed UWP row itself is current.

## Remaining gate

Build the wrapper/package job against the preserved core, install the update, launch once, then read
`LocalState\soh\uwp-boot.log`. No claim of a fixed Xbox runtime is made until that evidence identifies
the boundary and the title renders on hardware.
