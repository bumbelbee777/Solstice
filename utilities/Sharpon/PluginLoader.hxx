#pragma once

// Sharpon loads optional native plugins from a `plugins/` folder next to the executable.
// See utilities/UtilityPluginHost/UtilityPluginAbi.hxx for exported symbol names.

void SharponPlugins_LoadDefault();
void SharponPlugins_UnloadAll();
void SharponPlugins_DrawPanel(bool* pOpen);
