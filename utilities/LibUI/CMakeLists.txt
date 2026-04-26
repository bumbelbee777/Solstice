cmake_minimum_required(VERSION 3.20)

# LibUI - Standalone UI library for editor tools
project(LibUI VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Static library so Dear ImGui is linked once into each tool EXE. A shared LibUI + PUBLIC imgui duplicates imgui and
# leaves two GImGui instances (DLL vs EXE), which crashes inside LibUI::Core::Initialize on ImGui::GetIO().
add_library(LibUI STATIC
    Core/Core.cxx
    Tools/DiagLog.cxx
    Tools/Win32ExceptionDiag.cxx
    Tools/OpenGlDebug.cxx
    Tools/RecentPathsUi.cxx
    Tools/ClipboardButton.cxx
    Tools/UnsavedChangesModal.cxx
    Tools/FileChangedOnDiskModal.cxx
    Tools/AppAbout.cxx
    Tools/RgbaImageFile.cxx
    Tools/IniFile.cxx
    Tools/ViewportSpatialPick.cxx
    Tools/ToolScaffolds/ComingSoonPanel.cxx
    Widgets/Widgets.cxx
    Graphics/Graphics.cxx
    Graphics/PreviewTexture.cxx
    Shell/Frame.cxx
    Shell/DropFile.cxx
    Shell/GlWindow.cxx
    Shell/MainHost.cxx
    Docking/Docking.cxx
    Workspace/PanelRegistry.cxx
    Layout/SplitPane.cxx
    Viewport/Viewport.cxx
    Viewport/ViewportMath.cxx
    Viewport/ViewportGizmo.cxx
    Animation/Animation.cxx
    Timeline/TimelineWidget.cxx
    CurveEditor/CurveEditor.cxx
    Input/CommandRegistry.cxx
    Theme/Theme.cxx
    Icons/Icons.cxx
    FileDialogs/FileDialogs.cxx
    AssetBrowser/AssetBrowser.cxx
)

# Set up include directories
# Include directory is set to parent (utilities) so consumers can use "LibUI/Core/Core.hxx"
target_include_directories(LibUI
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<INSTALL_INTERFACE:include>
)

# Link against ImGui and SDL3 (OpenGL for GLSL / version query in Core)
target_link_libraries(LibUI PUBLIC
    imgui
    SDL3-shared
    PRIVATE
    tinygltf
)
if(WIN32)
    target_link_libraries(LibUI PRIVATE opengl32 dbghelp)
elseif(APPLE)
    target_link_libraries(LibUI PRIVATE "-framework" "OpenGL")
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(LibUI PRIVATE OpenGL::GL)
endif()

# Platform-specific configuration
if(MSVC)
    target_compile_options(LibUI PRIVATE /W0)  # Disable warnings for now
    target_compile_definitions(LibUI PRIVATE _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)
    target_compile_definitions(LibUI PRIVATE _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS)
    target_compile_definitions(LibUI PRIVATE NOMINMAX)
    target_compile_definitions(LibUI PRIVATE WIN32_LEAN_AND_MEAN)
    target_compile_definitions(LibUI PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    # For non-MSVC compilers
    target_compile_options(LibUI PRIVATE -Wno-unused-parameter)
endif()

target_compile_definitions(LibUI PUBLIC LIBUI_STATIC)

# Set C++ standard features
target_compile_features(LibUI PRIVATE cxx_std_20)

# Set target properties
set_target_properties(LibUI PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    ARCHIVE_OUTPUT_DIRECTORY "${SOLSTICE_ARCHIVE_OUTPUT_DIR}"
)

# Organize files in IDE
source_group("Core" FILES Core/Core.hxx Core/Core.cxx)
source_group("Tools" FILES Tools/DiagLog.hxx Tools/DiagLog.cxx Tools/Win32ExceptionDiag.hxx Tools/Win32ExceptionDiag.cxx Tools/RecentPathsUi.hxx Tools/RecentPathsUi.cxx
    Tools/ClipboardButton.hxx Tools/ClipboardButton.cxx
    Tools/UnsavedChangesModal.hxx Tools/UnsavedChangesModal.cxx Tools/FileChangedOnDiskModal.hxx Tools/FileChangedOnDiskModal.cxx
    Tools/AppAbout.hxx Tools/AppAbout.cxx
    Tools/RgbaImageFile.hxx Tools/RgbaImageFile.cxx Tools/ViewportSpatialPick.hxx Tools/ViewportSpatialPick.cxx
    Tools/ToolScaffolds/ComingSoonPanel.hxx Tools/ToolScaffolds/ComingSoonPanel.cxx)
source_group("Shell" FILES Shell/Frame.hxx Shell/Frame.cxx Shell/DropFile.hxx Shell/DropFile.cxx Shell/GlWindow.hxx Shell/GlWindow.cxx Shell/MainHost.hxx Shell/MainHost.cxx)
source_group("Docking" FILES Docking/Docking.hxx Docking/Docking.cxx)
source_group("Workspace" FILES Workspace/PanelRegistry.hxx Workspace/PanelRegistry.cxx)
source_group("Layout" FILES Layout/SplitPane.hxx Layout/SplitPane.cxx)
source_group("Widgets" FILES Widgets/Widgets.hxx Widgets/Widgets.cxx)
source_group("Graphics" FILES Graphics/Graphics.hxx Graphics/Graphics.cxx Graphics/PreviewTexture.hxx Graphics/PreviewTexture.cxx)
source_group("Viewport" FILES Viewport/Viewport.hxx Viewport/Viewport.cxx Viewport/ViewportMath.hxx Viewport/ViewportMath.cxx Viewport/ViewportGizmo.hxx Viewport/ViewportGizmo.cxx)
source_group("Timeline" FILES Timeline/TimelineModel.hxx Timeline/TimelineWidget.hxx Timeline/TimelineWidget.cxx)
source_group("CurveEditor" FILES CurveEditor/CurveModel.hxx CurveEditor/CurveEditor.hxx CurveEditor/CurveEditor.cxx)
source_group("Input" FILES Input/CommandRegistry.hxx Input/CommandRegistry.cxx)
source_group("Theme" FILES Theme/Tokens.hxx Theme/Theme.hxx Theme/Theme.cxx)
source_group("Animation" FILES Animation/Animation.hxx Animation/Animation.cxx)
source_group("Icons" FILES Icons/Icons.hxx Icons/Icons.cxx)
source_group("FileDialogs" FILES FileDialogs/FileDialogs.hxx FileDialogs/FileDialogs.cxx)
source_group("AssetBrowser" FILES AssetBrowser/AssetBrowser.hxx AssetBrowser/AssetBrowser.cxx)
source_group("Undo" FILES Undo/SnapshotStack.hxx)

