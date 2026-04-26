#include "PluginLoader.hxx"

#include "UtilityPluginAbi.hxx"
#include "UtilityPluginHost.hxx"
#include "UtilityPluginUi.hxx"

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
    Solstice::UtilityPluginHost::DrawPluginManagerWindow(g_PluginHost, pOpen, "Plugins##Sharpon", "Sharpon",
        g_PluginLoadErrors, [] { SharponPlugins_LoadDefault(); });
}
