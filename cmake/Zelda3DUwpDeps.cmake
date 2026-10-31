# Import the SDL2/Mesa dependency depot used by the Xbox UWP profile. The
# depot is pinned and fetched by CI; keeping it outside this repository avoids
# committing third-party binaries while still giving normal CMake targets to
# libultraship and SoH.

set(ZELDA3D_UWP_DEPS_ROOT "" CACHE PATH
    "UWP dependency depot containing x64/include, x64/lib, and x64/bin")

if(NOT IS_DIRECTORY "${ZELDA3D_UWP_DEPS_ROOT}/x64")
    message(FATAL_ERROR
        "ZELDA3D_UWP_DEPS_ROOT must point to the pinned uwp-dep checkout (missing x64 directory): "
        "${ZELDA3D_UWP_DEPS_ROOT}")
endif()

set(ZELDA3D_UWP_DEPS_ARCH "${ZELDA3D_UWP_DEPS_ROOT}/x64" CACHE INTERNAL "UWP x64 dependency root")
set(zelda3d_uwp_sdl_dll "${ZELDA3D_UWP_DEPS_ARCH}/bin/SDL2.dll")
set(zelda3d_uwp_sdl_lib "${ZELDA3D_UWP_DEPS_ARCH}/lib/SDL2.lib")
foreach(required_path IN ITEMS
        "${zelda3d_uwp_sdl_dll}"
        "${zelda3d_uwp_sdl_lib}"
        "${ZELDA3D_UWP_DEPS_ARCH}/include/SDL2/SDL.h")
    if(NOT EXISTS "${required_path}")
        message(FATAL_ERROR "Pinned UWP dependency is missing: ${required_path}")
    endif()
endforeach()

if(NOT TARGET SDL2::SDL2)
    add_library(SDL2::SDL2 SHARED IMPORTED GLOBAL)
    set_target_properties(SDL2::SDL2 PROPERTIES
        IMPORTED_LOCATION "${zelda3d_uwp_sdl_dll}"
        IMPORTED_IMPLIB "${zelda3d_uwp_sdl_lib}"
        INTERFACE_INCLUDE_DIRECTORIES
            "${ZELDA3D_UWP_DEPS_ARCH}/include/SDL2;${ZELDA3D_UWP_DEPS_ARCH}/include")
endif()
set(SDL2_INCLUDE_DIRS
    "${ZELDA3D_UWP_DEPS_ARCH}/include/SDL2;${ZELDA3D_UWP_DEPS_ARCH}/include")
set(SDL2_FOUND TRUE)

# Link against the exact app-local Mesa ABI, never the desktop SDK OpenGL lib.
# tools/ci/fusion_mm_windows.ps1 generates this import library from DLL exports.
set(zelda3d_uwp_mesa_dll "${ZELDA3D_UWP_DEPS_ARCH}/bin/opengl32.dll")
set(zelda3d_uwp_mesa_lib "${ZELDA3D_UWP_DEPS_ARCH}/lib/fusion-mesa-opengl32.lib")
foreach(required_path IN ITEMS "${zelda3d_uwp_mesa_dll}" "${zelda3d_uwp_mesa_lib}")
    if(NOT EXISTS "${required_path}")
        message(FATAL_ERROR "Generate the pinned Mesa import library first: ${required_path}")
    endif()
endforeach()
if(NOT TARGET OpenGL::GL)
    add_library(OpenGL::GL SHARED IMPORTED GLOBAL)
    set_target_properties(OpenGL::GL PROPERTIES
        IMPORTED_LOCATION "${zelda3d_uwp_mesa_dll}"
        IMPORTED_IMPLIB "${zelda3d_uwp_mesa_lib}")
endif()
set(OpenGL_FOUND TRUE)
set(OPENGL_FOUND TRUE)
set(OPENGL_gl_LIBRARY "${zelda3d_uwp_mesa_lib}")
include(${CMAKE_CURRENT_LIST_DIR}/Zelda3DUwpGlew.cmake)
