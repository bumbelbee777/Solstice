#include "VirtualTable.hxx"
#include "../Debug.hxx"
#include "Bootstrap.hxx"
#include "Reader.hxx"
#include <algorithm>

namespace Solstice::Core::Relic {

bool VirtualTable::Initialize(const std::filesystem::path& basePath) {
    Shutdown();
    m_BasePath = basePath;
    auto bootstrapPath = GetDefaultBootstrapPath(basePath);
    auto config = ParseBootstrap(bootstrapPath);
    if (!config || !config->Valid || config->Entries.empty()) {
        return false;
    }
    std::vector<std::filesystem::path> paths;
    paths.reserve(config->Entries.size());
    for (const auto& e : config->Entries) {
        paths.push_back(basePath / e.Path);
    }
    if (!LoadContainers(paths)) {
        Shutdown();
        return false;
    }
    m_Initialized = true;
    return true;
}

bool VirtualTable::InitializeWithPaths(const std::filesystem::path& basePath,
                                        const std::vector<std::filesystem::path>& relicPaths) {
    Shutdown();
    m_BasePath = basePath;
    if (!LoadContainers(relicPaths)) {
        Shutdown();
        return false;
    }
    m_Initialized = true;
    return true;
}

void VirtualTable::Shutdown() {
    m_Containers.clear();
    m_Table.clear();
    m_ClusterToHashes.clear();
    m_Initialized = false;
}

bool VirtualTable::LoadContainers(const std::vector<std::filesystem::path>& paths) {
    for (const auto& path : paths) {
        auto container = OpenRelic(path);
        if (!container) {
            continue;
        }
        ContainerState state;
        state.Container = std::move(*container);
        const uint64_t blobOffset = state.Container.Header.DataBlobOffset;
        const uint64_t blobSize = state.Container.Header.ManifestSize;  // wrong - we need blob size from file size - header
        // Blob runs from DataBlobOffset to end of file (or we need to store blob size in header). Plan says "data blob offset" - so blob starts there. Size = file size - DataBlobOffset.
        std::error_code ec;
        auto fileSize = std::filesystem::file_size(path, ec);
        if (ec) {
            continue;
        }
        uint64_t blobSizeActual = (fileSize >= blobOffset) ? (fileSize - blobOffset) : 0;
        if (!state.BlobMap.Open(path, blobOffset, blobSizeActual)) {
            continue;
        }
        size_t containerIndex = m_Containers.size();
        m_Containers.push_back(std::move(state));
        for (const auto& entry : m_Containers.back().Container.Manifest) {
            ResolvedEntry resolved;
            resolved.Hash = entry.AssetHash;
            resolved.DataOffset = entry.DataOffset;
            resolved.CompressedSize = entry.CompressedSize;
            resolved.UncompressedSize = entry.UncompressedSize;
            resolved.AssetTypeTag = entry.AssetTypeTag;
            resolved.Flags = entry.Flags;
            resolved.ClusterId = entry.ClusterId;
            resolved.ContainerIndex = containerIndex;
            GetDependencies(m_Containers.back().Container, entry, resolved.Dependencies);
            m_Table[entry.AssetHash] = std::move(resolved);
            m_ClusterToHashes[entry.ClusterId].push_back(entry.AssetHash);
        }
    }
    return !m_Containers.empty();
}

std::optional<ResolvedEntry> VirtualTable::Find(AssetHash hash) const {
    auto it = m_Table.find(hash);
    if (it == m_Table.end()) {
        return std::nullopt;
    }
    return it->second;
}

void VirtualTable::GetAssetHashesInCluster(uint32_t clusterId, std::vector<AssetHash>& out) const {
    out.clear();
    auto it = m_ClusterToHashes.find(clusterId);
    if (it != m_ClusterToHashes.end()) {
        out = it->second;
    }
}

std::vector<std::byte> VirtualTable::ReadAssetBytes(const ResolvedEntry& entry) const {
    if (entry.ContainerIndex >= m_Containers.size()) {
        return {};
    }
    const auto& state = m_Containers[entry.ContainerIndex];
    auto span = state.BlobMap.Read(entry.DataOffset, entry.CompressedSize);
    if (span.empty()) {
        return {};
    }
    return std::vector<std::byte>(span.begin(), span.end());
}

} // namespace Solstice::Core::Relic
