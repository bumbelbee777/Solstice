# Disco (Lighting Stress Demo)
add_executable(Disco
    Main.cxx
)

target_link_libraries(Disco
    SolsticeEngine
)

# Set output directory
set_target_properties(Disco PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# MSVC static analysis
if(MSVC)
    target_compile_options(Disco PRIVATE
        /analyze
    )
endif()

# Copy DLLs to the executable directory on Windows
if(WIN32)
    # Copy SolsticeEngine.dll
    add_custom_command(TARGET Disco POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SolsticeEngine>
        $<TARGET_FILE_DIR:Disco>
        COMMENT "Copying SolsticeEngine.dll to executable directory"
    )

    # Copy SDL3.dll
    add_custom_command(TARGET Disco POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3-shared>
        $<TARGET_FILE_DIR:Disco>
        COMMENT "Copying SDL3.dll to executable directory"
    )
endif()

# Copy assets directory to the executable directory
add_custom_command(TARGET Disco POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:Disco>/assets
    COMMENT "Copying assets directory to executable directory"
)

# Copy shaders to the executable directory
copy_shaders_to_target(Disco)
