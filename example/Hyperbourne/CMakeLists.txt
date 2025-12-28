cmake_minimum_required(VERSION 3.20)
project(Hyperbourne)

# Create the executable
add_executable(Hyperbourne
    "Main.cxx"
    "HyperbourneGame.cxx"
    "HyperbourneGame.hxx"
    "Archon.cxx"
    "Archon.hxx"
)

# Link against the Solstice engine
target_link_libraries(Hyperbourne PRIVATE SolsticeEngine)

# Set C++ standard
set_target_properties(Hyperbourne PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Copy DLLs to the executable directory on Windows
if(WIN32)
    # Copy SolsticeEngine.dll
    add_custom_command(TARGET Hyperbourne POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SolsticeEngine>
        $<TARGET_FILE_DIR:Hyperbourne>
        COMMENT "Copying SolsticeEngine.dll to executable directory"
    )

    # Copy SDL3.dll
    add_custom_command(TARGET Hyperbourne POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3-shared>
        $<TARGET_FILE_DIR:Hyperbourne>
        COMMENT "Copying SDL3.dll to executable directory"
    )
endif()

# Copy assets directory to the executable directory
add_custom_command(TARGET Hyperbourne POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:Hyperbourne>/assets
    COMMENT "Copying assets directory to executable directory"
)

# Copy shaders to the executable directory
copy_shaders_to_target(Hyperbourne)

