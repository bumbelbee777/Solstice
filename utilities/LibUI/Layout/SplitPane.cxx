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

} // namespace LibUI::Layout

