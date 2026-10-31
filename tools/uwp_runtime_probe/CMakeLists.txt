cmake_minimum_required(VERSION 3.26)
project(FusionUwpRuntimeProbe LANGUAGES CXX)
if(NOT CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION STREQUAL "10.0.19041.0")
    message(FATAL_ERROR "Select SDK 10.0.19041.0 explicitly in the Visual Studio generator platform")
endif()

# Isolated proof for the existing normal-Windows core layout. This does not
# change the game targets or reuse any /MT dependency objects.
if(NOT MSVC OR NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    message(FATAL_ERROR "Use a normal Windows MSVC generator for this probe")
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
set(CMAKE_CXX_STANDARD_LIBRARIES "WindowsApp.lib")
option(FUSION_PROBE_FREETYPE "Include the pinned Store font initialization dependency" OFF)

add_library(fusion_uwp_runtime_probe SHARED runtime_probe.cpp)
# Store props enable C++/CX by default; the core is native C++20, which cannot
# be compiled with /ZW. File metadata overrides the inherited Store default.
set_source_files_properties(runtime_probe.cpp PROPERTIES VS_SETTINGS "CompileAsWinRT=false")
if(FUSION_PROBE_FREETYPE)
    if(NOT VCPKG_TARGET_TRIPLET STREQUAL "x64-uwp-static-md")
        message(FATAL_ERROR "Font probe requires genuine x64-uwp-static-md dependencies")
    endif()
    find_package(Freetype 2.14.3 EXACT REQUIRED)
    target_sources(fusion_uwp_runtime_probe PRIVATE freetype_probe.cpp)
    set_source_files_properties(freetype_probe.cpp PROPERTIES VS_SETTINGS "CompileAsWinRT=false")
    target_link_libraries(fusion_uwp_runtime_probe PRIVATE Freetype::Freetype)
endif()
target_compile_features(fusion_uwp_runtime_probe PRIVATE cxx_std_20)
target_compile_definitions(fusion_uwp_runtime_probe PRIVATE
    WINAPI_FAMILY=WINAPI_FAMILY_APP _WIN32_WINNT=0x0A00 NOMINMAX)
target_compile_options(fusion_uwp_runtime_probe PRIVATE /EHsc /W4)
target_link_options(fusion_uwp_runtime_probe PRIVATE /APPCONTAINER /WINMD:NO /VERBOSE:LIB)
set_target_properties(fusion_uwp_runtime_probe PROPERTIES
    VS_GLOBAL_AppContainerApplication "true"
    VS_GLOBAL_ApplicationType "Windows Store"
    VS_GLOBAL_ApplicationTypeRevision "10.0"
    VS_GLOBAL_WindowsTargetPlatformMinVersion "10.0.19041.0")
