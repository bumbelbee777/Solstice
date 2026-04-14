#include "SharponConsole.hxx"

#include <imgui.h>

void Sharpon_DrawConsolePanel(std::vector<std::string>& scrollback, char* inputBuf, size_t inputBufSize,
                                void (*onLine)(void* user, const char* line), void* userData, bool* pOpen) {
    if (pOpen && !*pOpen) {
        return;
    }
    ImGui::SetNextWindowSize(ImVec2(520, 280), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Console", pOpen)) {
        ImGui::TextUnformatted("Commands: help | compile | run | clear");
        ImGui::Separator();
        ImGui::BeginChild("##scroll", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), true);
        for (const auto& line : scrollback) {
            ImGui::TextUnformatted(line.c_str());
        }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.f) {
            ImGui::SetScrollHereY(1.f);
        }
        ImGui::EndChild();
        ImGui::SetNextItemWidth(-1);
        const bool submit = ImGui::InputText("##line", inputBuf, inputBufSize,
                                              ImGuiInputTextFlags_EnterReturnsTrue);
        if (submit && inputBuf[0] != '\0' && onLine) {
            onLine(userData, inputBuf);
            inputBuf[0] = '\0';
        }
    }
    ImGui::End();
}
