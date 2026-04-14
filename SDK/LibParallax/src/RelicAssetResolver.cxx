#include <Parallax/RelicAssetResolver.hxx>

#include <Core/Relic/AssetService.hxx>
#include <cstring>

namespace Solstice::Parallax {

static uint64_t HashPathFNV1a(std::string_view path) {
    uint64_t h = 14695981039346656037ull;
    for (unsigned char c : path) {
        h ^= c;
        h *= 1099511628211ull;
    }
    return h;
}

RelicAssetResolver::RelicAssetResolver(Solstice::Core::Relic::AssetService& service) : m_Service(service) {}

void RelicAssetResolver::SetPathTable(std::unordered_map<std::string, uint64_t> pathToHash) {
    m_PathToHash = std::move(pathToHash);
}

bool RelicAssetResolver::Resolve(uint64_t assetHash, AssetData& outData) {
    auto opt = m_Service.LoadByHash(static_cast<Solstice::Core::Relic::AssetHash>(assetHash));
    if (!opt) {
        return false;
    }
    outData.Bytes.assign(opt->begin(), opt->end());
    return true;
}

uint64_t RelicAssetResolver::HashFromPath(std::string_view path) {
    std::string s(path);
    auto it = m_PathToHash.find(s);
    if (it != m_PathToHash.end()) {
        return it->second;
    }
    auto oh = m_Service.PathToHash(s);
    if (oh) {
        return *oh;
    }
    return HashPathFNV1a(path);
}

bool RelicAssetResolver::IsLoaded(uint64_t assetHash) const {
    return m_Service.IsLoaded(static_cast<Solstice::Core::Relic::AssetHash>(assetHash));
}

} // namespace Solstice::Parallax
