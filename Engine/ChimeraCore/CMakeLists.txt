file(GLOB_RECURSE CORE_SOURCES
        "Source/Private/*.cpp"
        "Source/Private/*.h"
        "Source/Public/*.h"
)

add_library(ChimeraCore STATIC ${CORE_SOURCES})

# Aseguramos explícitamente que la unidad con las implementaciones de allocation
# esté en el target (el glob ya la incluye, pero esto es explícito y claro).
#target_sources(ChimeraCore PRIVATE
#        Source/Private/EASTLNew.cpp
#)

#target_precompile_headers(ChimeraCore 
#        "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/Source/Public/CorePCH.h>"
#)

# Esto permite que otros módulos hagan #include "RenderTypes.h" sin rutas relativas
target_include_directories(ChimeraCore PUBLIC Source/Public/)

target_link_libraries(ChimeraCore PUBLIC
        glm::glm
        fmt::fmt
        EASTL
        imgui::imgui
)

# Cambiamos add_compile_definitions por target_compile_definitions con PUBLIC
# Esto hace que CUALQUIER target que enlace con Core (como Renderer o Game)
# herede estas definiciones automáticamente.
target_compile_definitions(ChimeraCore PUBLIC
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_ENABLE_EXPERIMENTAL
)