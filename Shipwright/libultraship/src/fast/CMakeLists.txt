#=================== Top-Level ===================
file(GLOB Source_Files__TopLevel RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS "*.cpp")

if(ZELDA3D_SDL2_OPENGL)
    list(FILTER Source_Files__TopLevel EXCLUDE REGEX "zelda3d_.*sdl3gpu.*\\.cpp$")
    # The fixed CMB shader templates are backend-neutral. Keep their source builder in the
    # SDL2/OpenGL profile; only the glslang/SPIR-V compiler half is SDL3-GPU-specific.
    list(APPEND Source_Files__TopLevel "zelda3d_sdl3gpu_shaders.cpp")
else()
    list(FILTER Source_Files__TopLevel EXCLUDE REGEX "zelda3d_opengl.*\\.cpp$")
endif()

source_group("" FILES ${Source_Files__TopLevel})
target_sources(libultraship PRIVATE ${Source_Files__TopLevel})

#=================== Debug ===================

file(GLOB Source_Files__Debug RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS "debug/*.cpp")

source_group("debug" FILES ${Source_Files__Debug})
target_sources(libultraship PRIVATE ${Source_Files__Debug})

#=================== Resource ===================

file(GLOB Source_Files__Resource__Types RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS "resource/type/*.h" "resource/type/*.cpp")
source_group("resource/type" FILES ${Source_Files__Resource__Types})

file(GLOB Source_Files__Resource__Factories RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS "resource/factory/*.h" "resource/factory/*.cpp")
source_group("resource/factory" FILES ${Source_Files__Resource__Factories})

target_sources(libultraship PRIVATE ${Source_Files__Resource} ${Source_Files__Resource__Types} ${Source_Files__Resource__Factories} ${Source_Files__Resource__Archive})

#=================== Graphic ===================

# SDL3 GPU is the only renderer (P4): the only backends left are the SDL3 window backend
# (gfx_sdl3.cpp) and the SDL3 GPU rendering API (gfx_sdl3gpu.cpp). The GL/DX11/Metal/Vulkan backend
# sources were deleted, so no per-platform filtering is needed.
file(GLOB Source_Files__Graphic RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS "backends/*.cpp")

if(ZELDA3D_SDL2_OPENGL)
    list(FILTER Source_Files__Graphic EXCLUDE REGEX "backends/gfx_sdl3.*\\.cpp$")
else()
    list(FILTER Source_Files__Graphic EXCLUDE REGEX "backends/gfx_opengl.*\\.cpp$")
    list(FILTER Source_Files__Graphic EXCLUDE REGEX "backends/gfx_sdl2.*\\.cpp$")
endif()

source_group("graphic" FILES ${Source_Files__Graphic})
target_sources(libultraship PRIVATE ${Source_Files__Graphic})
