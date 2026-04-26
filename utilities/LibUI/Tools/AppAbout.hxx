#pragma once

#include "LibUI/Core/Core.hxx"

#include <imgui.h>

namespace LibUI::Tools {

/// Content for the shared **About** floating window (utilities use the same layout).
struct AboutWindowContent {
    /// ImGui window title, e.g. ``"About Jackhammer"``.
    const char* windowTitle = "";
    /// First line under the title bar (often ``"Technology Preview 1"``).
    const char* headline = nullptr;
    /// Optional subtitle between headline and body (e.g. tool family name); may be nullptr.
    const char* subtitle = nullptr;
    /// Wrapped paragraph; may be nullptr.
    const char* body = nullptr;
    /// Small footnote (build id, git hash); shown disabled. May be nullptr.
    const char* footnote = nullptr;
};

/// Standard floating **About** window. Skips drawing when ``pOpen`` is null or false.
LIBUI_API void DrawAboutWindow(bool* pOpen, const AboutWindowContent& content, const ImVec2& firstUseSize = ImVec2(440, 220));

/// Gold accent line for **Technology Preview** (title bars, banners).
LIBUI_API void DrawTechnologyPreviewHeadline(const char* label = "Technology Preview 1");

} // namespace LibUI::Tools
