# Native plugin loading (shared libraries) and optional IPlugin interface.
set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
add_library(Plugin STATIC
    DynamicLibrary.cxx
    Plugin.cxx
    PluginManager.cxx
    PluginMessageBus.cxx
    SubsystemHooks.cxx
)
target_include_directories(Plugin PUBLIC ${SOURCE_DIR})
target_link_libraries(Plugin PUBLIC Core)
if(CMAKE_DL_LIBS)
    target_link_libraries(Plugin PRIVATE ${CMAKE_DL_LIBS})
endif()
if(MSVC)
    target_compile_options(Plugin PRIVATE /Zc:preprocessor /W0)
    target_compile_definitions(Plugin PRIVATE SOLSTICE_EXPORTS _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()
