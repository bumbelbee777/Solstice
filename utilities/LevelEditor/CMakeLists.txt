cmake_minimum_required(VERSION 3.20)
project(LevelEditor)

add_executable(LevelEditor
    Main.cxx
    "${CMAKE_SOURCE_DIR}/utilities/MovieMaker/PreviewTexture.cxx"
)

target_include_directories(LevelEditor PRIVATE
    "${CMAKE_SOURCE_DIR}/SDK"
    "${CMAKE_SOURCE_DIR}/utilities"
    "${CMAKE_SOURCE_DIR}/utilities/MovieMaker"
)

target_link_libraries(LevelEditor PRIVATE LibUI LibSmf EditorEnginePreview UtilityPluginHost)

if(MSVC)
    target_compile_definitions(LevelEditor PRIVATE SOLSTICE_STATIC_LINK)
endif()

if(WIN32)
    target_link_libraries(LevelEditor PRIVATE opengl32)
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(LevelEditor PRIVATE OpenGL::GL)
endif()

set_target_properties(LevelEditor PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(WIN32)
    add_custom_command(TARGET LevelEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:LibUI>
        $<TARGET_FILE_DIR:LevelEditor>
        COMMENT "Copying LibUI.dll to LevelEditor directory"
    )
    add_custom_command(TARGET LevelEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3-shared>
        $<TARGET_FILE_DIR:LevelEditor>
        COMMENT "Copying SDL3.dll to LevelEditor directory"
    )
    add_custom_command(TARGET LevelEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SolsticeEngine>
        $<TARGET_FILE_DIR:LevelEditor>
        COMMENT "Copying SolsticeEngine.dll to LevelEditor directory"
    )
endif()

copy_shaders_to_target(LevelEditor)
