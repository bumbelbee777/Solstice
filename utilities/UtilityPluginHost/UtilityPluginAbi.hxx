#pragma once

/// C ABI symbol names for optional native plugins loaded from `plugins/` next to each utility executable.
/// Export from the plugin (extern "C"):
///
/// **Sharpon**
///   `SharponPlugin_GetName` — display name (recommended).
///   `SharponPlugin_OnLoad` / `SharponPlugin_OnUnload` — optional lifecycle.
///
/// **Jackhammer (LevelEditor)**
///   `LevelEditorPlugin_GetName`, `LevelEditorPlugin_OnLoad`, `LevelEditorPlugin_OnUnload`
///
/// **SMM (MovieMaker)**
///   `MovieMakerPlugin_GetName`, `MovieMakerPlugin_OnLoad`, `MovieMakerPlugin_OnUnload`

#define SOLSTICE_UTILITY_ABI_SHARPON_GETNAME "SharponPlugin_GetName"
#define SOLSTICE_UTILITY_ABI_SHARPON_ONLOAD "SharponPlugin_OnLoad"
#define SOLSTICE_UTILITY_ABI_SHARPON_ONUNLOAD "SharponPlugin_OnUnload"

#define SOLSTICE_UTILITY_ABI_LEVEL_EDITOR_GETNAME "LevelEditorPlugin_GetName"
#define SOLSTICE_UTILITY_ABI_LEVEL_EDITOR_ONLOAD "LevelEditorPlugin_OnLoad"
#define SOLSTICE_UTILITY_ABI_LEVEL_EDITOR_ONUNLOAD "LevelEditorPlugin_OnUnload"

#define SOLSTICE_UTILITY_ABI_MOVIE_MAKER_GETNAME "MovieMakerPlugin_GetName"
#define SOLSTICE_UTILITY_ABI_MOVIE_MAKER_ONLOAD "MovieMakerPlugin_OnLoad"
#define SOLSTICE_UTILITY_ABI_MOVIE_MAKER_ONUNLOAD "MovieMakerPlugin_OnUnload"
