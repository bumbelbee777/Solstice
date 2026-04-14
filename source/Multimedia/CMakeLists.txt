# FFmpeg-backed video decode (optional). No bgfx — texture upload lives in UI/VideoTexture.
set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")

if(FFmpeg_FOUND)
    add_library(Multimedia STATIC
        FfmpegVideoDecoder.cxx
    )
    target_include_directories(Multimedia PUBLIC ${SOURCE_DIR})
    target_compile_definitions(Multimedia PUBLIC SOLSTICE_HAVE_FFMPEG=1)
    target_link_libraries(Multimedia PRIVATE
        FFmpeg::SDL::AVCODEC
        FFmpeg::SDL::AVFORMAT
        FFmpeg::SDL::AVUTIL
        FFmpeg::SDL::SWSCALE
    )
    if(MSVC)
        target_compile_options(Multimedia PRIVATE /Zc:preprocessor /W0)
        target_compile_definitions(Multimedia PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
endif()
