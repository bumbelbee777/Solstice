# Aggregate engine source into per-directory static libs, then build SolsticeEngine.
# Include order and link order matter: Math first, then Core, then libs that depend on them.
set(ENGINE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(Math)
add_subdirectory(Entity)
add_subdirectory(Skeleton)
add_subdirectory(MinGfx)
add_subdirectory(Core)
add_subdirectory(Arzachel)
add_subdirectory(Physics)
add_subdirectory(Render)
add_subdirectory(UI)
add_subdirectory(Scripting)
add_subdirectory(Game)

# SolsticeEngine: single shared library that links all engine parts
add_library(SolsticeEngine SHARED
    Init.cxx
    API/V1/Core.cxx
    API/V1/Audio.cxx
    API/V1/Scripting.cxx
    API/V1/Physics.cxx
)
target_include_directories(SolsticeEngine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${ENGINE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/SDK>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/Solstice
)
# Link engine static libs as whole archives so all SOLSTICE_API symbols are included
# in the DLL and exported (exes like PhysicsPlayground only link SolsticeEngine.lib).
set(ENGINE_LIBS Core Math Skeleton MinGfx Arzachel Game Physics Render UI Scripting Entity)
if(MSVC)
    foreach(LIB IN LISTS ENGINE_LIBS)
        # Only apply /WHOLEARCHIVE to STATIC libraries (skip INTERFACE libraries like Entity)
        target_link_options(SolsticeEngine PRIVATE
            "$<$<STREQUAL:$<TARGET_PROPERTY:${LIB},TYPE>,STATIC_LIBRARY>:/WHOLEARCHIVE:$<TARGET_FILE:${LIB}>>"
        )
    endforeach()
    target_link_libraries(SolsticeEngine PRIVATE ${ENGINE_LIBS})
else()
    # Filter out INTERFACE libraries for whole-archive (they don't produce files)
    set(STATIC_LIBS)
    set(INTERFACE_LIBS)
    foreach(LIB IN LISTS ENGINE_LIBS)
        get_target_property(LIB_TYPE ${LIB} TYPE)
        if(LIB_TYPE STREQUAL "STATIC_LIBRARY")
            list(APPEND STATIC_LIBS ${LIB})
        else()
            list(APPEND INTERFACE_LIBS ${LIB})
        endif()
    endforeach()
    target_link_libraries(SolsticeEngine PRIVATE
        -Wl,--whole-archive
        ${STATIC_LIBS}
        -Wl,--no-whole-archive
        ${INTERFACE_LIBS}  # Link INTERFACE libraries separately for include dirs/definitions
    )
endif()
if(MSVC)
    target_compile_options(SolsticeEngine PRIVATE /Zc:preprocessor /W0)
    target_compile_definitions(SolsticeEngine PRIVATE
        _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
        _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS
        SOLSTICE_EXPORTS
        SOLSTICE_BUILD_GIT_COMMIT="${GIT_COMMIT_HASH}"
    )
else()
    target_compile_definitions(SolsticeEngine PRIVATE
        SOLSTICE_EXPORTS
        SOLSTICE_BUILD_GIT_COMMIT="${GIT_COMMIT_HASH}"
    )
endif()
target_compile_features(SolsticeEngine PRIVATE cxx_std_20)
set_target_properties(SolsticeEngine PROPERTIES
    OUTPUT_NAME "$<$<PLATFORM_ID:Windows>:SolsticeEngine>$<$<NOT:$<PLATFORM_ID:Windows>>:solsticeengine>"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
