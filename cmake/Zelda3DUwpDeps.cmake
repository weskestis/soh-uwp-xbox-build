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

# The packaged Mesa opengl32.dll intentionally uses the Windows OpenGL ABI, so
# the SDK import name is the correct link contract. The app-local DLL supplies
# the implementation at runtime.
if(NOT TARGET OpenGL::GL)
    add_library(OpenGL::GL INTERFACE IMPORTED GLOBAL)
    set_property(TARGET OpenGL::GL PROPERTY INTERFACE_LINK_LIBRARIES opengl32)
endif()
set(OpenGL_FOUND TRUE)
set(OPENGL_FOUND TRUE)
set(OPENGL_gl_LIBRARY opengl32)

# Fast3D's Windows OpenGL path uses GLEW for extension entry points.  Keep it a
# static vcpkg dependency inside the core DLL; Mesa's app-local opengl32.dll is
# still the runtime implementation supplied by the UWP dependency depot.
find_package(GLEW CONFIG REQUIRED)
