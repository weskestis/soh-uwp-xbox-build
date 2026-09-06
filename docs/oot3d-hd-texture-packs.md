# Optional OoT3D HD texture packs

The merged build can read a Citra/Azahar-format OoT3D texture pack directly from its original ZIP.
It does not need the multi-gigabyte pack to be unpacked, converted, uploaded with the source tree, or
embedded in the eventual AppX.

## Henriko Magnifico pack

As of 2026-09-04, the latest public release is **OoT3D 4K v4.0**. Download it only from
[Henriko Magnifico's official OoT3D 4K page](https://www.henrikomagnifico.com/zelda-ocarina-of-time-3d-4k).
That page also links the author's Google Drive mirror. Version 5.0 is currently supporter-only early
access and is not part of this project.

The game, ROMs, keys, and Henriko's archive are not redistributed with this source checkpoint or
with a public package. Keep the downloaded archive as a separate user-owned file.

## Install on the desktop build

1. Start the game once, then open **Settings → Graphics → OoT3D HD Texture Pack**. The status row
   displays the exact writable install directory.
2. Close the game and copy the original downloaded ZIP into that directory. Usually it is the
   app-data directory's `texture-packs` folder. Do not extract the ZIP.
3. Start the game, return to the same settings section, enable **Use OoT3D HD Texture Pack**, and
   select **Rescan Installed Texture Pack** if the archive was copied while the game was open.
4. The status must say **active**, name the pack/version, identify it as a ZIP, and report a nonzero
   texture count. If it says unavailable, the displayed reason is the failure to fix.

Keep only the desired pack in the install folder. If several compatible ZIPs/folders are present,
the first name in deterministic alphabetical order is selected.

## Install through Xbox Device Portal

These steps apply after the separate UWP/AppX target has been built, signed, and tested:

1. Install the new app as its own package and launch it once so its writable LocalState exists.
2. Fully stop the app.
3. In Device Portal's file explorer, open the new package under **LocalAppData**, then open
   **LocalState → texture-packs**.
4. Upload the original Henriko ZIP without extracting it.
5. Launch the app and enable/rescan it under **Settings → Graphics → OoT3D HD Texture Pack**.

The runtime also probes `E:/soh/texture-packs`, but removable-storage access depends on the final
Xbox manifest and must be confirmed on hardware. LocalState is the primary supported location.
The present SDL3 desktop build is not itself a Device Portal package; see the packaging boundary in
[the full-build guide](cursor-fps-v3-oot3d-full-build.md#xboxuwp-packaging-boundary).

## What the toggle changes

- The pack replaces matching authored OoT3D CMB textures by their Citra legacy CityHash64 names.
- It remains independent of **Graphics Mode** and **Alternate Assets**. In Original SoH mode, it
  cannot replace N64 world geometry; only the small HUD paths that intentionally consume OoT3D pack
  art can observe it.
- Turning it on, off, or rescanning during OoT3D gameplay performs a black same-entrance reload.
  The old model, atlas, HUD, and GPU texture caches are discarded together before the new policy is
  used. In Original mode, the switch is immediate.
- Save files, seed settings, and the local SoH Randomizer are not modified. A scene reload resets
  temporary room/actor state in the same way as the Graphics Mode toggle.
- Internal resolution, interpolation FPS, refresh-rate matching, and aspect settings remain separate
  controls. A 4K pack increases texture memory use; Xbox performance is not claimed until the UWP
  hardware pass is complete.

## Compatibility and diagnostics

The loader accepts folders or ZIP/Zip64 archives containing mip-0 files named in Citra's legacy
`tex1_<size>_<16-hex-hash>_<format>_mip0.png` form (and the older form without `_mip0`).
It reads `pack.json`, honors `flip_png_files`, rejects `use_new_hash=true`, ignores nonzero
mips, and validates the US OoT3D title ID `0004000000033500` when title-ID folders are present.

The developer console command `texpack` reports status, source, install directory, pending state,
and override state. It also accepts `texpack on`, `texpack off`, and `texpack rescan`.
`ZELDA3D_TEXPACK=/absolute/path/to/pack.zip` forces a source for diagnostics; setting it to
`off`, `none`, `false`, or `0` disables loading and takes precedence over the saved switch.
