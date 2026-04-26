#include "LibUI/CurveEditor/CurveEditor.hxx"

#include <imgui.h>

#include <algorithm>

namespace LibUI::CurveEditor {

void DrawCurveEditor(const char* title, CurveEditorState& state, bool* visible) {
    if (visible && !(*visible)) {
        return;
    }
    const char* windowTitle = (title && title[0]) ? title : "Curve Editor";
    if (!ImGui::Begin(windowTitle, visible)) {
        ImGui::End();
        return;
    }
    state.zoomX = std::clamp(state.zoomX, 0.1f, 32.0f);
    state.zoomY = std::clamp(state.zoomY, 0.1f, 32.0f);
    ImGui::SliderFloat("Time zoom", &state.zoomX, 0.1f, 32.0f, "%.2fx");
    ImGui::SliderFloat("Value zoom", &state.zoomY, 0.1f, 32.0f, "%.2fx");
    ImGui::Separator();
    ImGui::TextUnformatted("Channels");
    for (size_t i = 0; i < state.channels.size(); ++i) {
        CurveChannel& channel = state.channels[i];
        ImGui::Checkbox(("##visible_" + channel.name).c_str(), &channel.visible);
        ImGui::SameLine();
        if (ImGui::Selectable(channel.name.c_str(), state.selectedChannel == static_cast<int>(i))) {
            state.selectedChannel = static_cast<int>(i);
        }
    }
    if (state.channels.empty()) {
        ImGui::TextDisabled("No channels yet. Curve editor scaffolding ready.");
    }
    ImGui::End();
}

} // namespace LibUI::CurveEditor

