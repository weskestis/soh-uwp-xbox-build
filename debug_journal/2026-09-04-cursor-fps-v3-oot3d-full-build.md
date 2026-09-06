# 2026-09-04 — CURSOR FPS V3 + OoT3D full-build integration

## User request and source identities

The user requested one toggleable SOH CURSOR FPS V3 build with the complete OoT3D graphics layer,
the complete SoH settings surface, and a Device Portal install rather than a loose texture dump.

The recovered V3 package is `SoH-UWP-Cursor-FPS-FRESH3-XBOX.zip`. Its AppX identity is
`ShipwrightCursorFPSFresh3DualOOTMQ`, version `9.1.0.0`, x64. The source embedded in that delivery
was commit `d5cad5fa` with libultraship `ec32b930`. The customized SDL2 proxy's SHA-256 is
`769afb0e72d1d743616e27a870afebfd6366a5385bfad579bcaa235d4045ec35`.

The supplied US cartridge image is a retail encrypted CCI (its NCCH no-crypto flag is clear and the
RomFS region is ciphertext). A complete owner-derived RomFS had already been extracted for the
texture work. Runtime assets come from that extracted directory; neither the image nor extracted
Nintendo data is added to the repository or source release artifact.

## V3 behavior recovered and ported

The proxy customized only event pumping, controller button/axis reads, and swap-window overlay
drawing. Its behavior was reproduced in the current SDL3 owner
`libultraship/src/fast/backends/cursor_fps_v3.cpp`:

- initial cursor mode off and non-persistent;
- L3+R3 held 350 ms toggles once per release cycle;
- L3/R3 always suppressed from the game;
- while active, A and right-stick X/Y are additionally suppressed;
- 6500 deadzone, 1150 px/s maximum, 1..50 ms movement delta clamp, and minimum nonzero ±1 step;
- A synthesizes held-capable left mouse down/up;
- enable rumble `0x5000/0x5000` for 120 ms; disable `0x2800/0x2800` for 70 ms;
- exact 1600 ms black toast and 5x7 white bitmap glyphs; exact crosshair geometry;
- relative mouse state and window mouse rectangle are restored on disable only when they existed
  before enable.

The SDL3 port ticks once per backend event pump rather than interposing every external
`SDL_PumpEvents` call. That is the only normal-path scheduling distinction. Disconnect safety
deliberately releases a synthetic held click, which the proxy did not do.

View/Back and F1 own the restored complete SoH ImGui menu. Start and Escape own the compact RmlUi
menu. The surfaces close each other, have separate ControlDeck blocker IDs, arbitrate pointer
capture, and disable ImGui gamepad navigation while A is acting as a synthetic mouse click.

## Graphics mode transaction

`gSettings.GraphicsMode` is the persistent selection. A change during gameplay does not mutate
the active render/collision mode in place. `Zelda3D_RequestGraphicsMode` records the request;
`Zelda3D_ProcessGraphicsModeRequest` starts a same-entrance black transition when no other
transition owns the state; and `Zelda3D_ApplyPendingGraphicsMode` commits only at the beginning of
the next `Play_Init`. Original and OoT3D state therefore cannot coexist inside one live scene.

The normal Alternate Assets CVar is untouched and remains independent.

## Asset-source and fallback contract

The runtime resolves either `oot3d-romfs` or a decrypted `oot3d.3ds` from authoritative environment
overrides, Xbox `E:/soh`, the app-data directory, app bundle, then current directory. The `CtrRom`
asset interface now supports both IVFC-backed CCI files and owner-extracted loose RomFS files. A
candidate must contain scene, Link, and environment RomFS anchors. The settings page displays the
resolved path or exact error and refuses OoT3D selection when the source is unavailable. This loose
RomFS support was added after the first live launch correctly rejected the encrypted supplied CCI.

The old replacement path frequently decided to suppress the N64 draw before proving that the
replacement payload was usable. This pass added preflight contracts for geometry, decoded textures,
and authored CSAB clips. Whole composites (sky layers, sun/moon/halos, Volvagia multipart models)
preflight every required piece before claiming the draw. Rooms, Link, generic skinned actors,
Navi/fairy wings, fish, butterflies, items, props, structures, title logo/effects, and boss parts
fall back to their original draw on an incomplete replacement.

