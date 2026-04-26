cmake_minimum_required(VERSION 3.20)

add_executable(JhSmfProjectGen JhSmfProjectGen.cxx)
target_include_directories(JhSmfProjectGen PRIVATE
    "${CMAKE_SOURCE_DIR}/SDK"
    "${CMAKE_SOURCE_DIR}/source"
)
target_link_libraries(JhSmfProjectGen PRIVATE LibSmf Material)
target_compile_features(JhSmfProjectGen PRIVATE cxx_std_20)
set_target_properties(JhSmfProjectGen PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${SOLSTICE_EXECUTABLE_OUTPUT_DIR}"
)
if(MSVC)
    target_compile_options(JhSmfProjectGen PRIVATE /Zc:preprocessor /W3)
    target_compile_definitions(JhSmfProjectGen PRIVATE
        _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
        _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS
    )
endif()
