cmake_minimum_required(VERSION 3.20)
project(Blizzard)

# Meshoptimizer sources from the same bgfx tree CPM provides (root sets BGFX_DIR = bgfx_SOURCE_DIR).
if(NOT BGFX_DIR)
    message(FATAL_ERROR "BGFX_DIR is not set; configure the top-level Solstice project so bgfx (CPM) is available.")
endif()
set(_solstice_meshopt "${BGFX_DIR}/3rdparty/meshoptimizer/src")
set(MESHOPTIMIZER_SOURCES
    "${_solstice_meshopt}/simplifier.cpp"
    "${_solstice_meshopt}/vertexcodec.cpp"
    "${_solstice_meshopt}/indexcodec.cpp"
    "${_solstice_meshopt}/vcacheoptimizer.cpp"
    "${_solstice_meshopt}/vfetchoptimizer.cpp"
    "${_solstice_meshopt}/overdrawoptimizer.cpp"
    "${_solstice_meshopt}/clusterizer.cpp"
    "${_solstice_meshopt}/vertexfilter.cpp"
)

# Create the executable
add_executable(Blizzard
    "Main.cxx"
    "BlizzardGame.cxx"
    "BlizzardGame.hxx"
)

target_include_directories(Blizzard PRIVATE
    "${BGFX_DIR}"
    "${BGFX_DIR}/3rdparty/meshoptimizer/src"
)

# Link against the Solstice engine and optional prop import support
target_link_libraries(Blizzard PRIVATE SolsticeEngine Asset)

# Set C++ standard
set_target_properties(Blizzard PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${SOLSTICE_EXECUTABLE_OUTPUT_DIR}"
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

