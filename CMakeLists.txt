cmake_minimum_required(VERSION 3.20)
project(ChimeraEngine)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─── Vulkan (requiere Vulkan SDK instalado en el sistema) ───────────────────
find_package(Vulkan REQUIRED)

# ─── Dependencias via FetchContent ──────────────────────────────────────────
include(FetchContent)

FetchContent_Declare(
        sdl2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG        release-2.30.9
)

FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG        1.0.1
)

FetchContent_Declare(
        vkbootstrap
        GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git
        GIT_TAG        v1.3.295
)

FetchContent_Declare(
        vma
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
        GIT_TAG        v3.1.0
)

FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        v1.91.6
)

FetchContent_Declare(
        stbimage
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG        master
)

FetchContent_Declare(
        fastgltf
        GIT_REPOSITORY https://github.com/spnda/fastgltf.git
        GIT_TAG        v0.8.0
)

FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG        11.0.2
)

FETCHCONTENT_Declare(
        DXC
        GIT_REPOSITORY https://github.com/microsoft/DirectXShaderCompiler
        GIT_TAG         v1.9.2602
)

FetchContent_MakeAvailable(sdl2 glm vkbootstrap vma stbimage fastgltf fmt)

# ImGui no tiene CMakeLists propio util, se compila manual
FetchContent_MakeAvailable(imgui)

# ─── Sources ────────────────────────────────────────────────────────────────
file(GLOB_RECURSE SOURCES
        "Source/*.cpp"
        "Source/*.h"
)

# ImGui sources — los archivos que realmente necesitamos
set(IMGUI_SOURCES
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

# ─── Ejecutable ─────────────────────────────────────────────────────────────
add_executable(ChimeraEngine
        ${SOURCES}
        ${IMGUI_SOURCES}
)

target_include_directories(ChimeraEngine PRIVATE
        Source/Public
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
        ${stbimage_SOURCE_DIR}
)

target_compile_definitions(ChimeraEngine PUBLIC
        GLM_FORCE_DEPTH_ZERO_TO_ONE
)

target_precompile_headers(ChimeraEngine PUBLIC
        <optional> <vector> <memory> <string>
        <unordered_map>
        <glm/mat4x4.hpp> <glm/vec4.hpp>
        <vulkan/vulkan.h>
)

target_link_libraries(ChimeraEngine PRIVATE
        Vulkan::Vulkan
        SDL2::SDL2
        glm::glm
        vk-bootstrap::vk-bootstrap
        GPUOpen::VulkanMemoryAllocator
        fastgltf::fastgltf
        fmt::fmt
)

# ─── Compilación automática de shaders ──────────────────────────────────────
find_program(GLSL_VALIDATOR glslangValidator
        HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/
)

file(GLOB_RECURSE GLSL_SOURCE_FILES
        "${PROJECT_SOURCE_DIR}/Shaders/*.frag"
        "${PROJECT_SOURCE_DIR}/Shaders/*.vert"
        "${PROJECT_SOURCE_DIR}/Shaders/*.comp"
)

foreach(GLSL ${GLSL_SOURCE_FILES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${PROJECT_SOURCE_DIR}/Shaders/compiled/${FILE_NAME}.spv")
    add_custom_command(
            OUTPUT ${SPIRV}
            COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
            DEPENDS ${GLSL}
    )
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach()

add_custom_target(Shaders DEPENDS ${SPIRV_BINARY_FILES})
add_dependencies(ChimeraEngine Shaders)

# ─── Resources ──────────────────────────────────────────────────────────────
target_compile_definitions(ChimeraEngine PUBLIC
        $<$<CONFIG:Debug>:RESOURCES_PATH="${CMAKE_SOURCE_DIR}/Resources/">
        $<$<CONFIG:Release>:RESOURCES_PATH="Resources/">
        $<$<CONFIG:Debug>:SHADER_PATH="${CMAKE_SOURCE_DIR}/Shaders/compiled/">
        $<$<CONFIG:Release>:SHADER_PATH="Shaders/compiled/">
)

add_custom_command(TARGET ChimeraEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E $<IF:$<CONFIG:Release>,copy_directory,true>
        ${CMAKE_SOURCE_DIR}/Resources
        $<TARGET_FILE_DIR:ChimeraEngine>/Resources
)

# ─── Definitions ────────────────────────────────────────────────────────────

add_compile_definitions(
        GLM_ENABLE_EXPERIMENTAL
)

# ─── DLLs en Windows ────────────────────────────────────────────────────────
if(WIN32)
    add_custom_command(TARGET ChimeraEngine POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:ChimeraEngine> $<TARGET_FILE_DIR:ChimeraEngine>
            COMMAND_EXPAND_LISTS
    )
endif()