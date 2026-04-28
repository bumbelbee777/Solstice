cmake_minimum_required(VERSION 3.20)
project(WarsOfHeaven)

if(NOT BGFX_DIR)
    message(FATAL_ERROR "BGFX_DIR is not set; configure the top-level Solstice project so bgfx (CPM) is available.")
endif()

add_executable(WarsOfHeaven
    "Main.cxx"
    "WarsOfHeavenSmoke.cxx"
    "WarsOfHeavenSmoke.hxx"
    "WohNetworkSession.cxx"
    "WohNetworkSession.hxx"
    "WohStarryBackground.cxx"
    "WohStarryBackground.hxx"
    "WohCombatScoring.cxx"
    "WohCombatScoring.hxx"
)

target_include_directories(WarsOfHeaven PRIVATE
    "${BGFX_DIR}"
    "${BGFX_DIR}/3rdparty/meshoptimizer/src"
)

target_link_libraries(WarsOfHeaven PRIVATE SolsticeEngine)

set_target_properties(WarsOfHeaven PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${SOLSTICE_EXECUTABLE_OUTPUT_DIR}"
)

if(WIN32)
    add_custom_command(TARGET WarsOfHeaven POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SolsticeEngine>
        $<TARGET_FILE_DIR:WarsOfHeaven>
        COMMENT "Copying SolsticeEngine.dll to executable directory"
    )
    add_custom_command(TARGET WarsOfHeaven POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3-shared>
        $<TARGET_FILE_DIR:WarsOfHeaven>
        COMMENT "Copying SDL3.dll to executable directory"
    )
endif()

add_custom_command(TARGET WarsOfHeaven POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/scripts
    $<TARGET_FILE_DIR:WarsOfHeaven>/example/WarsOfHeaven/scripts
    COMMENT "Copying WarsOfHeaven scripts next to executable"
)

copy_shaders_to_target(WarsOfHeaven)
