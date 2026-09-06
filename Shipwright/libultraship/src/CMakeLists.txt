include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/cvars.cmake)

if(ZELDA3D_UWP)
    # Xbox ships one OoT core.  Fold the engine into that DLL so all static
    # x64-windows dependencies and the game share one CRT/heap boundary, as in
    # the established SoH UWP port.  Desktop's two-core launcher still needs a
    # single shared libultraship instance and keeps the normal SHARED target.
    add_library(libultraship STATIC)
else()
    add_library(libultraship SHARED)
endif()
set_target_properties(libultraship PROPERTIES PREFIX "")
set_property(TARGET libultraship PROPERTY CXX_STANDARD 20)
# Need to set C11 for libgfxd (_Alignas)
set_property(TARGET libultraship PROPERTY C_STANDARD 11)
set_target_properties(libultraship PROPERTIES ENABLE_EXPORTS TRUE)
set_target_properties(libultraship PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../include)
option(USE_OPENGLES "Enable GLES3" OFF)
option(GFX_DEBUG_DISASSEMBLER "Enable libgfxd" OFF)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
use_props(${PROJECT_NAME} "${CMAKE_CONFIGURATION_TYPES}" "${DEFAULT_CXX_PROPS}")
if(ZELDA3D_UWP)
    set_target_properties(libultraship PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
endif()

#=================== Top-Level ===================

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ship/install_config.h.in ${CMAKE_CURRENT_BINARY_DIR}/ship/install_config.h @ONLY)

