cmake_minimum_required(VERSION 3.5)

project(ChimeraEngine CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(sdl2           CONFIG REQUIRED)
find_package(glm            CONFIG REQUIRED)
find_package(vk-bootstrap    CONFIG REQUIRED)
find_package(VulkanMemoryAllocator            CONFIG REQUIRED)
find_package(fastgltf       CONFIG REQUIRED)
find_package(fmt            CONFIG REQUIRED)
find_package(EASTL          CONFIG REQUIRED)
find_package(imgui          CONFIG REQUIRED)
find_path(STB_INCLUDE_DIRS "stb_image.h")


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