Inventory checks against the actual OoT3D source found all 101/101 scene headers and all 329/329
literal scene references used by the port.

## Settings audit

The stripped hidden-menu tree was restored: Settings, Enhancements, Randomizer, Network, and Dev
Tools are built and initialized, including registered menu-init callbacks and presets. Gameplay,
save, randomizer, controller, audio, HUD, and shared-runtime settings remain active in both graphics
modes. N64 display-list/model options and texture filtering are explicitly described as applying to
Original/fallback assets when an authored CMB owns the replacement.

Known SDL3 no-ops are not presented as working:

- MSAA, runtime VSync selection, and multi-window viewports are disabled with reasons;
- Fix Vanishing Paths is disabled because the SDL3 GPU depth-bias mode is fixed;
- the legacy OpenGL Gfx Debugger page is marked unavailable;
- the SDL_net compatibility shim declares networking unavailable, shows one explanatory Network
  page, suppresses stale auto-connect CVars, and does no fake connection attempt.

## Optional texture-pack transaction

The former loader only searched an unpacked developer `textures/` directory. The packaged runtime
now discovers a user-owned pack under the app-data `texture-packs` directory (plus explicit
diagnostic/future-Xbox roots), and libzip reads original ZIP/Zip64 archives in place. The loader
accepts legacy Citra mip-0 hashes, reads the pack name/version and flip policy from `pack.json`,
rejects `use_new_hash=true` and foreign-title-only packs, and bounds archive entries, manifest
size, encoded PNG size, and decoded dimensions.

`gSettings.OoT3DTexturePackEnabled` is independent from Graphics Mode and Alternate Assets.
Changing it while OoT3D owns a live scene queues a same-entrance fade and commits in `Play_Init`;
changing it in Original mode is immediate. Every pack-dependent cache observes the loader
generation: model CPU data and renderer uploads are evicted explicitly, while the atlas and
runtime-built HUD sources invalidate lazily on their next use. A rescan follows the same
transaction, so no old and new pack textures coexist in a live OoT3D scene.

The full settings page exposes the toggle, live source/version/count status, install directory, and
rescan action. The REPL mirrors it with `texpack [on|off|rescan]`. Save data and local-randomizer
state are not part of this transaction.

## Independent Normal/Master Quest provisioning

Startup previously used one `VanillaArchiveExists()` predicate that returned true when *either*
`oot.o2r` or `oot-mq.o2r` existed. It also returned after the first successful candidate. That had
two consequences: leaving a Normal ROM beside an existing archive regenerated it every boot, while
one generated edition prevented the other candidate from ever running.

The core now tracks Normal and Master Quest readiness independently. It validates every candidate
needed to fill a missing edition, preserves an existing matching archive, continues until both are
ready, and performs an immediate no-scan return when both already exist. The Python bootstrap gained
the explicit `ZELDA3D_OOT_MQ_ROM` input, identifies MQ from the same normalized header CRCs as SoH,
and stages Normal and MQ links independently.

The supplied inputs were identified as NTSC-U 1.1 Normal (normalized header CRC `D43DA81F`) and PAL
Master Quest (`1D4136F3`). A cold live gate moved both derived archives aside, exposed both inputs,
and launched once. The core generated `oot-mq.o2r` (35,352 entries) and `oot.o2r` (38,526 entries),
ZIP-tested both, loaded both in the same ArchiveManager, enabled the OoT3D source, and generated seed
`full-build-dual-rom-mq-gate-20260904`. The spoiler's `masterQuestDungeons` is the complete list of
twelve dungeons and its settings record `MQ Dungeon Count: 12`. Teardown released 694 GPU handles
with zero duplicates and left 0/2 run-scoped pointers.

