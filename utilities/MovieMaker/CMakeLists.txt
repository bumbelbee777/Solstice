cmake_minimum_required(VERSION 3.20)
project(MovieMaker)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SOLSTICE_FFMPEG_CLI_ROOT "" CACHE PATH
    "Directory whose bin/ contains ffmpeg (e.g. from python tools/fetch_ffmpeg.py, or tools/_ffmpeg/ffmpeg)")
find_program(SOLSTICE_FFMPEG_EXECUTABLE NAMES ffmpeg ffmpeg.exe
    HINTS
        "${SOLSTICE_FFMPEG_CLI_ROOT}/bin"
        "$ENV{SOLSTICE_FFMPEG_CLI_ROOT}/bin"
    DOC "ffmpeg CLI for MovieMaker export / diagnostics")
mark_as_advanced(SOLSTICE_FFMPEG_EXECUTABLE)

if(TARGET Python3::Interpreter)
    add_custom_target(solstice-fetch-ffmpeg
        COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tools/fetch_ffmpeg.py
            --dest "${CMAKE_BINARY_DIR}/_ffmpeg_cli"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Download FFmpeg CLI into build/_ffmpeg_cli (optional)")
endif()

# Create the executable
add_executable(MovieMaker
    "Main.cxx"
    "FfmpegUtil.cxx"
    "VideoExport.cxx"
    "PreviewTexture.cxx"
    "Workflow/Workflow.cxx"
)

# LibParallax pulls engine static libs (Core, Math, …) for PARALLAX I/O.
# ffmpeg: optional CLI — runtime probe/test encode / video pipe; no linked libav.
# UI: MotionGraphics compositor (ImGui MGDisplayList; ImTextureID textures).
target_link_libraries(MovieMaker PRIVATE LibUI LibParallax UI EditorEnginePreview)
if(SOLSTICE_FFMPEG_EXECUTABLE)
    target_compile_definitions(MovieMaker PRIVATE SOLSTICE_HAVE_FFMPEG_CLI=1)
endif()
if(MSVC)
    target_compile_definitions(MovieMaker PRIVATE SOLSTICE_STATIC_LINK)
endif()
if(WIN32)
    target_link_libraries(MovieMaker PRIVATE opengl32 user32)
endif()

# Set C++ standard
set_target_properties(MovieMaker PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Copy DLLs to the executable directory on Windows
if(WIN32)
    # Copy LibUI.dll
    add_custom_command(TARGET MovieMaker POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:LibUI>
        $<TARGET_FILE_DIR:MovieMaker>
        COMMENT "Copying LibUI.dll to executable directory"
    )

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
