#include "UtilityPluginUi.hxx"

#include <imgui.h>

namespace Solstice::UtilityPluginHost {

void DrawPluginManagerWindow(UtilityPluginHost& host, bool* pOpen, const char* imguiWindowTitle,
    const char* executableNameForBlurb, std::vector<std::pair<std::string, std::string>>& loadErrors,
    const std::function<void()>& reloadPlugins) {
    if (pOpen && !*pOpen) {
        return;
    }
    ImGui::SetNextWindowSize(ImVec2(440, 260), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(imguiWindowTitle, pOpen)) {
        ImGui::End();
        return;
    }
    ImGui::TextUnformatted("Native plugins: ./plugins next to");
    ImGui::SameLine();
    if (executableNameForBlurb && executableNameForBlurb[0]) {
        ImGui::TextUnformatted(executableNameForBlurb);
    } else {
        ImGui::TextUnformatted("this executable");
    }
    if (ImGui::Button("Reload##utilityplug")) {
        if (reloadPlugins) {
            reloadPlugins();
        }
    }
    ImGui::Separator();
    if (!loadErrors.empty()) {
        ImGui::TextUnformatted("Failed to load:");
        for (const auto& fe : loadErrors) {
            ImGui::BulletText("%s\n  %s", fe.first.c_str(), fe.second.c_str());
        }
        ImGui::Separator();
    }
    std::vector<ModuleSummary> mods;
    host.EnumerateModules(mods);
    if (mods.empty()) {
        ImGui::TextUnformatted("No plugins loaded.");
    }
    for (const auto& m : mods) {
        ImGui::BulletText("%s — %s", m.DisplayName.c_str(), m.PathUtf8.c_str());
    }
    ImGui::End();
}

} // namespace Solstice::UtilityPluginHost
