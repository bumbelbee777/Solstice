#include "Relic.hxx"
#include "Core/Debug/Debug.hxx"
#include <memory>

namespace Solstice::Core::Relic {

static std::unique_ptr<VirtualTable> s_VirtualTable;
static std::unique_ptr<AssetService> s_AssetService;

bool Initialize(const std::filesystem::path& basePath) {
    Shutdown();
    s_VirtualTable = std::make_unique<VirtualTable>();
    if (!s_VirtualTable->Initialize(basePath)) {
        s_VirtualTable.reset();
        return false;
    }
    s_AssetService = std::make_unique<AssetService>();
    s_AssetService->SetVirtualTable(s_VirtualTable.get());
    return true;
}

void Shutdown() {
    s_AssetService.reset();
    s_VirtualTable.reset();
}

bool IsInitialized() {
    return s_VirtualTable != nullptr && s_VirtualTable->IsInitialized();
}

AssetService* GetAssetService() {
    return s_AssetService.get();
}

VirtualTable* GetVirtualTable() {
    return s_VirtualTable.get();
}

} // namespace Solstice::Core::Relic
