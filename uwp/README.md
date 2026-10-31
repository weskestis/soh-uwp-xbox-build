# Zelda Fusion Xbox/UWP package

This branch starts the separate Zelda Fusion app. Its stable package name is
`WesKestis.ZeldaFusion.UWP`, publisher is `CN=WesKestisZeldaFusion`, and installed display name is
`Zelda Fusion`. The package family differs from every previous Shipwright/Cursor FPS build;
Windows therefore gives it separate `LocalState`, settings, imports, and saves. The existing
`LocalState\soh` directory is inside Fusion's own package, not the older application's directory.
Configure and final package audit both reject another identity or publisher.

Current scope is package isolation only. The inherited runtime still boots the OoT core and needs
pre-extracted archives. MM loading, raw-ROM import on Xbox, shared gameplay, and the combined
randomizer are not implemented by this change. Intended seed modes are exactly **OoT + MM** and
**MQ + MM**, with one OoT edition locked to each seed; see `docs/zelda-fusion-foundation.md`.

The package is ROM-free. It contains the source-generated `soh.o2r`, but never contains `oot.o2r`,
`oot-mq.o2r`, an N64 ROM, `oot3d.3ds`, an extracted RomFS, a save, or an HD texture pack. SDL2 maps
the runtime app directory to `LocalState\soh`; the owner uploads private files there through Xbox
Device Portal after installing the app:

- `oot.o2r` for Normal Quest and/or `oot-mq.o2r` for Master Quest;
- `oot3d.3ds` (decrypted) for OoT3D graphics, or an `oot3d-romfs` directory; and
- optional HD texture packs and mods in the folders created by the app.

GitHub Actions builds a normal x64 Windows `soh_core.dll` with the UWP-aware SDL2/OpenGL profile
(including packaged-DLL Mesa loaders). That profile requires modern `.o2r` archives and leaves
legacy MPQ/`.otr` support plus StormLib's desktop Win32/WinINet imports out of the package,
then loads that core at runtime from this small WindowsStore SDL2/Mesa wrapper. Delaying the core
load lets the wrapper record loader failures instead of dying before its entry point. Every launch
rewrites `LocalState\soh\uwp-boot.log` and flushes each startup boundary immediately, including the
package version, CoreWindow handoff, core DLL load, ABI entry lookup, core call, and caught Windows
exception details. It creates an AppX/MSIX, signs it with
a short-lived build certificate, exports only the public `.cer`, and uploads one Device Portal
artifact. The private `.pfx` is deleted before artifact upload. See `ANDROID-INSTALL.txt` for the
phone-only installation path. Packaging-only workflow commits restore and validate the previously
built core from a stable cache or versioned Actions artifact, so they do not repeat the costly core
compile. A full rebuild occurs only when that preserved core is unavailable or fails validation.

## Safety gates

Configuration requires an x64 Visual Studio Windows toolchain for the self-contained core, an x64
UWP toolchain for this wrapper, the pinned SDL2/libuwp/Mesa dependency tree, runtime assets, and
`soh.o2r`. `ZELDA3D_UWP_ALLOW_INCOMPLETE_RENDERER` remains off by default until the remote build and
Xbox hardware run are validated. CI enables it only for explicit compile/package experiments; that
output is not called device-verified until it has launched and rendered correctly on Xbox.

All five UWP tile/logo resources are deterministic sizes of the approved Master Quest Flames art.
The unpacked-package audit checks their dimensions and exact SHA-256 values so the stock SoH icon
cannot silently return.

No certificate private key or private game data belongs in this repository or its Actions artifact.
