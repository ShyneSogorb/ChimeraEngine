cmake_minimum_required(VERSION 3.5)
project(ChimeraEngine CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ─── FetchContent global ────────────────────────────────────────────────────
include(FetchContent)

FetchContent_Declare(sdl2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG        release-2.30.9
)
FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG        1.0.1
)
FetchContent_Declare(vkbootstrap
        GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git
        GIT_TAG        v1.3.295
)
FetchContent_Declare(vma
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
        GIT_TAG        v3.1.0
)
FetchContent_Declare(imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        v1.91.6
)
FetchContent_Declare(stbimage
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG        master
)
FetchContent_Declare(fastgltf
        GIT_REPOSITORY https://github.com/spnda/fastgltf.git
        GIT_TAG        v0.8.0
)
FetchContent_Declare(fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG        11.0.2
)
FetchContent_Declare(EASTL
        GIT_REPOSITORY https://github.com/electronicarts/EASTL.git
        GIT_TAG        master
)

FetchContent_MakeAvailable(sdl2 glm vkbootstrap vma stbimage fastgltf fmt imgui EASTL)

# ImGui como librería reutilizable desde aquí
add_library(imgui_lib STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)
target_include_directories(imgui_lib PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
        ${stbimage_SOURCE_DIR}
)
target_link_libraries(imgui_lib PUBLIC
        Vulkan::Vulkan
        SDL2::SDL2
)

# ─── Vulkan ─────────────────────────────────────────────────────────────────
find_package(Vulkan REQUIRED)

# ─── Definiciones globales ───────────────────────────────────────────────────
add_compile_definitions(
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_ENABLE_EXPERIMENTAL
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR NOT CMAKE_BUILD_TYPE)
    add_compile_definitions(RESOURCES_PATH="${CMAKE_SOURCE_DIR}/Resources/")
    add_compile_definitions(SHADER_PATH="${CMAKE_SOURCE_DIR}/Shaders/Compiled/")
else()
    add_compile_definitions(RESOURCES_PATH="Resources/")
    add_compile_definitions(SHADER_PATH="Shaders/Compiled/")
endif()

# ─── Módulos ─────────────────────────────────────────────────────────────────
add_subdirectory(Engine/ChimeraCore)
add_subdirectory(Engine/VulkanRenderer)
#add_subdirectory(Engine/Physics)
#add_subdirectory(Engine/Audio)
add_subdirectory(Game/Main)
add_subdirectory(Game/Launch)

# ─── Shaders ─────────────────────────────────────────────────────────────────
find_program(GLSL_VALIDATOR glslangValidator
        HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/
)

file(GLOB_RECURSE GLSL_SOURCE_FILES
        "Shaders/*.frag"
        "Shaders/*.vert"
        "Shaders/*.comp"
)

foreach(GLSL ${GLSL_SOURCE_FILES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "Shaders/Compiled/${FILE_NAME}.spv")

    if(GLSL MATCHES ".*((\.inc)|(\.glsl))$")
        continue()
    endif()

    set(GLSL_STRIPPED "${CMAKE_BINARY_DIR}/ShaderStripped/${FILE_NAME}")
    add_custom_command(
            OUTPUT ${GLSL_STRIPPED}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/ShaderStripped"
            COMMAND python3 -c
            "import sys; d=open('${GLSL}','rb').read(); open('${GLSL_STRIPPED}','wb').write(d[3:] if d[:3]==b'\\xef\\xbb\\xbf' else d)"
            DEPENDS ${GLSL}
    )

    add_custom_command(
            OUTPUT ${SPIRV}
            COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
            DEPENDS ${GLSL}
    )

    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach()


add_custom_target(Shaders DEPENDS ${SPIRV_BINARY_FILES})