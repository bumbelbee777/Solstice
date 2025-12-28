cmake_minimum_required(VERSION 3.20)
project(Blizzard)

# Meshoptimizer source files for mesh optimization
set(MESHOPTIMIZER_SOURCES
    "${CMAKE_SOURCE_DIR}/3rdparty/bgfx/3rdparty/meshoptimizer/src/simplifier.cpp"
    "${CMAKE_SOURCE_DIR}/3rdparty/bgfx/3rdparty/meshoptimizer/src/vertexcodec.cpp"
    "${CMAKE_SOURCE_DIR}/3rdparty/bgfx/3rdparty/meshoptimizer/src/indexcodec.cpp"
    "${CMAKE_SOURCE_DIR}/3rdparty/bgfx/3rdparty/meshoptimizer/src/vcacheoptimizer.cpp"
    "${CMAKE_SOURCE_DIR}/3rdparty/bgfx/3rdparty/meshoptimizer/src/vfetchoptimizer.cpp"
    "${CMAKE_SOURCE_DIR}/3rdparty/bgfx/3rdparty/meshoptimizer/src/overdrawoptimizer.cpp"
    "${CMAKE_SOURCE_DIR}/3rdparty/bgfx/3rdparty/meshoptimizer/src/clusterizer.cpp"
    "${CMAKE_SOURCE_DIR}/3rdparty/bgfx/3rdparty/meshoptimizer/src/vertexfilter.cpp"
)

# Create the executable
add_executable(Blizzard
    "Main.cxx"
    "BlizzardGame.cxx"
    "BlizzardGame.hxx"
)

# Include directories for meshoptimizer
target_include_directories(Blizzard PRIVATE
    "${CMAKE_SOURCE_DIR}/3rdparty/bgfx"
    "${CMAKE_SOURCE_DIR}/3rdparty/bgfx/3rdparty/meshoptimizer/src"
)

# Link against the Solstice engine
target_link_libraries(Blizzard PRIVATE SolsticeEngine)

# Set C++ standard
set_target_properties(Blizzard PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Copy DLLs to the executable directory on Windows
if(WIN32)
    # Copy SolsticeEngine.dll
    add_custom_command(TARGET Blizzard POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SolsticeEngine>
        $<TARGET_FILE_DIR:Blizzard>
        COMMENT "Copying SolsticeEngine.dll to executable directory"
    )

    # Copy SDL3.dll
    add_custom_command(TARGET Blizzard POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3-shared>
        $<TARGET_FILE_DIR:Blizzard>
        COMMENT "Copying SDL3.dll to executable directory"
    )
endif()

# Copy assets directory to the executable directory
add_custom_command(TARGET Blizzard POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:Blizzard>/assets
    COMMENT "Copying assets directory to executable directory"
)

# Copy scripts directory to the executable directory
add_custom_command(TARGET Blizzard POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/scripts
    $<TARGET_FILE_DIR:Blizzard>/scripts
    COMMENT "Copying scripts directory to executable directory"
)

# Copy shaders to the executable directory
copy_shaders_to_target(Blizzard)

