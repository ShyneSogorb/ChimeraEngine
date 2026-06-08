file(GLOB_RECURSE GENERIC_INPUT_SOURCES
        "Source/Private/*.cpp"
        "Source/Private/*.h"
        "Source/Public/*.h"
)

add_library(GenericInput STATIC ${GENERIC_INPUT_SOURCES})

target_include_directories(GenericInput PUBLIC Source/Public/)

target_link_libraries(GenericInput
        PUBLIC  ChimeraCore
)
