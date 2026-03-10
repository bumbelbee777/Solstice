# UI: ImGui integration, widgets, animation, viewport UI
set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
add_library(UI STATIC
    Window.cxx
    UISystem.cxx
    Widgets.cxx
    TextEffects.cxx
    Transparency.cxx
    ViewportUI.cxx
    ImageLoader.cxx
    Sprite.cxx
    Animation.cxx
    Transition.cxx
    Primitives.cxx
    VisualEffects.cxx
    MotionGraphics.cxx
)
target_include_directories(UI PUBLIC ${SOURCE_DIR})
target_link_libraries(UI PUBLIC Core Math Render MinGfx)
if(MSVC)
    target_compile_options(UI PRIVATE /Zc:preprocessor /W0)
    target_compile_definitions(UI PRIVATE SOLSTICE_EXPORTS _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()
