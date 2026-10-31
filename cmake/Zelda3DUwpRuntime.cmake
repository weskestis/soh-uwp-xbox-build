# Native C++20 core DLLs use Store CRT/project properties without C++/CX.
# The separate uwp/ WindowsStore project still owns application activation.
include_guard(GLOBAL)

macro(zelda3d_uwp_runtime_defaults)
    if(NOT MSVC OR NOT CMAKE_GENERATOR MATCHES "Visual Studio")
        message(FATAL_ERROR "The Fusion Store core requires MSVC and the Visual Studio generator")
    endif()
    if(NOT CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION STREQUAL "10.0.19041.0")
        message(FATAL_ERROR "Select -A x64,version=10.0.19041.0 for the Fusion Store core")
    endif()
    if(NOT VCPKG_TARGET_TRIPLET STREQUAL "x64-uwp-static-md")
        message(FATAL_ERROR "The Fusion Store core requires x64-uwp-static-md dependencies")
    endif()
    file(WRITE "${CMAKE_BINARY_DIR}/uwp-store-targets.txt" "")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    # Replace normal-Windows default libraries, which include User32/Shell32.
    set(CMAKE_C_STANDARD_LIBRARIES "WindowsApp.lib")
    set(CMAKE_CXX_STANDARD_LIBRARIES "WindowsApp.lib")
    add_compile_definitions(WINAPI_FAMILY=WINAPI_FAMILY_APP _WIN32_WINNT=0x0A00)
endmacro()

function(zelda3d_uwp_runtime_targets directory)
    get_property(children DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
    foreach(child IN LISTS children)
        zelda3d_uwp_runtime_targets("${child}")
    endforeach()
    get_property(targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(target IN LISTS targets)
        get_target_property(kind "${target}" TYPE)
        if(NOT kind MATCHES "^(STATIC_LIBRARY|SHARED_LIBRARY|MODULE_LIBRARY|OBJECT_LIBRARY|EXECUTABLE)$")
            continue()
        endif()
        get_target_property(binary_dir "${target}" BINARY_DIR)
        file(APPEND "${CMAKE_BINARY_DIR}/uwp-store-targets.txt" "${binary_dir}/${target}.vcxproj\n")
        set_target_properties("${target}" PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
            VS_GLOBAL_AppContainerApplication "true"
            VS_GLOBAL_ApplicationType "Windows Store"
            VS_GLOBAL_ApplicationTypeRevision "10.0"
            VS_GLOBAL_WindowsTargetPlatformMinVersion "10.0.19041.0")
        # File metadata overrides the /ZW default inherited from Store props.
        # Use each target's source directory, including FetchContent targets.
        get_target_property(sources "${target}" SOURCES)
        get_target_property(source_dir "${target}" SOURCE_DIR)
        foreach(source IN LISTS sources)
            if(source MATCHES "\\$<" OR NOT source MATCHES "\\.(c|cc|cpp|cxx)$")
                continue()
            endif()
            if(NOT IS_ABSOLUTE "${source}")
                set(source "${source_dir}/${source}")
            endif()
            set_property(SOURCE "${source}" TARGET_DIRECTORY "${target}" APPEND
                PROPERTY VS_SETTINGS "CompileAsWinRT=false")
        endforeach()
        if(kind MATCHES "^(SHARED_LIBRARY|MODULE_LIBRARY|EXECUTABLE)$")
            target_link_options("${target}" PRIVATE /APPCONTAINER /WINMD:NO /VERBOSE:LIB)
        endif()
    endforeach()
endfunction()
