#pragma once

#include "IAssetResolver.hxx"
#include <memory>
#include <unordered_map>

namespace Solstice::Core::Relic {
class AssetService;
}

namespace Solstice::Parallax {

// Resolves assets via RELIC AssetService; optional path→hash for authoring.
class RelicAssetResolver final : public IAssetResolver {
public:
    explicit RelicAssetResolver(Solstice::Core::Relic::AssetService& service);

    void SetPathTable(std::unordered_map<std::string, uint64_t> pathToHash);

    bool Resolve(uint64_t assetHash, AssetData& outData) override;
    uint64_t HashFromPath(std::string_view path) override;
    bool IsLoaded(uint64_t assetHash) const override;

private:
    Solstice::Core::Relic::AssetService& m_Service;
    std::unordered_map<std::string, uint64_t> m_PathToHash;
};

} // namespace Solstice::Parallax
