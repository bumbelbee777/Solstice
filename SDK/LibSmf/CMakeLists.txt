cmake_minimum_required(VERSION 3.20)

project(LibSmf LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(LIBSMF_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(LIBSMF_INCLUDE "${LIBSMF_DIR}/include")

add_library(LibSmf STATIC
    src/SmfWire.cxx
    src/SmfBinary.cxx
)

target_include_directories(LibSmf
    PUBLIC
        $<BUILD_INTERFACE:${LIBSMF_INCLUDE}>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(LibSmf PUBLIC cxx_std_20)

if(DEFINED zstd_SOURCE_DIR)
    target_include_directories(LibSmf PRIVATE "${zstd_SOURCE_DIR}/lib")
endif()
target_link_libraries(LibSmf PRIVATE libzstd_static)

if(MSVC)
    target_compile_options(LibSmf PRIVATE /W3 /Zc:preprocessor)
    target_compile_definitions(LibSmf PRIVATE
        _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS
    )
endif()
