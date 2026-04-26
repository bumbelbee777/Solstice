#include "LibUI/Tools/ToolScaffolds/ComingSoonPanel.hxx"

#include <imgui.h>

namespace LibUI::Tools::ToolScaffolds {

void DrawComingSoonPanel(const char* title, bool* visible, const char* summary, const char* plannedScope) {
    if (visible && !(*visible)) {
        return;
    }
    if (!ImGui::Begin(title ? title : "Coming Soon", visible)) {
        ImGui::End();
        return;
    }
    ImGui::TextUnformatted(summary ? summary : "Scaffolded panel placeholder.");
    ImGui::Separator();
    ImGui::TextWrapped("%s", plannedScope ? plannedScope : "Implementation intentionally deferred.");
    ImGui::End();
}

} // namespace LibUI::Tools::ToolScaffolds

