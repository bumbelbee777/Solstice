# RenderStress example
add_executable(RenderStress
    Main.cxx
)

target_link_libraries(RenderStress PRIVATE SolsticeEngine)

set_target_properties(RenderStress PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${SOLSTICE_EXECUTABLE_OUTPUT_DIR}"
)

if(WIN32)
    add_custom_command(TARGET RenderStress POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3-shared>
        $<TARGET_FILE_DIR:RenderStress>
        COMMENT "Copying SDL3 runtime for RenderStress"
    )
endif()

copy_shaders_to_target(RenderStress)
