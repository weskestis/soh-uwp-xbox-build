cmake_minimum_required(VERSION 3.26 FATAL_ERROR)

# This is intentionally a separate configure from the desktop/core build.  The
# latter produces the self-contained core DLL; this WindowsStore project owns only the WinRT entry,
# deployment graph, AppX manifest, and signing boundary.
set(CMAKE_SYSTEM_NAME WindowsStore CACHE STRING "UWP system" FORCE)
set(CMAKE_SYSTEM_VERSION 10.0.19041.0 CACHE STRING "UWP SDK baseline" FORCE)
project(zelda3d_uwp LANGUAGES CXX)

if(NOT MSVC OR NOT CMAKE_GENERATOR MATCHES "Visual Studio")
    message(FATAL_ERROR "The UWP wrapper requires the Visual Studio generator and Windows SDK.")
endif()
if(NOT CMAKE_VS_PLATFORM_NAME STREQUAL "x64")
    message(FATAL_ERROR "The Xbox Device Portal target is x64; configure with -A x64.")
endif()

get_filename_component(ZELDA3D_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(ZELDA3D_UWP_RUNTIME_DIR "" CACHE PATH
    "Staged Windows runtime containing the core DLL, assets, soh.o2r, and dependency DLLs")
set(ZELDA3D_UWP_DEPS_ROOT "" CACHE PATH
    "UWP dependency root with x64/include, x64/lib, and x64/bin (SDL2/libuwp/Mesa)")
set(ZELDA3D_UWP_EXTRA_DLLS "" CACHE STRING
    "Optional semicolon-separated absolute paths to additional runtime DLLs")

set(ZELDA3D_UWP_PACKAGE_NAME "WesKestis.SOHCursorFPSV3.OoT3DFull" CACHE STRING "Stable AppX identity")
set(ZELDA3D_UWP_PUBLISHER "CN=SOHCursorFPSV3OoT3DFull" CACHE STRING
    "Publisher subject; must match the signing certificate")
set(ZELDA3D_UWP_PUBLISHER_DISPLAY_NAME "Wes Kestis" CACHE STRING "Publisher display name")
set(ZELDA3D_UWP_DISPLAY_NAME "SOH CURSOR FPS V3 - OoT3D Full" CACHE STRING "Installed app name")
set(ZELDA3D_UWP_VERSION "1.0.0.0" CACHE STRING "Four-part numeric AppX version")
set(ZELDA3D_UWP_SIGNING_CERTIFICATE "" CACHE FILEPATH "Private AppX signing certificate")
option(ZELDA3D_UWP_REQUIRE_SIGNING "Refuse to produce a Device Portal package without signing" ON)
option(ZELDA3D_UWP_ALLOW_INCOMPLETE_RENDERER
       "Allow wrapper-only experiments before WindowsStore OpenGL validation is complete" OFF)

if(NOT ZELDA3D_UWP_ALLOW_INCOMPLETE_RENDERER)
    message(FATAL_ERROR
        "The SDL2/OpenGL renderer is implemented and verified on Linux, but its WindowsStore build and "
        "Xbox runtime are not yet validated. This guard prevents an installable-looking AppX from being "
        "mistaken for a tested package. Set -DZELDA3D_UWP_ALLOW_INCOMPLETE_RENDERER=ON only for the next "
        "Windows wrapper/link validation experiment; keep it OFF for any claimed Device Portal release.")
endif()
if(ZELDA3D_UWP_PACKAGE_NAME STREQUAL "Shipwright")
    message(FATAL_ERROR "The full build must not reuse the original V3 package identity.")
endif()
if(NOT ZELDA3D_UWP_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "ZELDA3D_UWP_VERSION must be four numeric fields, for example 1.0.0.0.")
endif()
if(ZELDA3D_UWP_REQUIRE_SIGNING AND NOT EXISTS "${ZELDA3D_UWP_SIGNING_CERTIFICATE}")
    message(FATAL_ERROR
        "A real Device Portal package must be signed. Set ZELDA3D_UWP_SIGNING_CERTIFICATE to a "
        "private certificate whose subject exactly matches ZELDA3D_UWP_PUBLISHER.")
endif()

foreach(required_dir IN ITEMS
        ZELDA3D_UWP_RUNTIME_DIR
        ZELDA3D_UWP_DEPS_ROOT)
    if(NOT IS_DIRECTORY "${${required_dir}}")
        message(FATAL_ERROR "${required_dir} is not a directory: ${${required_dir}}")
    endif()
endforeach()

set(uwp_dep_arch "${ZELDA3D_UWP_DEPS_ROOT}/x64")
set(required_runtime_files
    "${ZELDA3D_UWP_RUNTIME_DIR}/soh_core.dll"
    "${ZELDA3D_UWP_RUNTIME_DIR}/soh.o2r"
    "${uwp_dep_arch}/bin/dxil.dll"
    "${uwp_dep_arch}/bin/glfw3.dll"
    "${uwp_dep_arch}/bin/libgallium_wgl.dll"
    "${uwp_dep_arch}/bin/libuwp.dll"
    "${uwp_dep_arch}/bin/opengl32.dll"
    "${uwp_dep_arch}/bin/SDL2.dll"
    "${uwp_dep_arch}/bin/z-1.dll"
    "${uwp_dep_arch}/lib/libuwp.lib"
    "${uwp_dep_arch}/lib/SDL2.lib")
foreach(required_file IN LISTS required_runtime_files)
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "Required UWP runtime/link file is missing: ${required_file}")
    endif()
