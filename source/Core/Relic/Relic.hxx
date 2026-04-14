#pragma once

#include "Solstice.hxx"
#include "AssetService.hxx"
#include "VirtualTable.hxx"
#include <filesystem>

namespace Solstice::Core::Relic {

// Global RELIC subsystem: virtual table + asset service. Initialized from Init() before other systems.
SOLSTICE_API bool Initialize(const std::filesystem::path& basePath);
SOLSTICE_API void Shutdown();
SOLSTICE_API bool IsInitialized();
SOLSTICE_API AssetService* GetAssetService();
SOLSTICE_API VirtualTable* GetVirtualTable();

} // namespace Solstice::Core::Relic
