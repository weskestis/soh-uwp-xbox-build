cmake_minimum_required(VERSION 3.26)
project(FusionLoggerHost LANGUAGES CXX)

# Exercise the UWP logger factory in a native host process, with a separately
# built desktop spdlog. This proves synchronous error delivery, not Xbox boot.
find_package(spdlog CONFIG REQUIRED)
add_executable(uwp_release_logger_test ../uwp_release_logger_test.cpp)
target_compile_features(uwp_release_logger_test PRIVATE cxx_std_20)
target_compile_definitions(uwp_release_logger_test PRIVATE ZELDA3D_UWP=1 NOMINMAX)
target_include_directories(uwp_release_logger_test PRIVATE
    ../../Shipwright/libultraship/include)
target_link_libraries(uwp_release_logger_test PRIVATE spdlog::spdlog)
