#include "PluginLoader.hxx"

#include "UtilityPluginAbi.hxx"
#include "UtilityPluginHost.hxx"

#include <SDL3/SDL.h>
#include <imgui.h>

#include <cctype>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace {

Solstice::UtilityPluginHost::UtilityPluginHost g_PluginHost;
std::vector<std::pair<std::string, std::string>> g_PluginLoadErrors;

} // namespace

void SharponPlugins_LoadDefault() {
    g_PluginHost.UnloadAll();
    g_PluginLoadErrors.clear();
    const char* base = SDL_GetBasePath();
    std::filesystem::path dir = base ? std::filesystem::path(base) / "plugins" : std::filesystem::path("plugins");
    if (base) {
        SDL_free((void*)base);
    }

    Solstice::UtilityPluginHost::PluginAbiSymbols abi{};
    abi.GetName = SOLSTICE_UTILITY_ABI_SHARPON_GETNAME;
    abi.OnLoad = SOLSTICE_UTILITY_ABI_SHARPON_ONLOAD;
    abi.OnUnload = SOLSTICE_UTILITY_ABI_SHARPON_ONUNLOAD;

    g_PluginHost.LoadAllFromDirectory(dir.string(), abi, g_PluginLoadErrors);
}

void SharponPlugins_UnloadAll() {
    g_PluginHost.UnloadAll();
    g_PluginLoadErrors.clear();
}

void SharponPlugins_DrawPanel(bool* pOpen) {
    if (pOpen && !*pOpen) {
        return;
    }
    ImGui::SetNextWindowSize(ImVec2(420, 220), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Plugins", pOpen)) {
        ImGui::TextUnformatted("Loaded from ./plugins next to Sharpon.exe");
        if (ImGui::Button("Reload")) {
            SharponPlugins_LoadDefault();
        }
        ImGui::Separator();
        if (!g_PluginLoadErrors.empty()) {
            ImGui::TextUnformatted("Failed to load (see path + system error):");
            for (const auto& fe : g_PluginLoadErrors) {
                ImGui::BulletText("%s\n  %s", fe.first.c_str(), fe.second.c_str());
            }
            ImGui::Separator();
        }
        std::vector<Solstice::UtilityPluginHost::ModuleSummary> mods;
        g_PluginHost.EnumerateModules(mods);
        if (mods.empty()) {
            ImGui::TextUnformatted("No plugins loaded.");
        }
        for (const auto& m : mods) {
            ImGui::BulletText("%s — %s", m.DisplayName.c_str(), m.PathUtf8.c_str());
        }
    }
    ImGui::End();
}
