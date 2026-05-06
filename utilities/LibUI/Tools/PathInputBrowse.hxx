#pragma once

#include "LibUI/Core/Core.hxx"
#include "LibUI/FileDialogs/FileDialogs.hxx"

#include <SDL3/SDL.h>
#include <functional>
#include <span>
#include <string>

namespace LibUI::Tools {

/// One row: `InputTextWithHint` sized to the row minus `trailingReservePx`, plus a browse button that opens
/// an async open-file dialog and copies the result into `buf` (NUL-terminated, truncated).
LIBUI_API void InputPathOpenBrowseRowHint(float trailingReservePx, const char* inputId, const char* hint, char* buf,
    size_t bufSize, SDL_Window* window, const char* dialogTitle, const char* browseButtonLabel,
    std::span<const LibUI::FileDialogs::FileFilter> filters = {},
    const std::function<std::string(const std::string&)>& mapSelectedPath = {});

/// Same as InputPathOpenBrowseRowHint but edits a std::string directly.
/// If text is edited manually this frame, returns true.
LIBUI_API bool InputPathOpenBrowseRowHintString(float trailingReservePx, const char* inputId, const char* hint,
    std::string& value, size_t editBufferSize, SDL_Window* window, const char* dialogTitle, const char* browseButtonLabel,
    std::span<const LibUI::FileDialogs::FileFilter> filters = {},
    const std::function<std::string(const std::string&)>& mapSelectedPath = {},
    const std::function<void()>& onValueMutated = {});

} // namespace LibUI::Tools
