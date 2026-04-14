cmake_minimum_required(VERSION 3.20)
project(EditorEnginePreview)

add_library(EditorEnginePreview STATIC
    EditorEnginePreview.cxx
)

target_include_directories(EditorEnginePreview PUBLIC
    "${CMAKE_SOURCE_DIR}/utilities"
)

target_link_libraries(EditorEnginePreview PUBLIC LibUI SolsticeEngine)

target_compile_features(EditorEnginePreview PUBLIC cxx_std_20)

set_target_properties(EditorEnginePreview PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