endforeach()

file(GLOB runtime_dependency_dlls CONFIGURE_DEPENDS
    "${ZELDA3D_UWP_RUNTIME_DIR}/deps/*.dll")

configure_file(Package.appxmanifest.in "${CMAKE_CURRENT_BINARY_DIR}/Package.appxmanifest" @ONLY)

add_executable(zelda3d-uwp WIN32
    main.cpp
    boot_diagnostics.cpp
    boot_diagnostics.h
    core_bootstrap.cpp
    core_bootstrap.h
    "${CMAKE_CURRENT_BINARY_DIR}/Package.appxmanifest")
target_compile_features(zelda3d-uwp PRIVATE cxx_std_20)
target_compile_definitions(zelda3d-uwp PRIVATE
    ZELDA3D_CORE_DYNAMIC_LOAD=1
    ZELDA3D_UWP_PACKAGE_VERSION="${ZELDA3D_UWP_VERSION}")
target_include_directories(zelda3d-uwp PRIVATE
    "${ZELDA3D_SOURCE_ROOT}/Shipwright/libultraship/include"
    "${uwp_dep_arch}/include")
target_link_libraries(zelda3d-uwp PRIVATE
    "${uwp_dep_arch}/lib/libuwp.lib"
    "${uwp_dep_arch}/lib/SDL2.lib"
    runtimeobject.lib
    WindowsApp.lib
    OneCore.lib)
# SDL2's non-XAML WinRT entry pattern is a native WinMain that calls
# SDL_WinRTRunApp. It does not emit a C++/CX .winmd; asking link.exe for one can
# turn an otherwise valid native wrapper into the secondary APPX0702 failure.
target_link_options(zelda3d-uwp PRIVATE "/WINMD:NO")

set_target_properties(zelda3d-uwp PROPERTIES
    OUTPUT_NAME "soh-cursor-fps-v3-oot3d-full"
    VS_WINDOWS_TARGET_PLATFORM_MIN_VERSION "10.0.19041.0"
    VS_GLOBAL_RootNamespace "uwp"
    VS_GLOBAL_ApplicationTypeRevision "10.0"
    VS_GLOBAL_GenerateAppxPackageOnBuild "true"
    VS_GLOBAL_AppxBundle "Never")
if(EXISTS "${ZELDA3D_UWP_SIGNING_CERTIFICATE}")
    set_target_properties(zelda3d-uwp PROPERTIES
        VS_GLOBAL_AppxPackageSigningEnabled "true"
        VS_GLOBAL_PackageCertificateKeyFile "${ZELDA3D_UWP_SIGNING_CERTIFICATE}")
else()
    set_target_properties(zelda3d-uwp PROPERTIES VS_GLOBAL_AppxPackageSigningEnabled "false")
endif()

function(zelda3d_uwp_deploy source destination)
    get_filename_component(source_abs "${source}" ABSOLUTE)
    set_source_files_properties("${source_abs}" PROPERTIES
        VS_COPY_TO_OUT_DIR Always
        VS_DEPLOYMENT_CONTENT TRUE)
    # An explicit "." becomes a literal dot directory in Visual Studio's
    # package.map.txt. MakeAppx rejects that destination with ERROR_INVALID_NAME
    # (0x8007007b), so root payloads must omit DeploymentContent's Link path.
    if(NOT destination STREQUAL "")
        set_source_files_properties("${source_abs}" PROPERTIES
            VS_DEPLOYMENT_LOCATION "${destination}")
    endif()
    target_sources(zelda3d-uwp PRIVATE "${source_abs}")
endfunction()

foreach(runtime_file IN LISTS required_runtime_files runtime_dependency_dlls ZELDA3D_UWP_EXTRA_DLLS)
    if(runtime_file MATCHES "\\.dll$")
        zelda3d_uwp_deploy("${runtime_file}" "")
    endif()
endforeach()
zelda3d_uwp_deploy("${ZELDA3D_UWP_RUNTIME_DIR}/soh.o2r" "")

if(IS_DIRECTORY "${ZELDA3D_UWP_RUNTIME_DIR}/assets")
    file(GLOB_RECURSE runtime_assets CONFIGURE_DEPENDS LIST_DIRECTORIES FALSE
         "${ZELDA3D_UWP_RUNTIME_DIR}/assets/*")
    foreach(asset IN LISTS runtime_assets)
        file(RELATIVE_PATH asset_rel "${ZELDA3D_UWP_RUNTIME_DIR}/assets" "${asset}")
        get_filename_component(asset_dir "${asset_rel}" DIRECTORY)
        if(asset_dir STREQUAL "")
            set(asset_destination "assets")
        else()
            set(asset_destination "assets/${asset_dir}")
        endif()
        zelda3d_uwp_deploy("${asset}" "${asset_destination}")
    endforeach()
else()
    message(FATAL_ERROR "The runtime assets directory is missing: ${ZELDA3D_UWP_RUNTIME_DIR}/assets")
endif()

set(package_assets
    Assets/LockScreenLogo.scale-200.png
    Assets/Square44x44Logo.targetsize-24_altform-unplated.png
    Assets/Square44x44Logo.scale-200.png
    Assets/Square150x150Logo.scale-200.png
    Assets/StoreLogo.png)
foreach(asset IN LISTS package_assets)
    zelda3d_uwp_deploy("${CMAKE_CURRENT_LIST_DIR}/${asset}" "Assets")
endforeach()