if(ENABLE_SCRIPTING)
    file(GLOB SCRIPT_PUB_KEYS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../keys/script/*.pem")
    set(EMBEDDED_KEYS_HEADER ${CMAKE_CURRENT_BINARY_DIR}/ship/DefaultKeys.h)
    set(GENERATE_KEYS_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/../tools/generate_keys_header.py)
    get_filename_component(EMBEDDED_KEYS_DIR ${EMBEDDED_KEYS_HEADER} DIRECTORY)

    add_custom_command(
        OUTPUT ${EMBEDDED_KEYS_HEADER}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${EMBEDDED_KEYS_DIR}
        COMMAND Python3::Interpreter ${GENERATE_KEYS_SCRIPT}
                ${EMBEDDED_KEYS_HEADER}
                ${SCRIPT_PUB_KEYS}
        DEPENDS ${GENERATE_KEYS_SCRIPT}
                ${SCRIPT_PUB_KEYS}
        COMMENT "Generating DefaultKeys.h from all keys in /keys/script/"
        VERBATIM
    )

    add_custom_target(GenerateScriptKeys DEPENDS ${EMBEDDED_KEYS_HEADER})
    add_dependencies(libultraship GenerateScriptKeys)
endif()

#=================== Fast ===================

add_subdirectory("fast")

#=================== LUS ===================

add_subdirectory("libultraship")

#=================== Ship ===================

add_subdirectory("ship")

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/ship/utils/AppleFolderManager.mm PROPERTIES COMPILE_FLAGS -fno-objc-arc)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/ship/utils/macUtils.mm PROPERTIES COMPILE_FLAGS -fno-objc-arc)
endif()

#=================== Packages & Includes ===================

target_include_directories(libultraship
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${INCLUDE_DIR} ${ADDITIONAL_LIB_INCLUDES}
)

#=================== Linking ===================
# PRIVATE, not PUBLIC. StormLib is a static library and libultraship is a SHARED one, so a PUBLIC
# link whole-archived a separate copy of the MPQ code -- and of its globals -- into every consumer:
# libultraship.so, libsoh_core.so, libmm_core.so and the zelda3d launcher, four in one process.
# AddressSanitizer calls that what it is (`odr-violation: global 'DistBits' at pklib/explode.c:34`),
# and which copy the loader binds is not something the four agree on. Nothing outside libultraship
# references StormLib: `SFile*` and `StormLib.h` appear only in ship/resource/archive/OtrArchive.cpp
# (OTRExporter is a separate tool that fetches its own). Same shape as the dr_libs fix in the root
# CMakeLists -- a dependency used by one target had been declared where both games picked it up.
if(INCLUDE_MPQ_SUPPORT)
    if (NOT MSVC AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
        target_link_libraries(libultraship PRIVATE "$<LINK_LIBRARY:WHOLE_ARCHIVE,storm>")
    else()
        target_link_libraries(libultraship PRIVATE storm)
    endif()
endif()

# Dear ImGui is a real library again (see cmake/dependencies/common.cmake), so the no-op stub that
# stood in for it is gone. PUBLIC because the games' own dev-tool windows include imgui.h and must
# inherit both the include dirs and IMGUI_USER_CONFIG.
if(ZELDA3D_SDL2_OPENGL)
    target_link_libraries(libultraship PUBLIC SDL2::SDL2 ImGui OpenGL::GL)
    if(ZELDA3D_WINDOWS_FAMILY)
        target_link_libraries(libultraship PUBLIC GLEW::GLEW)
    endif()
    target_compile_definitions(libultraship PRIVATE ENABLE_OPENGL ZELDA3D_USE_SDL2=1)
else()
    target_link_libraries(libultraship PUBLIC SDL3::SDL3 ImGui)
endif()

# OpenGL was formerly linked transitively via the (removed) ImGui target; a couple of renderer
# paths (e.g. gfx_sdl3.cpp frame-dump glReadPixels) still need it. find_package(OpenGL) is in
# common.cmake.
if(OPENGL_opengl_LIBRARY)
    target_link_libraries(libultraship PUBLIC ${OPENGL_opengl_LIBRARY})
elseif(OPENGL_gl_LIBRARY)
    target_link_libraries(libultraship PUBLIC ${OPENGL_gl_LIBRARY})
endif()

target_link_libraries(libultraship PUBLIC RmlUi::RmlUi)

target_link_libraries(libultraship PRIVATE stb)

if (GFX_DEBUG_DISASSEMBLER)
    target_link_libraries(libultraship PRIVATE libgfxd)
endif()

find_package(libzip REQUIRED)
# PUBLIC, not PRIVATE. As a STATIC library the distinction did not matter -- PRIVATE deps still
# reached the final executable link. As a SHARED library it does: soh.elf then failed with
#   libultraship.so: undefined reference to symbol 'zip_file_add'
#   /usr/lib64/libzip.so.5: DSO missing from command line
# because ld will not resolve a DSO's reference through a library the consumer never names.
target_link_libraries(libultraship PUBLIC libzip::zip)

find_package(nlohmann_json REQUIRED)
target_link_libraries(libultraship PUBLIC nlohmann_json::nlohmann_json)

find_package(tinyxml2 REQUIRED)
target_link_libraries(libultraship PUBLIC tinyxml2::tinyxml2)

find_package(spdlog REQUIRED)
target_link_libraries(libultraship PUBLIC spdlog::spdlog)

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    target_link_libraries(libultraship PRIVATE Threads::Threads)
    find_library(COCOA Cocoa)
    target_link_libraries(libultraship PRIVATE ${COCOA})

endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
    find_Library(OSX_FOUNDATION Foundation)
    find_Library(OSX_AVFOUNDATION AVFoundation)
    find_library(METAL Metal)
    find_library(QUARTZCORE QuartzCore)
    find_library(COREAUDIO CoreAudio)
    find_library(AUDIOTOOLBOX AudioToolbox)
    target_link_libraries(libultraship PRIVATE ${OSX_FOUNDATION} ${OSX_AVFOUNDATION} ${METAL} ${QUARTZCORE} ${COREAUDIO} ${AUDIOTOOLBOX} ${CMAKE_DL_LIBS})
endif()

if (USE_OPENGLES)
    if (CMAKE_SYSTEM_NAME STREQUAL "Android")
        find_library(OPENGLES_LIBRARY GLESv3)
        target_link_libraries(libultraship PRIVATE GLESv3)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
        target_link_libraries(libultraship PRIVATE OpenGL::GLES2)
    else()
        find_library(OPENGLES_LIBRARY GLESv2)
        target_link_libraries(libultraship PRIVATE GLESv2)
    endif()
endif()

# ----- SDL3 GPU backend (normal/default desktop renderer) -----
# The default Fast3D backend renders through SDL3's GPU API (FAST3D_SDL_GPU). The separate opt-in
# SDL2/OpenGL profile above exists for Xbox/UWP. SDL3's per-combiner shaders are generated as GLSL at
# runtime and compiled to SPIR-V with glslang (SDL3 GPU consumes SPIR-V on its Vulkan/Metal/D3D12
# driver), so glslang stays a hard dependency when this profile is selected; the Vulkan loader /
# MoltenVK are not needed. Desktop Linux / macOS only (not GLES/iOS/Android).
if (NOT ZELDA3D_SDL2_OPENGL AND NOT USE_OPENGLES AND
    NOT CMAKE_SYSTEM_NAME STREQUAL "iOS" AND NOT CMAKE_SYSTEM_NAME STREQUAL "Android")
    # find_package(glslang) exposes the imported targets glslang::glslang / glslang::SPIRV /
    # the default resource limits used by the runtime GLSL->SPIR-V compile.
    find_package(glslang QUIET)
    if (glslang_FOUND)
        target_link_libraries(libultraship PRIVATE
            glslang::glslang glslang::SPIRV glslang::glslang-default-resource-limits)
        target_compile_definitions(libultraship PRIVATE ENABLE_SDL3GPU)
        message(STATUS "zelda3d: SDL3 GPU backend ENABLED (glslang SPIR-V + SDL3 GPU API)")
    else()
        message(FATAL_ERROR "zelda3d: glslang not found; the default SDL3 GPU renderer cannot build")
    endif()
endif()

target_link_libraries(libultraship PUBLIC prism monocypher)

if(ENABLE_SCRIPTING)
    target_link_libraries(libultraship PUBLIC libtcc libtcc1)
    target_compile_definitions(libultraship PUBLIC ENABLE_SCRIPTING)
endif()

#=================== Compile Options & Defs ===================

target_compile_definitions(libultraship PRIVATE ${GBI_UCODE})

if (ZELDA3D_WINDOWS_FAMILY)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        WIN32
        _CONSOLE
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
    )
endif()
if(ZELDA3D_UWP)
    # This generated RmlUi GL loader normally infers UWP from WINAPI_FAMILY.
    # The core intentionally uses the normal Windows toolchain, so select its
    # app-container branch explicitly and load the packaged Mesa opengl32.dll.
    target_compile_definitions(libultraship PRIVATE GLAD_PLATFORM_UWP=1)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES
    )
endif()

set(SPDLOG_LEVEL_TRACE 0)
set(SPDLOG_LEVEL_DEBUG 1)
set(SPDLOG_LEVEL_INFO 2)
set(SPDLOG_LEVEL_WARN 3)
set(SPDLOG_LEVEL_ERROR 4)
set(SPDLOG_LEVEL_CRITICAL 5)
set(SPDLOG_LEVEL_OFF 6)
set(LOG_LEVELS "SPDLOG_LEVEL_TRACE;SPDLOG_LEVEL_DEBUG;SPDLOG_LEVEL_INFO;SPDLOG_LEVEL_WARN;SPDLOG_LEVEL_ERROR;SPDLOG_LEVEL_CRITICAL;SPDLOG_LEVEL_OFF")
set(SPDLOG_MIN_CUTOFF SPDLOG_LEVEL_TRACE CACHE STRING "The minimum log level that will be available to a build. Can be overridden in cmake or in InitLogging")
set_property(CACHE SPDLOG_MIN_CUTOFF PROPERTY STRINGS ${LOG_LEVELS})
if(NOT SPDLOG_MIN_CUTOFF IN_LIST LOG_LEVELS)
    message(FATAL_ERROR "SPDLOG_MIN_CUTOFF must be one of ${LOG_LEVELS}")
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
    target_compile_definitions(libultraship PRIVATE
        $<$<CONFIG:Debug>:_DEBUG>
        $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
        SPDLOG_ACTIVE_LEVEL=${SPDLOG_MIN_CUTOFF}
        __IOS__
    )

    set_xcode_property(${PROJECT_NAME} PRODUCT_BUNDLE_IDENTIFIER ${BUNDLE_ID} All)

    if(NOT SIGN_LIBRARY)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ""
            XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
        )

        if(TARGET SDL2)
            set_target_properties(SDL2 PROPERTIES
                XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ""
                XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
            )
        endif()

        set_target_properties(zip PROPERTIES
            XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ""
            XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
        )
    endif()
else()
    target_compile_definitions(libultraship PRIVATE
        $<$<CONFIG:Debug>:_DEBUG>
        $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
        SPDLOG_ACTIVE_LEVEL=${SPDLOG_MIN_CUTOFF}
        $<$<BOOL:${USE_OPENGLES}>:USE_OPENGLES>
        $<$<BOOL:${GFX_DEBUG_DISASSEMBLER}>:GFX_DEBUG_DISASSEMBLER>
    )
endif()

if(MSVC)
    target_compile_options(libultraship PRIVATE
        $<$<CONFIG:Debug>:
            /Od;
            /Oi-
        >
        $<$<CONFIG:Release>:
            /Oi;
            /Gy
        >
        /permissive-;
        /MP;
        /sdl;
        /W3;
        ${DEFAULT_CXX_DEBUG_INFORMATION_FORMAT};
        ${DEFAULT_CXX_EXCEPTION_HANDLING};
    )
    target_link_options(libultraship PRIVATE
        $<$<CONFIG:Release>:
            /OPT:REF;
            /OPT:ICF
        >
    )
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
        target_link_options(libultraship PRIVATE /SUBSYSTEM:CONSOLE)
    endif()
endif()

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU|Clang")
	target_compile_options(libultraship PRIVATE
		-Wall
		-Wextra
		-Wno-unused-variable
		-Wno-unused-parameter
		-Wno-unused-function
		-Wno-parentheses
		-Wno-missing-field-initializers
        $<$<COMPILE_LANGUAGE:C,OBJC>:-Wno-implicit-function-declaration>
        $<$<COMPILE_LANGUAGE:C,OBJC>:-Wno-int-conversion>
	)
    if (MSVC)
    target_compile_options(libultraship PRIVATE
        -Wno-implicit-function-declaration
    )
    endif()
endif()
