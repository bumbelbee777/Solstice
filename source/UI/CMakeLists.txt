# UI: ImGui integration, widgets, animation, viewport UI
set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(LIBPARALLAX_INCLUDE "${CMAKE_SOURCE_DIR}/SDK/LibParallax/include")
set(UI_SOURCES
    Core/Window.cxx
    Core/UISystem.cxx
    Widgets/Widgets.cxx
    Widgets/TextEffects.cxx
    Widgets/Transparency.cxx
    Viewport/ViewportUI.cxx
    Media/ImageLoader.cxx
    Motion/Sprite.cxx
    Motion/Animation.cxx
    Motion/Transition.cxx
    Motion/Primitives.cxx
    Motion/VisualEffects.cxx
    Motion/MotionGraphics.cxx
    Motion/MotionGraphicsCompositor.cxx
    Motion/SpritePhysics2D.cxx
)
if(TARGET Multimedia)
    list(APPEND UI_SOURCES Media/VideoTexture.cxx)
endif()
add_library(UI STATIC ${UI_SOURCES})
target_include_directories(UI PUBLIC ${SOURCE_DIR} ${LIBPARALLAX_INCLUDE})
target_link_libraries(UI PUBLIC Core Math Render MinGfx)
if(TARGET Multimedia)
    target_link_libraries(UI PUBLIC Multimedia)
endif()
if(MSVC)
    target_compile_options(UI PRIVATE /Zc:preprocessor /W0)
    target_compile_definitions(UI PRIVATE SOLSTICE_EXPORTS _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()
source_group("Core" FILES Core/Window.cxx Core/UISystem.cxx Core/Window.hxx Core/UISystem.hxx)
source_group("Widgets" FILES Widgets/Widgets.cxx Widgets/Widgets.hxx Widgets/TextEffects.cxx Widgets/TextEffects.hxx Widgets/Transparency.cxx Widgets/Transparency.hxx)
source_group("Viewport" FILES Viewport/ViewportUI.cxx Viewport/ViewportUI.hxx)
source_group("Media" FILES Media/ImageLoader.cxx Media/ImageLoader.hxx)
if(TARGET Multimedia)
    source_group("Media" FILES Media/VideoTexture.cxx Media/VideoTexture.hxx)
endif()
source_group("Motion" FILES
    Motion/Sprite.cxx Motion/Sprite.hxx
    Motion/Animation.cxx Motion/Animation.hxx
    Motion/Transition.cxx Motion/Transition.hxx
    Motion/Primitives.cxx Motion/Primitives.hxx
    Motion/VisualEffects.cxx Motion/VisualEffects.hxx
    Motion/MotionGraphics.cxx Motion/MotionGraphics.hxx
    Motion/MotionGraphicsCompositor.cxx Motion/MotionGraphicsCompositor.hxx
    Motion/SpritePhysics2D.cxx Motion/SpritePhysics2D.hxx)
