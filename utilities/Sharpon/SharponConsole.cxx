#include "SharponConsole.hxx"

#include <algorithm>
#include <imgui.h>

void Sharpon_DrawSystemShellTab(SharponEmbeddedShell& shell, const std::filesystem::path& workDirHint) {
    std::error_code ec;
    std::filesystem::path cwd = workDirHint;
    if (cwd.empty() || !std::filesystem::is_directory(cwd, ec)) {
        cwd = std::filesystem::current_path(ec);
    }
    shell.ensureStarted(cwd);

    if (!shell.startError().empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.2f, 1.0f), "%s", std::string(shell.startError()).c_str());
        ImGui::TextWrapped(
            "A full interactive shell requires a pseudo-terminal (ConPTY on Windows 10 1809+, or PTY on Unix).");
    }

    const float inH = ImGui::GetFrameHeightWithSpacing();
    const float ch = std::max(1.0f, ImGui::GetTextLineHeight());
    const float cw = std::max(1.0f, ImGui::CalcTextSize("M").x);
    const ImVec2 r = ImGui::GetContentRegionAvail();
    const int cols = std::max(2, static_cast<int>(r.x / cw));
    const int rows = std::max(1, static_cast<int>((r.y - inH) / ch));
    shell.setTerminalSize(cols, rows);

    const float outH = std::max(4.0f, r.y - inH);
    ImGui::BeginChild("##shell_out", ImVec2(0, outH), true, ImGuiWindowFlags_HorizontalScrollbar);
    const std::string& t = shell.viewText();
    if (!t.empty()) {
        ImGui::TextUnformatted(t.c_str());
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();

    static char inbuf[4096] = {0};
    ImGui::SetNextItemWidth(-1.0f);
    const bool submit = ImGui::InputText("##shellin", inbuf, sizeof(inbuf), ImGuiInputTextFlags_EnterReturnsTrue);
    if (submit && inbuf[0] != '\0') {
        shell.writeLine(inbuf);
        inbuf[0] = '\0';
    }
}
