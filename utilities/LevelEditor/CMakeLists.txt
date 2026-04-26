cmake_minimum_required(VERSION 3.20)
project(LevelEditor)

add_executable(LevelEditor
    Main.cxx
    JackhammerApp.cxx
    JackhammerSpatial.cxx
    JackhammerViewportDraw.cxx
    JackhammerTexturePaint.cxx
    JackhammerMeshOps.cxx
    JackhammerArzachelProps.cxx
    JackhammerBspTextureOps.cxx
    JackhammerLightmapBake.cxx
    JackhammerPrefabs.cxx
    JackhammerViewportGeoTools.cxx
)

target_include_directories(LevelEditor PRIVATE
    "${CMAKE_SOURCE_DIR}/SDK"
    "${CMAKE_SOURCE_DIR}/utilities"
)

target_link_libraries(LevelEditor PRIVATE LibUI LibSmf LibFileWatch LibSettingsStore LibEditorAudio EditorEnginePreview UtilityPluginHost)

if(MSVC)
    target_compile_definitions(LevelEditor PRIVATE SOLSTICE_STATIC_LINK)
    # Synchronized PDB writes when /MP or parallel MSBuild cl.exe share vc*.pdb (avoids C1041).
    target_compile_options(LevelEditor PRIVATE /FS)
    # Console subsystem so stdout/stderr from fprintf / DiagLog / std::cerr show in Cursor's integrated terminal.
    target_link_options(LevelEditor PRIVATE /SUBSYSTEM:CONSOLE)
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
    RUNTIME_OUTPUT_DIRECTORY "${SOLSTICE_EXECUTABLE_OUTPUT_DIR}"
)

if(WIN32)
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

add_custom_command(TARGET LevelEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:LevelEditor>/fonts"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/resources/fonts/CascadiaCode-Regular.ttf"
        "$<TARGET_FILE_DIR:LevelEditor>/fonts/CascadiaCode-Regular.ttf"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/resources/fonts/Phosphor.ttf"
        "$<TARGET_FILE_DIR:LevelEditor>/fonts/Phosphor.ttf"
    COMMENT "Copy bundled UI fonts next to LevelEditor"
)

solstice_utility_postbuild_package(LevelEditor "tools/packaging/LevelEditor.fst")
