file(GLOB_RECURSE MAIN_SOURCES
        "Source/Private/*.cpp"
        "Source/Private/*.h"
        "Source/Public/*.h"
)

add_library(Main STATIC ${MAIN_SOURCES})

target_include_directories(Main PUBLIC Source/Public/)

target_link_libraries(Main PUBLIC
        ChimeraCore
        VulkanRenderer
)