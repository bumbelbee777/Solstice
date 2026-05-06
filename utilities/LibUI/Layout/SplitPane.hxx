#pragma once

#include "LibUI/Core/Core.hxx"

#include <imgui.h>

namespace LibUI::Layout {

struct SplitPaneState {
    float ratio{0.5f};
    float minRatio{0.1f};
    float maxRatio{0.9f};
};

struct ThreePaneWorkspaceConfig {
    float leftWidth{272.0f};
    float rightWidth{308.0f};
    float bottomBarHeight{86.0f};
    float minCenterWidth{120.0f};
    float fallbackCenterRatio{0.28f};
    float minFallbackCenterWidth{80.0f};
    ImGuiWindowFlags workspaceFlags{ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse};
};

LIBUI_API bool BeginVerticalSplit(const char* id, SplitPaneState& state, ImVec2 size = ImVec2(0.0f, 0.0f));
LIBUI_API bool NextVerticalSplitPane(const char* id, SplitPaneState& state);
LIBUI_API void EndVerticalSplit();
LIBUI_API bool BeginThreePaneWorkspace(const char* id, const ThreePaneWorkspaceConfig& config, float* outCenterWidth = nullptr);
LIBUI_API void EndThreePaneWorkspace();
LIBUI_API bool BeginTwoPaneFixedLeftTable(const char* id, float leftWidth,
    ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV);
LIBUI_API void EndTwoPaneFixedLeftTable();

} // namespace LibUI::Layout

