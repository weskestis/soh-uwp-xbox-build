# Zelda Fusion Xbox/UWP test package

Fusion uses identity `WesKestis.ZeldaFusion.UWP`, publisher `CN=WesKestisZeldaFusion`,
and display name `Zelda Fusion`. It owns separate LocalState, settings and saves
from previous Shipwright/Cursor FPS installations.

The diagnostic launches one original N64 game per process. Four exact startup
routes are accepted through `LocalState/soh/fusion-startup.txt`; see
[ANDROID-INSTALL.txt](ANDROID-INSTALL.txt) and the included `StartupSelections`.
Default is OoT. Invalid or unreadable selection stops startup, and the selected
private archive must be readable before loading a core. This does not yet enforce
MQ layouts or combined randomizer logic, bind saves to a pair, or switch engines
inside a running process. Intended combined modes remain OoT+MM and MQ+MM only.

The ROM-free package includes `soh.o2r` and `2ship.o2r`. Upload owner game archives
through Xbox Device Portal to Fusion's own `LocalState/soh/oot.o2r`,
`LocalState/soh/oot-mq.o2r`, or `LocalState/2ship/mm.o2r`. Raw-ROM conversion is not
part of this test. No 3DS image is required. Keep Original SoH graphics selected
and do not import settings from abandoned 3DS builds.

`.github/workflows/fusion-test-package.yml` owns diagnostic packaging. Exact
native core runs, artifact names, source SHAs and payload hashes are validated by
`tools/fusion_package_inputs.py`; engine and wrapper revisions remain distinct in
`BUILD.json`. The old cache-based `uwp_windows.ps1` is not used. The separate
WindowsStore wrapper uses SDK 19041 and the dynamic Store CRT, then packages and
signs an MSIX with its own ephemeral certificate. Only the public certificate is
shipped; the private key is deleted. The Microsoft-signed retail x64 VCLibs AppX
is required and included under `Dependencies/x64`.

`tools/fusion_package_audit.py` validates exact identity/version/architecture,
activation, framework metadata, private-file exclusion, every staged payload hash,
and native imports against the packaged DLL/framework exports. The five approved
icons retain exact size/hash checks. System API and dynamic graphics-driver
behavior still require physical Xbox testing; packaging checks do not certify them.

Every launch rewrites `LocalState/soh/uwp-boot.log`, including when MM is selected.
Per-line flushing records selection, missing archives, core load/ABI failures,
and caught Windows exceptions. Failures before the SDL callback cannot write it.

`ZELDA3D_UWP_ALLOW_INCOMPLETE_RENDERER` stays off by default. This explicitly
experimental test workflow enables it for device validation. A green native
build is not proof of successful Xbox startup, rendering, input, audio or saves.
