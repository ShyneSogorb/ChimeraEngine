add_executable(ChimeraEngine
        Source/main.cpp
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