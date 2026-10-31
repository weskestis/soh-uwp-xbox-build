# GLEW's desktop CMake/vcpkg recipe imports Windows SDK OpenGL. Build only
# its released loader source and link our pinned packaged Mesa implementation.
include(FetchContent)
FetchContent_Declare(fusion_glew
    URL https://github.com/nigels-com/glew/releases/download/glew-2.3.1/glew-2.3.1.tgz
    URL_HASH SHA512=cb4caecf32ec0f180c2691dc7769ffc99571c64f259a2663a2b80e788f1c2fd5362c59e0caaeefed6fb78a4070366d244666a657358049b09071b59fae2377e0
    SOURCE_SUBDIR fusion-no-upstream-cmake)
FetchContent_MakeAvailable(fusion_glew)
add_library(fusion_glew STATIC "${fusion_glew_SOURCE_DIR}/src/glew.c")
add_library(GLEW::GLEW ALIAS fusion_glew)
target_include_directories(fusion_glew PUBLIC "${fusion_glew_SOURCE_DIR}/include")
target_compile_definitions(fusion_glew PUBLIC GLEW_STATIC GLEW_NO_GLU)
target_compile_options(fusion_glew PRIVATE "/FI${CMAKE_CURRENT_LIST_DIR}/uwp_mesa_wgl.h")
target_link_libraries(fusion_glew PUBLIC OpenGL::GL)
