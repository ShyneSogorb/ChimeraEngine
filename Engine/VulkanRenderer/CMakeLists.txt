file(GLOB_RECURSE VULKAN_RENDERER_SOURCES
        "Source/Private/*.cpp"
        "Source/Private/*.h"
        "Source/Public/*.h"
)

add_library(VulkanRenderer STATIC ${VULKAN_RENDERER_SOURCES})

target_include_directories(VulkanRenderer PUBLIC Source/Public/)

target_link_libraries(VulkanRenderer
        PUBLIC  ChimeraCore
        PRIVATE Vulkan::Vulkan
        PRIVATE SDL2::SDL2
        PRIVATE vk-bootstrap::vk-bootstrap
        PRIVATE GPUOpen::VulkanMemoryAllocator
        PRIVATE fastgltf::fastgltf
        PRIVATE imgui_lib
)

#target_precompile_headers(VulkanRenderer PRIVATE
#        <cstdlib>       # Para 'abort'
#        <cmath>         # Para 'acosf', 'log2'
#        <iostream>
#        <vector>
#        <memory>
#        <string>
#        <optional>
#)

# El renderer necesita saber dónde están los shaders compilados
target_compile_definitions(VulkanRenderer PUBLIC
        $<$<CONFIG:Debug>:SHADER_PATH="${CMAKE_SOURCE_DIR}/Shaders/Compiled/">
        $<$<CONFIG:Release>:SHADER_PATH="../../Shaders/Compiled">
)

add_dependencies(VulkanRenderer Shaders)
target_compile_definitions(VulkanRenderer PRIVATE SDL_MAIN_HANDLED)
