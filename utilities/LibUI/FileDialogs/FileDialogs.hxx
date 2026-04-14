#pragma once

#include "../Core/Core.hxx"
#include <SDL3/SDL.h>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace LibUI::FileDialogs {

/// One row for SDL_ShowOpenFileDialog / SDL_ShowSaveFileDialog (`name`, `pattern` e.g. "prlx" or "*").
struct FileFilter {
    std::string Name;
    std::string Pattern;
};

/// Async SDL3 open-file dialog. If `filters` is empty, defaults to PARALLAX (.prlx) and All (*).
LIBUI_API void ShowOpenFile(SDL_Window* window, const char* title, std::function<void(std::optional<std::string>)> onResult,
    std::span<const FileFilter> filters = {});

/// Async SDL3 save-file dialog. If `filters` is empty, defaults to PARALLAX (.prlx) and All (*).
LIBUI_API void ShowSaveFile(SDL_Window* window, const char* title, std::function<void(std::optional<std::string>)> onResult,
    std::span<const FileFilter> filters = {});

} // namespace LibUI::FileDialogs
