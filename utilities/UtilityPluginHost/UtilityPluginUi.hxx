#pragma once

#include "UtilityPluginHost.hxx"

#include <imgui.h>

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace Solstice::UtilityPluginHost {

/// Shared **Plugins** floating window for LevelEditor / MovieMaker / other utilities (native DLLs in ``./plugins``).
void DrawPluginManagerWindow(UtilityPluginHost& host, bool* pOpen, const char* imguiWindowTitle,
    const char* executableNameForBlurb, std::vector<std::pair<std::string, std::string>>& loadErrors,
    const std::function<void()>& reloadPlugins);

} // namespace Solstice::UtilityPluginHost