The first repeat-extraction comparison then found that ZIP-valid archives with the same entry
counts did not have identical resource payloads. There were three independent causes in the
vendored ZAPDTR exporter: recursive filesystem discovery order was unspecified, primitive skin
fields could be serialized before initialization, and `SetMesh::data` was never assigned from the
parsed command. The last defect was the remaining measured one: exactly one differing byte in each
of 483 Normal and 456 MQ scene resources. ZAPDTR now sorts discovery, zero-initializes the serialized
skin primitives, and assigns `SetMesh::data = cmdArg1`.

Two new isolated cold boots, each using a private app/core directory with no archives, then
generated and ZIP-tested both editions. Normal had 38,526 entries and payload-manifest SHA-256
`64e041a30df127b20c4e7892a20b1690b0cae341f55895695dfc6a3e3cdff159` in both runs. Master Quest had
35,352 entries and payload-manifest SHA-256
`1d72e6609e52558f04adce9675694265f4220af4fae9fb193e5757ad99b2be58` in both runs. The manifest
includes each filename, size, CRC32, and decompressed per-entry SHA-256; both archives had zero
duplicate names. The second run also generated
`full-build-deterministic-dual-rom-mq-final-20260904` with all twelve MQ dungeons, reached scene
`0x44` at entrance `0x6e` with the OoT3D source ready, released 608 GPU handles with zero duplicate
releases, and left 0/2 run-scoped pointers.

## Verification

- optimized `soh_core` build: PASS after all integration/fallback changes;
- complete optimized `zelda3d_app` build: PASS (1378/1378 actions);
- `python3 tools/test_cursor_fps_v3_full_build.py`: 18/18 PASS;
- `python3 tools/test_texture_pack_loader.py`: 1/1 PASS (folder/ZIP, manifest, orientation,
  old/current legacy names, validation failures, decode failure, toggle generation);
- ROM provisioning 13/13, launcher bootstrap 10/10, and launcher build 17/17 regressions: PASS;
- live SDL3 GPU launch on offscreen/lavapipe: PASS, including source `ready=1`;
- live same-process Original -> OoT3D -> Original switch: PASS, with both transitions reloading
  entrance `0xEE`; the OoT3D pass loaded collision, a 21-group/22-texture room, Link's
  55-group/44-texture model, facial frames, skinned NPCs, props, and authored animations;
- live compact/full settings-menu arbitration and full settings draw: PASS;
- live normal-discovery ZIP pack: PASS; status named the fixture and reported one indexed texture;
  live OoT3D `off → on → rescan` each queued, reloaded, and committed; return to Original mode and
  full teardown passed. This used a generated 2D fixture, not the third-party archive, which was not
  redistributed;
- live dual-ROM cold extraction: PASS; two independent clean app directories each generated and
  loaded both missing archives, and all 73,878 decompressed resource payloads matched;
- live local Randomizer in OoT3D mode with all 12 dungeons MQ: PASS; final seed hash `672743998` and
  twelve-entry `masterQuestDungeons` spoiler recorded;
- orderly renderer/core shutdown: PASS (0 run-scoped pointers left; 851 GPU handles released once);
- `tools/re_frontier.py check`: PASS;
- `git diff --check`: PASS after the final documentation/evidence update;
- `tools/codemap.py check`: the new entries pass; the repository still reports four unrelated,
  pre-existing stale tool paths (`../shared/re-harness/tools/info.py`,
  `tools/build_appimage_release.py`, `tools/build_mm_custom_archive.py`, and
  `tools/package_appimage.py`).

## Xbox/UWP blocker

The donor now uses SDL3 GPU, while SDL3 dropped WinRT/UWP. The recovered V3 AppX uses an SDL2/UWP
target and is signed; changing its payload invalidates both signature and block map, and its private
key is unavailable. Linux cannot supply the Windows SDK `MakePri`/`MakeAppx`/`SignTool` chain or a
real Xbox test.

Therefore the merged portable core can be completed and verified here, but a legitimate Device
Portal artifact still needs a separate SDL2/UWP window/renderer target, Windows SDK build/signing,
and Xbox validation. No unsigned or renamed repack is accepted as delivery evidence.
