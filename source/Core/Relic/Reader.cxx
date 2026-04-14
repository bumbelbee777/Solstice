#include "Reader.hxx"
#include "Core/Debug/Debug.hxx"
#include <fstream>
#include <cstring>

namespace Solstice::Core::Relic {

std::optional<RelicContainer> OpenRelic(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        return std::nullopt;
    }

    RelicContainer container;
    container.FilePath = path;

    f.read(reinterpret_cast<char*>(&container.Header), sizeof(RelicFileHeader));
    if (!f || container.Header.Magic != RELIC_MAGIC ||
        container.Header.FormatVersion != RELIC_FORMAT_VERSION) {
        return std::nullopt;
    }

    const uint64_t manifestCount = container.Header.ManifestSize / sizeof(RelicManifestEntry);
    if (manifestCount > 1024 * 1024) {
        return std::nullopt;
    }
    container.Manifest.resize(static_cast<size_t>(manifestCount));
    f.seekg(static_cast<std::streamoff>(container.Header.ManifestOffset));
    f.read(reinterpret_cast<char*>(container.Manifest.data()),
           static_cast<std::streamsize>(container.Header.ManifestSize));
    if (!f) {
        return std::nullopt;
    }

    if (container.Header.DependencyTableSize > 0) {
        container.DependencyTableBlob.resize(static_cast<size_t>(container.Header.DependencyTableSize));
        f.seekg(static_cast<std::streamoff>(container.Header.DependencyTableOffset));
        f.read(reinterpret_cast<char*>(container.DependencyTableBlob.data()),
               static_cast<std::streamsize>(container.Header.DependencyTableSize));
        if (!f) {
            return std::nullopt;
        }
    }

    return container;
}

void GetDependencies(const RelicContainer& container, const RelicManifestEntry& entry,
    std::vector<AssetHash>& outDeps) {
    outDeps.clear();
    const uint32_t off = entry.DependencyListOffset;
    if (off + sizeof(uint32_t) > container.DependencyTableBlob.size()) {
        return;
    }
    uint32_t count = 0;
    std::memcpy(&count, &container.DependencyTableBlob[off], sizeof(uint32_t));
    if (count > 1024u) {
        return;
    }
    const size_t hashListSize = count * sizeof(AssetHash);
    if (off + sizeof(uint32_t) + hashListSize > container.DependencyTableBlob.size()) {
        return;
    }
    outDeps.resize(count);
    std::memcpy(outDeps.data(), &container.DependencyTableBlob[off + sizeof(uint32_t)], hashListSize);
}

} // namespace Solstice::Core::Relic
