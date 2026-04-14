cmake_minimum_required(VERSION 3.20)
project(UtilityPluginHost)

add_library(UtilityPluginHost STATIC
    DynamicLibrary.cxx
    UtilityPluginHost.cxx
)

target_include_directories(UtilityPluginHost PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_features(UtilityPluginHost PUBLIC cxx_std_20)

set_target_properties(UtilityPluginHost PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

if(CMAKE_DL_LIBS)
    target_link_libraries(UtilityPluginHost PRIVATE ${CMAKE_DL_LIBS})
endif()
