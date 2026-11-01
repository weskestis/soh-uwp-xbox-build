cmake_minimum_required(VERSION 3.26)
project(FusionBootDiagnosticsHost LANGUAGES CXX)

if(NOT MSVC OR NOT CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION STREQUAL "10.0.19041.0")
    message(FATAL_ERROR "Use MSVC with -A x64,version=10.0.19041.0")
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
get_filename_component(root "../.." ABSOLUTE)

# A normal host loads two genuine Store-CRT DLLs: the production diagnostic
# code and a faulting fake core. No game assets or Xbox activation are needed.
add_executable(diagnostics_host host.cpp)
add_library(diagnostics_driver SHARED driver.cpp
    "${root}/uwp/boot_diagnostics.cpp" "${root}/uwp/boot_exception_report.cpp"
    "${root}/uwp/core_bootstrap.cpp")
add_library(diagnostics_fixture SHARED fixture.cpp)
foreach(target diagnostics_driver diagnostics_fixture)
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_include_directories(${target} PRIVATE
        "${root}/uwp" "${root}/Shipwright/libultraship/include" "${CMAKE_CURRENT_LIST_DIR}/stubs")
    target_compile_definitions(${target} PRIVATE WINAPI_FAMILY=WINAPI_FAMILY_APP
        _WIN32_WINNT=0x0A00 NOMINMAX ZELDA3D_CORE_DYNAMIC_LOAD=1
        ZELDA3D_UWP_PACKAGE_VERSION="native-diagnostics-test")
    target_compile_options(${target} PRIVATE /W4 /EHsc)
    target_link_libraries(${target} PRIVATE WindowsApp.lib OneCore.lib)
    target_link_options(${target} PRIVATE /APPCONTAINER /WINMD:NO)
    set_target_properties(${target} PROPERTIES
        VS_USER_PROPS "${root}/cmake/UwpNativeCpp.props"
        VS_GLOBAL_AppContainerApplication "true"
        VS_GLOBAL_ApplicationType "Windows Store"
        VS_GLOBAL_ApplicationTypeRevision "10.0"
        VS_GLOBAL_WindowsTargetPlatformMinVersion "10.0.19041.0")
    get_target_property(sources ${target} SOURCES)
    foreach(source IN LISTS sources)
        set_source_files_properties("${source}" PROPERTIES VS_SETTINGS "CompileAsWinRT=false")
    endforeach()
endforeach()
target_compile_features(diagnostics_host PRIVATE cxx_std_20)
target_compile_definitions(diagnostics_host PRIVATE NOMINMAX ZELDA3D_CORE_DYNAMIC_LOAD=1)
target_include_directories(diagnostics_host PRIVATE "${root}/Shipwright/libultraship/include")
