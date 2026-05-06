#include "LibUI/Layout/SplitPane.hxx"

#include <algorithm>
#include <string>

namespace LibUI::Layout {

bool BeginVerticalSplit(const char* id, SplitPaneState& state, ImVec2 size) {
    state.ratio = std::clamp(state.ratio, state.minRatio, state.maxRatio);
    return ImGui::BeginChild(id, size, false, ImGuiWindowFlags_None);
}

bool NextVerticalSplitPane(const char* id, SplitPaneState& state) {
    const float availY = ImGui::GetContentRegionAvail().y;
    const float topY = std::max(1.0f, availY * state.ratio);
    ImGui::BeginChild((std::string(id) + "_Top").c_str(), ImVec2(0.0f, topY), true, ImGuiWindowFlags_None);
    ImGui::EndChild();
    ImGui::Separator();
    return ImGui::BeginChild((std::string(id) + "_Bottom").c_str(), ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_None);
}

void EndVerticalSplit() {
    ImGui::EndChild();
}

bool BeginThreePaneWorkspace(const char* id, const ThreePaneWorkspaceConfig& config, float* outCenterWidth) {
    if (!id) {
        return false;
    }
    if (!ImGui::BeginChild(id, ImVec2(0.f, -config.bottomBarHeight), false, config.workspaceFlags)) {
        return false;
    }

    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const float availW = ImGui::GetContentRegionAvail().x;
    float centerW = availW - config.leftWidth - config.rightWidth - gap * 2.f;
    if (centerW < config.minCenterWidth) {
        centerW = std::max(config.minFallbackCenterWidth, availW * config.fallbackCenterRatio);
    }
    if (outCenterWidth) {
        *outCenterWidth = centerW;
    }
    return true;
}

void EndThreePaneWorkspace() {
    ImGui::EndChild();
}

bool BeginTwoPaneFixedLeftTable(const char* id, float leftWidth, ImGuiTableFlags flags) {
    if (!id) {
        return false;
    }
    if (!ImGui::BeginTable(id, 2, flags)) {
        return false;
    }
    ImGui::TableSetupColumn("Left", ImGuiTableColumnFlags_WidthFixed, leftWidth);
    ImGui::TableSetupColumn("Main", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    return true;
}

void EndTwoPaneFixedLeftTable() {
    ImGui::EndTable();
}

} // namespace LibUI::Layout

