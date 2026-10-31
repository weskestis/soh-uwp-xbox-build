# Fusion UWP dependency overlays

Base recipes and support files: microsoft/vcpkg commit
`9e593bb18ea69cc5095e012465dcd675a822ed0d` (MIT).
The upstream archive hashes remain pinned in each portfile. Changes are scoped to the
Fusion dependency installation; no unsupported-port bypass is used.

- FreeType 2.14.3: select its existing FromApp file/mapping functions for native
  `WINAPI_FAMILY_APP`, without pretending the whole project is C++/CX. WindowsStore
  builds disable `FT_CONFIG_OPTION_ENVIRONMENT_PROPERTIES` in the generated options
  header: font initialization keeps FreeType's defaults without importing unsupported
  `getenv`. Programmatic `FT_Property_Set` and desktop environment overrides remain.
- OpusFile 0.12+20221121: UWP-only overlay exports `OP_DISABLE_FILE_IO`; preserve
  memory/callback opening, PCM seeking and decoding. File, fd and HTTP helpers are omitted.
  Both games use `op_open_memory`, `op_pcm_seek`, `op_read`, and `op_free`.

GLEW is intentionally absent from vcpkg installation: its OpenGL dependency imports desktop
SDK libraries. `Zelda3DUwpGlew.cmake` builds the pinned release loader with the packaged Mesa ABI.
The installed upstream copyright files remain owned by the corresponding port recipes.
