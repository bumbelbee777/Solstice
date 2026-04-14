#pragma once

#include "Solstice.hxx"
#include "Types.hxx"
#include "VirtualTable.hxx"
#include "Core/System/Async.hxx"
#include "Core/Platform/Cache.hxx"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <future>
#include <optional>
#include <cstdint>

namespace Solstice::Core::Relic {

// RELIC asset service: load by hash, prefetch by cluster, cache, unload.
// Uses VirtualTable, Decompress, Delta; caches decompressed/delta-applied bytes by hash.
class SOLSTICE_API AssetService {
public:
    AssetService() = default;

    void SetVirtualTable(VirtualTable* table) { m_Table = table; }
    VirtualTable* GetVirtualTable() const { return m_Table; }

    // Load asset bytes by hash (decompress + apply delta if needed). Returns cached or freshly loaded.
    std::optional<std::vector<std::byte>> LoadByHash(AssetHash hash);

    // Prefetch all assets in cluster (async). Dependencies are loaded in the same batch.
    void PrefetchCluster(uint32_t clusterId);

    bool IsLoaded(AssetHash hash) const;

    void UnloadCluster(uint32_t clusterId);

    // Path → hash mapping (optional, for path-based API compatibility). Set by VirtualTable or loader.
    void SetPathToHash(std::unordered_map<std::string, AssetHash> pathToHash) {
        m_PathToHash = std::move(pathToHash);
    }
    std::optional<AssetHash> PathToHash(const std::string& path) const;

    void ClearCache();

private:
    VirtualTable* m_Table = nullptr;
    mutable Spinlock m_Lock;
    std::unordered_map<AssetHash, std::vector<std::byte>> m_Cache;
    std::unordered_map<std::string, AssetHash> m_PathToHash;
    std::unordered_set<AssetHash> m_PendingLoads;

    std::vector<std::byte> LoadByHashInternal(AssetHash hash);
};

} // namespace Solstice::Core::Relic
