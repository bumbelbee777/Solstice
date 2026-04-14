#pragma once

/// Optional Sharpon plugin DLLs (loaded from `plugins/` next to the executable).
/// Export from the plugin:
///   extern "C" const char* SharponPlugin_GetName(void);
/// Optional:
///   extern "C" void SharponPlugin_OnLoad(void);
///   extern "C" void SharponPlugin_OnUnload(void);
