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

Implemented here: independent package identity, branding, signing defaults, and rejection of
legacy identities. The inherited UWP wrapper still loads only `soh_core.dll` and requires
pre-extracted archives. MM loading, Xbox ROM extraction, world travel, shared gameplay, and the
combined randomizer remain future implementation. No signed Fusion MSIX or Xbox result is claimed.

Next milestone: integrate the two-engine runtime, boot both selected games on Xbox, then test
travel in both directions and save/reload with the selected pairing preserved.

The native recipe remains in `.source-workflow/build-xbox-uwp.yml`, manual-only when activated.
This branch does not replace the existing main branch's pinned dispatcher or start a native build.

## Validation

`python3 tools/test_uwp_package_contract.py`: **19/19 PASS** on 2026-10-31. The tests populate the
checked-in manifest from its CMake defaults, accept Fusion and later Fusion versions, reject four
old package names, reject a matching legacy signing publisher, and reject a publisher mismatch.
Inherited staging, icon, private-file, and compiled bootstrap checks remain covered.

`tools/codemap.py check` reports the same 23 missing external/pre-existing paths as the unmodified
baseline (including optional decomp submodules); this change introduces zero missing references.

Physical acceptance is pending: install alongside a prior build, create distinct settings/saves
in each, restart both, and verify independent persistence. Source checks do not prove Xbox behavior.

Reference: [Microsoft package identity overview](https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/package-identity-overview).
