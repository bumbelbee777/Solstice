cmake_minimum_required(VERSION 3.20)

project(LibNativeLoad LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(LibNativeLoad STATIC
    DynamicLibrary.cxx
)

target_include_directories(LibNativeLoad
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(LibNativeLoad PUBLIC cxx_std_20)

if(CMAKE_DL_LIBS)
    target_link_libraries(LibNativeLoad PRIVATE ${CMAKE_DL_LIBS})
endif()

if(MSVC)
    target_compile_options(LibNativeLoad PRIVATE /W3 /Zc:preprocessor)
    target_compile_definitions(LibNativeLoad PRIVATE
        _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS
    )
endif()
