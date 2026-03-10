#pragma once

#include "../../Solstice.hxx"
#include "Types.hxx"
#include "Reader.hxx"
#include "../Mmap.hxx"
#include <optional>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <memory>

namespace Solstice::Core::Relic {

// Resolved entry for one asset in the virtual table (after merging all RELICs).
struct ResolvedEntry {
    AssetHash Hash = 0;
    uint64_t DataOffset = 0;
    uint32_t CompressedSize = 0;
    uint32_t UncompressedSize = 0;
    uint16_t AssetTypeTag = 0;
    uint16_t Flags = 0;
    uint32_t ClusterId = 0;
    std::vector<AssetHash> Dependencies;
    // Which container and mmap (index into VirtualTable's containers)
    size_t ContainerIndex = 0;
};

// Merges bootstrap + all RELIC containers, builds virtual asset table (later override wins).
// Holds opened containers and mmap'd data blobs for read access.
class SOLSTICE_API VirtualTable {
public:
    VirtualTable() = default;
    VirtualTable(const VirtualTable&) = delete;
    VirtualTable& operator=(const VirtualTable&) = delete;

    // Load bootstrap from basePath/game.data.relic and open all listed RELICs. Merge manifests (by priority).
    bool Initialize(const std::filesystem::path& basePath);

    // Optional: initialize with explicit list of RELIC paths (e.g. for tests). Priority = order (last wins).
    bool InitializeWithPaths(const std::filesystem::path& basePath, const std::vector<std::filesystem::path>& relicPaths);

    void Shutdown();

    std::optional<ResolvedEntry> Find(AssetHash hash) const;

    // Get all asset hashes in a cluster (for prefetch).
    void GetAssetHashesInCluster(uint32_t clusterId, std::vector<AssetHash>& out) const;

    // Read raw bytes for an entry from the data blob (no decompression). Returns empty if not found or read error.
    std::vector<std::byte> ReadAssetBytes(const ResolvedEntry& entry) const;

    bool IsInitialized() const { return m_Initialized; }

private:
    struct ContainerState {
        RelicContainer Container;
        MmapFile BlobMap;
    };
    std::vector<ContainerState> m_Containers;
    std::unordered_map<AssetHash, ResolvedEntry> m_Table;
    std::unordered_map<uint32_t, std::vector<AssetHash>> m_ClusterToHashes;
    std::filesystem::path m_BasePath;
    bool m_Initialized = false;

    bool LoadContainers(const std::vector<std::filesystem::path>& paths);
};

} // namespace Solstice::Core::Relic
