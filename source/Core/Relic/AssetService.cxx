#include "AssetService.hxx"
#include "Decompress.hxx"
#include "Delta.hxx"
#include "../Debug.hxx"
#include "../Profiler.hxx"
#include <algorithm>

namespace Solstice::Core::Relic {

std::optional<std::vector<std::byte>> AssetService::LoadByHash(AssetHash hash) {
    {
        LockGuard guard(m_Lock);
        auto it = m_Cache.find(hash);
        if (it != m_Cache.end()) {
            Profiler::Instance().IncrementCounter("Relic.CacheHits", 1);
            return it->second;
        }
    }
    std::vector<std::byte> data = LoadByHashInternal(hash);
    if (data.empty()) {
        Profiler::Instance().IncrementCounter("Relic.CacheMisses", 1);
        return std::nullopt;
    }
    {
        LockGuard guard(m_Lock);
        Profiler::Instance().IncrementCounter("Relic.CacheMisses", 1);
        auto& entry = m_Cache[hash];
        entry = std::move(data);
        return m_Cache[hash];
    }
}

void AssetService::PrefetchCluster(uint32_t clusterId) {
    if (!m_Table) return;
    std::vector<AssetHash> hashes;
    m_Table->GetAssetHashesInCluster(clusterId, hashes);
    std::vector<AssetHash> toLoad;
    {
        LockGuard guard(m_Lock);
        for (AssetHash h : hashes) {
            if (m_Cache.find(h) == m_Cache.end()) toLoad.push_back(h);
        }
    }
    for (AssetHash h : toLoad) {
        LoadByHash(h);
    }
}

bool AssetService::IsLoaded(AssetHash hash) const {
    LockGuard guard(m_Lock);
    return m_Cache.find(hash) != m_Cache.end();
}

void AssetService::UnloadCluster(uint32_t clusterId) {
    std::vector<AssetHash> hashes;
    if (m_Table) {
        m_Table->GetAssetHashesInCluster(clusterId, hashes);
    }
    LockGuard guard(m_Lock);
    for (AssetHash h : hashes) {
        m_Cache.erase(h);
    }
}

std::optional<AssetHash> AssetService::PathToHash(const std::string& path) const {
    auto it = m_PathToHash.find(path);
    if (it == m_PathToHash.end()) return std::nullopt;
    return it->second;
}

void AssetService::ClearCache() {
    LockGuard guard(m_Lock);
    m_Cache.clear();
}

std::vector<std::byte> AssetService::LoadByHashInternal(AssetHash hash) {
    if (!m_Table) return {};
    auto resolved = m_Table->Find(hash);
    if (!resolved) return {};
    std::vector<std::byte> raw = m_Table->ReadAssetBytes(*resolved);
    if (raw.empty()) return {};
    CompressionType comp = GetCompressionType(resolved->Flags);
    std::vector<std::byte> decompressed = DecompressAsset(raw, comp, resolved->UncompressedSize);
    if (decompressed.empty()) return {};
    if (resolved->Flags & FlagIsDelta) {
        if (resolved->Dependencies.empty()) return {};
        AssetHash baseHash = resolved->Dependencies[0];
        std::optional<std::vector<std::byte>> baseOpt = LoadByHash(baseHash);
        if (!baseOpt) return {};
        decompressed = ApplyDelta(*baseOpt, decompressed);
    }
    return decompressed;
}

} // namespace Solstice::Core::Relic
