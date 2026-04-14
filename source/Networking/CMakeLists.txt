# Networking: transport layer and packet/socket abstractions.
set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
add_library(Networking STATIC
    NetworkingSystem.cxx
    Socket.cxx
)
target_include_directories(Networking PUBLIC ${SOURCE_DIR})
target_link_libraries(Networking PUBLIC Core PRIVATE GameNetworkingSockets::static Plugin)
if(MSVC)
    target_compile_options(Networking PRIVATE /Zc:preprocessor /W0)
    target_compile_definitions(Networking PRIVATE SOLSTICE_EXPORTS _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()
