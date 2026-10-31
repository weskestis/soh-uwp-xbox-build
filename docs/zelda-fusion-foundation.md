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

Implemented here: independent package identity, branding, signing defaults, rejection of legacy
identities, and a pair-aware DLL routing/validation boundary. `uwp/core_bootstrap.*` now accepts only
the following requested game routes:

| Selected pair | Requested game | Packaged DLL | Required core ID |
|---|---|---|---|
| OoT + MM | OoT | `soh_core.dll` | `oot` |
| OoT + MM | MM | `mm_core.dll` | `mm` |
| OoT MQ + MM | OoT MQ | `soh_core.dll` | `oot` |
| OoT MQ + MM | MM | `mm_core.dll` | `mm` |

An excluded edition or unknown selection returns error 78 before calling a core. A loaded DLL must
return the expected ID, current ABI, and a non-null runner. The Windows loader uses the matching
allowlisted filename and logs the requested pair/game, engine, and missing DLL by name. MQ is a
content/layout choice within the OoT engine, not a third engine.

**This is loader groundwork, not completed pairing enforcement.** Startup still requests OoT in
the OoT + MM pair. There is no pairing selector or persisted combined session yet; archive selection,
MQ layout enforcement, save binding, and randomizer logic are not connected to this boundary.
The package still builds only `soh_core.dll` and requires pre-extracted archives. No actual MM DLL
load, signed Fusion MSIX, or Xbox result is claimed.

The opt-in MM Xbox compile profile now selects SDL2/OpenGL, MSVC options, direct object aggregation,
the Windows core export, and archive-only startup. MM's desktop extraction UI moved out of
`BenPort.cpp` into `2ship/2s2h/host/rom_extraction.cpp`; `host/archive_boot.*` validates archives
before window setup in the Xbox profile. Missing, corrupt, or incompatible archives return through
the core's existing boot-error boundary. The Xbox profile excludes desktop console setup and the
POSIX developer FIFO. MM output stays in its own build directory so its assets cannot overwrite OoT.

`.github/workflows/fusion-mm-uwp.yml` is a separate branch-scoped compile check. It builds current
`mm_core` source on Windows, inspects exports/imports, and preserves diagnostics. Its DLL artifact is
experimental compiler output, not an installer. Dependency binaries are cached before the compile
so failures do not discard the dependency build. Native compilation is pending at this checkpoint.

### Next integration milestone

Boot each game from the separate Fusion application, then establish a safe shared runtime before
attempting world travel. The source survey found concrete prerequisites:

1. **Verify MM's Xbox compile/link.** The build and source adaptations above are implemented; the
   dedicated Windows check must pass before enabling MM in the installable package. Root CMake
   keeps MM off by default for existing UWP builds and permits an explicit `ZELDA3D_BUILD_MM=ON`.
2. **Give both cores one engine owner.** The current Xbox build folds libultraship into the OoT DLL.
   Linking that static library into MM too would create separate `Ship::Context` and switch-request
   state. Desktop handoff depends on one shared engine. MM teardown retains its engine context, and
   the existing lifecycle documents window teardown failures. Loading both DLLs alone does not fix
   that. Do not add an in-process switch loop until ownership and run/teardown boundaries are tested.
3. **Stage MM's distinct runtime assets.** MM needs `2ship.o2r` plus the user's `mm.o2r`. The former
   belongs in the ROM-free package; the latter must be imported. Keep game-specific extraction
   metadata separate so staging one game cannot overwrite the other's files.
4. **Connect import, pair selection, and saves.** Build the on-Xbox ROM validation/extraction flow,
   bind the selected OoT edition to the session, and enforce it in archive loading and actual
   randomizer generation/load. The current loader tests are not evidence of combined-game logic.

Acceptance after those prerequisites: both selected games reach gameplay on Xbox, travel works in
both directions in one process, and save/reload retains the same pair and progression. The shared
items, models, enemies, entrances, and logic are subsequent gameplay integration work.

The original packaging recipe remains in `.source-workflow/build-xbox-uwp.yml`, manual-only when
activated. The new MM compile workflow does not replace main's pinned dispatcher or produce an MSIX.

## Validation

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

`tools/codemap.py check` reports the same 23 missing external/pre-existing paths as the unmodified
baseline (including optional decomp submodules); this change introduces zero missing references.

Physical acceptance is pending: install alongside a prior build, create distinct settings/saves
in each, restart both, and verify independent persistence. Source checks do not prove Xbox behavior.

Reference: [Microsoft package identity overview](https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/package-identity-overview).
