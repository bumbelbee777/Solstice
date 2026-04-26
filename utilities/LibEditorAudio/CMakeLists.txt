cmake_minimum_required(VERSION 3.20)

project(LibEditorAudio LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(LibEditorAudio STATIC
    EditorAudio.cxx
    EditorRecovery.cxx
)

target_include_directories(LibEditorAudio
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(LibEditorAudio
    PUBLIC
        SDL3-shared
        SDL3_mixer-static
)

target_compile_features(LibEditorAudio PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(LibEditorAudio PRIVATE /W3 /Zc:preprocessor)
    target_compile_definitions(LibEditorAudio PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS
    )
endif()
