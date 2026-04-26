#pragma once

#include "LibUI/Core/Core.hxx"

#include <imgui.h>

namespace LibUI::Layout {

struct SplitPaneState {
    float ratio{0.5f};
    float minRatio{0.1f};
    float maxRatio{0.9f};
};

LIBUI_API bool BeginVerticalSplit(const char* id, SplitPaneState& state, ImVec2 size = ImVec2(0.0f, 0.0f));
LIBUI_API bool NextVerticalSplitPane(const char* id, SplitPaneState& state);
LIBUI_API void EndVerticalSplit();

} // namespace LibUI::Layout

