cmake_minimum_required(VERSION 3.20)
project(PhysicsPlayground)

# Create the executable
add_executable(PhysicsPlayground
    "PhysicsPlayground.cxx"
    "PhysicsPlayground.hxx"
    "ObjectSpawner.cxx"
    "ObjectSpawner.hxx"
    "SelectionSystem.cxx"
    "SelectionSystem.hxx"
    "UIManager.cxx"
    "UIManager.hxx"
    "TerminalHub.cxx"
    "TerminalHub.hxx"
)

# Link against the Solstice engine
target_link_libraries(PhysicsPlayground PRIVATE SolsticeEngine)

# Set C++ standard
set_target_properties(PhysicsPlayground PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Copy DLLs to the executable directory on Windows
if(WIN32)
    # Copy SolsticeEngine.dll
    add_custom_command(TARGET PhysicsPlayground POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SolsticeEngine>
        $<TARGET_FILE_DIR:PhysicsPlayground>
        COMMENT "Copying SolsticeEngine.dll to executable directory"
    )

    # Copy SDL3.dll
    add_custom_command(TARGET PhysicsPlayground POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3-shared>
        $<TARGET_FILE_DIR:PhysicsPlayground>
        COMMENT "Copying SDL3.dll to executable directory"
    )
endif()

# Copy assets directory to the executable directory
add_custom_command(TARGET PhysicsPlayground POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:PhysicsPlayground>/assets
    COMMENT "Copying assets directory to executable directory"
)

# Copy shaders to the executable directory
copy_shaders_to_target(PhysicsPlayground)
