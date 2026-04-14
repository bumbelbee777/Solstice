#pragma once

// Sharpon loads optional native plugins from a `plugins/` folder next to the executable.
// See Plugin.hxx for the exported symbol names the host resolves (SharponPlugin_GetName, etc.).

#include <string>
#include <vector>

struct SharponLoadedPlugin {
    std::string Path;
    std::string Name;
    void* Module{nullptr}; // HMODULE or dlopen handle
};

void SharponPlugins_LoadDefault();
void SharponPlugins_UnloadAll();
void SharponPlugins_DrawPanel(bool* pOpen);
