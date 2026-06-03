file(GLOB_RECURSE CORE_SOURCES
        "Source/Private/*.cpp"
        "Source/Private/*.h"
        "Source/Public/*.h"
)

add_library(ChimeraCore STATIC ${CORE_SOURCES})

# Esto permite que otros módulos hagan #include "RenderTypes.h" sin rutas relativas
target_include_directories(ChimeraCore PUBLIC Source/Public/)

target_link_libraries(ChimeraCore PUBLIC
        glm::glm
        fmt::fmt
        EASTL::EASTL
)

# Cambiamos add_compile_definitions por target_compile_definitions con PUBLIC
# Esto hace que CUALQUIER target que enlace con Core (como Renderer o Game)
# herede estas definiciones automáticamente.
target_compile_definitions(ChimeraCore PUBLIC
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_ENABLE_EXPERIMENTAL
)