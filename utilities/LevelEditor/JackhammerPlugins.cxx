#include "JackhammerPlugins.hxx"

#include "UtilityPluginAbi.hxx"
#include "UtilityPluginHost.hxx"
#include "UtilityPluginHost/UtilityPluginUi.hxx"

#include <SDL3/SDL.h>

#include <exception>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace Jackhammer {

namespace {

std::vector<std::pair<std::string, std::string>>& LevelPluginLoadErrorsState() {
    static std::vector<std::pair<std::string, std::string>> errs;
    return errs;
}

Solstice::UtilityPluginHost::UtilityPluginHost& LevelEditorPlugins() {
    // Function-local static avoids startup work before crash diagnostics are installed in main().
    static Solstice::UtilityPluginHost::UtilityPluginHost host;
    return host;
}

} // namespace

void LoadLevelEditorPlugins() {
    auto& pluginLoadErrors = LevelPluginLoadErrorsState();
    pluginLoadErrors.clear();
    try {
        const char* base = SDL_GetBasePath();
        std::filesystem::path dir = base ? std::filesystem::path(base) / "plugins" : std::filesystem::path("plugins");
        Solstice::UtilityPluginHost::PluginAbiSymbols abi{};
        abi.GetName = SOLSTICE_UTILITY_ABI_LEVEL_EDITOR_GETNAME;
        abi.OnLoad = SOLSTICE_UTILITY_ABI_LEVEL_EDITOR_ONLOAD;
        abi.OnUnload = SOLSTICE_UTILITY_ABI_LEVEL_EDITOR_ONUNLOAD;
        auto& plugins = LevelEditorPlugins();
        plugins.UnloadAll();
        plugins.LoadAllFromDirectory(dir.string(), abi, pluginLoadErrors);
    } catch (const std::exception& ex) {
        pluginLoadErrors.push_back({"plugins/", std::string("Reload failed: ") + ex.what()});
    } catch (...) {
        pluginLoadErrors.push_back({"plugins/", "Reload failed: unknown exception."});
    }
}

void LevelEditorPluginsDrawPanel(bool* pOpen) {
    Solstice::UtilityPluginHost::DrawPluginManagerWindow(LevelEditorPlugins(), pOpen, "Plugins##Jackhammer", "LevelEditor",
        LevelPluginLoadErrorsState(), [] { LoadLevelEditorPlugins(); });
}

} // namespace Jackhammer
