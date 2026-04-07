cmake_minimum_required(VERSION 3.20)
project(ThermalPlume)

add_executable(ThermalPlume
    "Main.cxx"
    "ThermalPlumeGame.cxx"
    "ThermalPlumeGame.hxx"
    "ExhaustParticleSystem.cxx"
    "ExhaustParticleSystem.hxx"
)

target_include_directories(ThermalPlume PRIVATE
    "${CMAKE_SOURCE_DIR}/3rdparty/bgfx"
    "${CMAKE_SOURCE_DIR}/3rdparty/bgfx/3rdparty/meshoptimizer/src"
)

target_link_libraries(ThermalPlume PRIVATE SolsticeEngine)

set_target_properties(ThermalPlume PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(WIN32)
    add_custom_command(TARGET ThermalPlume POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SolsticeEngine>
        $<TARGET_FILE_DIR:ThermalPlume>
        COMMENT "Copying SolsticeEngine.dll to executable directory"
    )
    add_custom_command(TARGET ThermalPlume POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3-shared>
        $<TARGET_FILE_DIR:ThermalPlume>
        COMMENT "Copying SDL3.dll to executable directory"
    )
endif()

add_custom_command(TARGET ThermalPlume POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:ThermalPlume>/assets
    COMMENT "Copying assets directory to executable directory"
)

copy_shaders_to_target(ThermalPlume)
