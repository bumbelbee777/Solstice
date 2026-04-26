#pragma once

#include "LibUI/Core/Core.hxx"

#include <imgui.h>

namespace LibUI::Shell {

/// Fullscreen dock root matching Jackhammer: menu bar, no title bar, no move/resize.
LIBUI_API ImGuiWindowFlags MainHostFlags_MenuBarNoTitle();

/// SMM-style root: no decoration, no move, no bring-to-front on focus.
LIBUI_API ImGuiWindowFlags MainHostFlags_NoDecoration();

/// Sharpon-style root: menu bar + no title bar + no nav focus (caller may OR extra flags).
LIBUI_API ImGuiWindowFlags MainHostFlags_SharponEditor();

LIBUI_API void BeginMainHostWindow(const char* imguiId, ImGuiWindowFlags flags);
LIBUI_API void EndMainHostWindow();

} // namespace LibUI::Shell
