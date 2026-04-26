#include "LibUI/Tools/AppAbout.hxx"

#include <imgui.h>

namespace LibUI::Tools {

void DrawTechnologyPreviewHeadline(const char* label) {
    if (!label || !label[0]) {
        return;
    }
    ImGui::TextColored(ImVec4(1.f, 0.85f, 0.35f, 1.f), "%s", label);
}

void DrawAboutWindow(bool* pOpen, const AboutWindowContent& content, const ImVec2& firstUseSize) {
    if (!pOpen || !*pOpen || !content.windowTitle || !content.windowTitle[0]) {
        return;
    }
    ImGui::SetNextWindowSize(firstUseSize, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(content.windowTitle, pOpen)) {
        ImGui::End();
        return;
    }
    if (content.headline && content.headline[0]) {
        DrawTechnologyPreviewHeadline(content.headline);
    }
    if (content.subtitle && content.subtitle[0]) {
        ImGui::TextUnformatted(content.subtitle);
    }
    if ((content.headline && content.headline[0]) || (content.subtitle && content.subtitle[0])) {
        ImGui::Separator();
    }
    if (content.body && content.body[0]) {
        ImGui::TextWrapped("%s", content.body);
    }
    if (content.footnote && content.footnote[0]) {
        ImGui::Separator();
        ImGui::TextDisabled("%s", content.footnote);
    }
    ImGui::End();
}

} // namespace LibUI::Tools
