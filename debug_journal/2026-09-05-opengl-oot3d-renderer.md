# Current-feature OoT3D renderer on SDL2/OpenGL

Date: 2026-09-05
Task: KANBAN #212, Xbox/UWP renderer prerequisite

## Result

The opt-in `ZELDA3D_SDL2_OPENGL` profile now owns a real OoT3D native-model and native-HUD renderer.
It is not a restored copy of the historical `soh3d_gl.cpp`: the old queue, shadow, AO, and pose
owners remain retired. The new backend consumes the same current model/provider, interpolated pose,
material override, lighting, fog, and fixed shader contracts as SDL3 GPU.

This closes the prior Linux renderer-symbol and no-op gap. It does **not** certify an AppX. The
WindowsStore compile/link, signing, Device Portal install/launch, and physical Xbox runtime pass are
still required.

## Owners

- `include/fast/backends/zelda3d_opengl.h`: narrow backend API.
- `zelda3d_opengl_resources.cpp`: shader, VAO/UBO, texture, model-cache, and eviction ownership.
- `zelda3d_opengl_pass.cpp`: synchronous draw, UBO packing, fixed-function state, diagnostics, and
  overlay-depth clear.
- `zelda3d_opengl_state.cpp`: complete state capture/restore boundary around native GL work.
- `zelda3d_opengl_hud.cpp`: ordered native HUD quad runs and texture cache.
- `zelda3d_opengl.cpp`: thin C diagnostics ABI.
- `zelda3d_submission.cpp` and `zelda3d_model_provider.cpp`: backend dispatch only.
- `gfx_opengl.cpp`: tears down model and HUD GL objects before the context dies.

Renderer-owned draw-isolation globals moved from the SDL3 pass into
`zelda3d_instrumentation.cpp`, so both backends provide one definition. The fixed CMB shader source
builder is now backend-neutral; glslang/SPIR-V compilation remains compiled only for SDL3 GPU.

## Current rendering contract

The OpenGL program is generated from `Zelda3DSdl3GpuShaders::BuildSources`, then adapted to desktop
GLSL 1.40. It retains:

- 64 bone matrices and interpolated skin poses;
- position, normal, color, UV0, UV1, UV2, bone-id, and weight attributes;
- three independently filtered/wrapped textures, including authored mip chains;
- generic six-stage packed PICA TEV and six material constants;
- material texture/constant/UV overrides and visible-mesh masks;
- vertex and fragment lighting gates, two scene lights, sphere-normal overrides, both fog paths,
  alpha test, tint/alpha, and coordinator mappings;
- per-group depth, blend, cull, winding, and polygon-offset state;
- synchronous overlay depth reset at the interpreter marker.

Native models draw synchronously after the interpreter flushes Fast3D, preserving the existing
emission order. Every native operation uses an isolated VAO and restores program, VAO, array/index
and uniform-buffer bindings, indexed UBO bindings, texture units 0--2 plus active unit, blend state,
depth state, cull/winding, viewport/scissor, polygon offset, color mask, clear depth, and unpack
alignment. Fast3D's cached state therefore remains valid after the handoff.

The HUD path batches only adjacent runs with the same texture/wrap mode. It supports plain modulate
and both ENV combiner modes, flushes at `G_ZELDA3D_HUDFLUSH`, and restores GL state after each flush.

## Verification

SDL2/OpenGL optimized OoT core:

```sh
CPATH=/workspace/scratch/69c2326d4f52/work/deps-prefix-sdl2/usr/include/x86_64-linux-gnu \
  ninja -C /workspace/scratch/69c2326d4f52/work/build-linux-full -j9 soh_core
```

Result: `libultraship.so` and `soh/libsoh_core.so` linked successfully after all new OpenGL owners
compiled.

The redistributable live test builds the production shader/resource/pass/HUD sources directly:

```sh
c++ -std=c++17 -O2 -DENABLE_OPENGL=1 \
  -IShipwright/libultraship/include -IShipwright/libultraship/src/fast \
  tools/zelda3d_opengl_smoke.cpp \
  Shipwright/libultraship/src/fast/zelda3d_opengl_state.cpp \
  Shipwright/libultraship/src/fast/zelda3d_opengl_resources.cpp \
  Shipwright/libultraship/src/fast/zelda3d_opengl_pass.cpp \
  Shipwright/libultraship/src/fast/zelda3d_opengl_hud.cpp \
  Shipwright/libultraship/src/fast/zelda3d_sdl3gpu_shaders.cpp \
  -lEGL -lOpenGL -o scratch/zelda3d_opengl_smoke
LIBGL_ALWAYS_SOFTWARE=1 scratch/zelda3d_opengl_smoke
```

Mesa supplied a real OpenGL 4.5 compatibility context. The fixed model shader compiled and linked,
a provider-backed triangle produced the expected red center pixel, the sentinel program and active
texture were restored, the HUD then produced the expected green center pixel, teardown completed,
and `glGetError()` remained `GL_NO_ERROR`.

Additional results:

- Clean default SDL3 profile (`ZELDA3D_SDL2_OPENGL=OFF`): 2205/2205 build steps completed and
  `soh/libsoh_core.so` linked; a second incremental pass after the shared-UBO comment/CMake update
  completed 40/40. Existing FetchContent sources were reused offline because GitHub access is
  blocked in this sandbox.
- `ldd -r build-linux-full/libultraship/src/libultraship.so`: exit 0, no undefined-symbol report.
- `ldd -r build-linux-full/soh/libsoh_core.so`: exit 0, no undefined-symbol report.
- SDL2 launcher `--probe-cores`: OoT and MM both load under `RTLD_NOW | RTLD_LOCAL`; ABI 1; all
  three checked decomp symbols are private; 2/2 cores loaded.
- `python3 tools/test_uwp_package_contract.py`: 5/5 PASS.
- `python3 tools/test_cursor_fps_v3_full_build.py`: 18/18 PASS.
- `git diff --check`: PASS.
- `tools/verify_clang.py --format-only --files ...` stops in its repository-wide structure
  preflight on pre-existing oversized files outside this change. Every new renderer/test file is
  below the 1200-line limit (the largest is `zelda3d_opengl_pass.cpp` at 498 lines).

## Still unproved

- A WindowsStore build of the SDL2/OpenGL core and its Mesa/GLEW linkage.
- AppX resource generation and authorized signing.
- Device Portal installation and launch on Xbox.
- Physical Xbox controller/audio/display behavior and full OoT3D gameplay inspection on this new
  backend.

The `ZELDA3D_UWP_ALLOW_INCOMPLETE_RENDERER` guard remains OFF by default. It now describes the
remaining WindowsStore/Xbox validation gap, not a Linux no-op renderer; enable it only for the next
wrapper/link experiment, never for a claimed Device Portal release.

## Next

Build the SDL2/OpenGL core with the WindowsStore toolchain and supplied UWP dependency tree, resolve
any platform-specific GL loader/API differences, then perform the signed package and device gates in
order.
