cmake_minimum_required(VERSION 3.20)

project(LibFileWatch LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(LibFileWatch STATIC
    FileWatcher.cxx
)

target_include_directories(LibFileWatch
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(LibFileWatch PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(LibFileWatch PRIVATE /W3 /Zc:preprocessor)
    target_compile_definitions(LibFileWatch PRIVATE
        _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS
    )
endif()
