add_executable(ChimeraEngine
        Source/main.cpp
)

#target_precompile_headers(ChimeraEngine REUSE_FROM ChimeraCore)

#target_link_options(ChimeraEngine PRIVATE
#        /WHOLEARCHIVE:$<TARGET_FILE:ChimeraCore>
#)

target_link_options(ChimeraEngine PRIVATE
        /INCLUDE:??_U@YAPEAX_KPEBDHI1H@Z
        /INCLUDE:??_U@YAPEAX_K00PEBDHI1H@Z
)

target_link_libraries(ChimeraEngine PRIVATE
        Main
)

add_custom_command(TARGET ChimeraEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:ChimeraEngine>
        $<TARGET_FILE_DIR:ChimeraEngine>
        COMMAND_EXPAND_LISTS
)