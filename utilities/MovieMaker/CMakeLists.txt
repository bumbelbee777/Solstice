cmake_minimum_required(VERSION 3.20)
project(MovieMaker)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SOLSTICE_FFMPEG_CLI_ROOT "" CACHE PATH
    "Directory whose bin/ contains ffmpeg (e.g. from python tools/fetch_ffmpeg.py, or tools/_ffmpeg/ffmpeg)")
if(SOLSTICE_FFMPEG_CLI_ROOT STREQUAL "" AND DEFINED ENV{SOLSTICE_FFMPEG_CLI_ROOT} AND NOT "$ENV{SOLSTICE_FFMPEG_CLI_ROOT}" STREQUAL "")
    set(SOLSTICE_FFMPEG_CLI_ROOT "$ENV{SOLSTICE_FFMPEG_CLI_ROOT}" CACHE PATH
        "Directory whose bin/ contains ffmpeg (also read from env SOLSTICE_FFMPEG_CLI_ROOT when unset)" FORCE)
endif()
find_program(SOLSTICE_FFMPEG_EXECUTABLE NAMES ffmpeg ffmpeg.exe
    HINTS
        "${SOLSTICE_FFMPEG_CLI_ROOT}/bin"
        "$ENV{SOLSTICE_FFMPEG_CLI_ROOT}/bin"
    DOC "ffmpeg CLI for MovieMaker export / diagnostics")
mark_as_advanced(SOLSTICE_FFMPEG_EXECUTABLE)

if(TARGET Python3::Interpreter)
    add_custom_target(solstice-fetch-ffmpeg
        COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tools/fetch_ffmpeg.py
            --dest "${SOLSTICE_STAGING_DIR}/ffmpeg_cli"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Download FFmpeg CLI into SOLSTICE_STAGING_DIR/ffmpeg_cli (optional)")
endif()

# Create the executable
add_executable(MovieMaker
    "Main.cxx"
    "SmmKeyframePresets.cxx"
    "SmmMg2DPanel.cxx"
    "SmmParallaxAuthoringPanel.cxx"
    "SmmFileOps.cxx"
    "SmmSessionAuthoring.cxx"
    "SmmLipsyncMorpheme.cxx"
    "SmmGltf.cxx"
    "Media/SmmImage.cxx"
    "Media/SmmAudio.cxx"
    "Editing/SmmCurveGraphBridge.cxx"
    "Editing/SmmCurveEditor.cxx"
    "Editing/SmmGraphEditor.cxx"
    "Editing/SmmParticleEditor.cxx"
    "Editing/SmmParticleScene.cxx"
    "Editing/SmmFluidPanel.cxx"
    "FfmpegUtil.cxx"
    "VideoExport.cxx"
    "Workflow/Workflow.cxx"
    "UI/Panels/ComingSoonPanels.cxx"
    "UI/Panels/PreviewPanels.cxx"
)

# LibParallax pulls engine static libs (Core, Math, …) for PARALLAX I/O.
# ffmpeg: optional CLI — runtime probe/test encode / video pipe; no linked libav.
# UI: MotionGraphics compositor (ImGui MGDisplayList; ImTextureID textures).
target_link_libraries(MovieMaker PRIVATE LibUI LibParallax LibFileWatch LibSettingsStore LibEditorAudio UI EditorEnginePreview UtilityPluginHost)
if(SOLSTICE_FFMPEG_EXECUTABLE)
    target_compile_definitions(MovieMaker PRIVATE SOLSTICE_HAVE_FFMPEG_CLI=1)
endif()
if(MSVC)
    target_compile_definitions(MovieMaker PRIVATE SOLSTICE_STATIC_LINK)
    # Many TUs: avoid C1041 when MSBuild compiles several .cxx with one PDB
    target_compile_options(MovieMaker PRIVATE /FS)
endif()
if(WIN32)
    target_link_libraries(MovieMaker PRIVATE opengl32 user32)
endif()

# Set C++ standard
set_target_properties(MovieMaker PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${SOLSTICE_EXECUTABLE_OUTPUT_DIR}"
)

# Copy DLLs to the executable directory on Windows
if(WIN32)
    # Copy SDL3.dll
    add_custom_command(TARGET MovieMaker POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3-shared>
        $<TARGET_FILE_DIR:MovieMaker>
        COMMENT "Copying SDL3.dll to executable directory"
    )

    add_custom_command(TARGET MovieMaker POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SolsticeEngine>
        $<TARGET_FILE_DIR:MovieMaker>
        COMMENT "Copying SolsticeEngine.dll to executable directory"
    )

    if(SOLSTICE_FFMPEG_EXECUTABLE)
        add_custom_command(TARGET MovieMaker POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SOLSTICE_FFMPEG_EXECUTABLE}"
            $<TARGET_FILE_DIR:MovieMaker>
            COMMENT "Copying ffmpeg.exe next to MovieMaker (if configured)"
        )
    endif()
endif()

copy_shaders_to_target(MovieMaker)

target_include_directories(MovieMaker PRIVATE "${CMAKE_SOURCE_DIR}/utilities" "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/source")

add_custom_command(TARGET MovieMaker POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:MovieMaker>/fonts"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/resources/fonts/CascadiaCode-Regular.ttf"
        "$<TARGET_FILE_DIR:MovieMaker>/fonts/CascadiaCode-Regular.ttf"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/resources/fonts/Phosphor.ttf"
        "$<TARGET_FILE_DIR:MovieMaker>/fonts/Phosphor.ttf"
    COMMENT "Copy bundled UI fonts next to MovieMaker"
)

if(EXISTS "${CMAKE_SOURCE_DIR}/presets")
    add_custom_command(TARGET MovieMaker POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/presets"
            "$<TARGET_FILE_DIR:MovieMaker>/presets"
        COMMENT "Copy presets/ next to MovieMaker (Keyframe INI, etc.)"
    )
endif()

solstice_utility_postbuild_package(MovieMaker "tools/packaging/MovieMaker.fst")
