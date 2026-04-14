cmake_minimum_required(VERSION 3.20)

project(LibParallax LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(LIBPARALLAX_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(LIBPARALLAX_INCLUDE "${LIBPARALLAX_DIR}/include")

add_library(LibParallax STATIC
    src/ParallaxScene.cxx
    src/ParallaxEditOps.cxx
    src/ParallaxBinary.cxx
    src/ParallaxEvaluate.cxx
    src/ParallaxIntegration.cxx
    src/ParallaxFrameExport.cxx
    src/MGRaster.cxx
    src/RelicAssetResolver.cxx
    src/DevSessionAssetResolver.cxx
)

target_include_directories(LibParallax
    PUBLIC
        $<BUILD_INTERFACE:${LIBPARALLAX_INCLUDE}>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/source
        ${CMAKE_SOURCE_DIR}/3rdparty/tinygltf
        ${CMAKE_SOURCE_DIR}/3rdparty/stb
)

target_link_libraries(LibParallax PUBLIC
    Core
    Math
    MinGfx
    Skeleton
    Arzachel
    Physics
    Scripting
    Render
    Entity
)
target_link_libraries(LibParallax PRIVATE LibSmf)

if(DEFINED zstd_SOURCE_DIR)
    target_include_directories(LibParallax PRIVATE "${zstd_SOURCE_DIR}/lib")
endif()

if(MSVC)
    target_compile_options(LibParallax PRIVATE /W0 /Zc:preprocessor)
    target_compile_definitions(LibParallax PRIVATE
        SOLSTICE_STATIC_LINK
        _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS
    )
endif()

target_compile_features(LibParallax PUBLIC cxx_std_20)
