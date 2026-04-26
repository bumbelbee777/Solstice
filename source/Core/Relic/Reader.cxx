#include "Reader.hxx"
#include "Core/Debug/Debug.hxx"
#include <fstream>
#include <cstring>

namespace Solstice::Core::Relic {

namespace {

constexpr size_t kRelicHeaderLegacyBytes = 52;

bool RangeOk(uint64_t fileSize, uint64_t off, uint64_t size) {
    if (size == 0) {
        return true;
    }
    return off <= fileSize && off + size <= fileSize;
}

} // namespace

std::optional<RelicContainer> OpenRelic(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        return std::nullopt;
    }

    f.seekg(0, std::ios::end);
    const auto endPos = f.tellg();
    if (endPos < 0) {
        return std::nullopt;
    }
    const uint64_t fileSize = static_cast<uint64_t>(endPos);
    if (fileSize < kRelicHeaderLegacyBytes) {
        return std::nullopt;
    }

    RelicContainer container;
    container.FilePath = path;

    f.seekg(0);
    f.read(reinterpret_cast<char*>(&container.Header), static_cast<std::streamsize>(kRelicHeaderLegacyBytes));
    if (!f || container.Header.Magic != RELIC_MAGIC || container.Header.FormatVersion != RELIC_FORMAT_VERSION) {
        return std::nullopt;
    }

    // Legacy layout: 52-byte header only; manifest begins at offset 52. Do not interpret the next 16 bytes
    // as path-table fields (they are the start of the manifest).
    if (container.Header.ManifestOffset == kRelicHeaderLegacyBytes) {
        container.Header.PathTableOffset = 0;
        container.Header.PathTableSize = 0;
    } else if (fileSize >= sizeof(RelicFileHeader)) {
        f.read(reinterpret_cast<char*>(&container.Header.PathTableOffset),
            static_cast<std::streamsize>(sizeof(uint64_t) * 2));
        if (!f) {
            return std::nullopt;
        }
    } else {
        container.Header.PathTableOffset = 0;
        container.Header.PathTableSize = 0;
    }

    const uint64_t manifestCount = container.Header.ManifestSize / sizeof(RelicManifestEntry);
    if (manifestCount > 1024 * 1024) {
        return std::nullopt;
    }
    if (!RangeOk(fileSize, container.Header.ManifestOffset, container.Header.ManifestSize) ||
        !RangeOk(fileSize, container.Header.DependencyTableOffset, container.Header.DependencyTableSize)) {
        return std::nullopt;
    }
    if (container.Header.DataBlobOffset > fileSize) {
        return std::nullopt;
    }
    if (container.Header.PathTableSize > 0) {
        if (container.Header.PathTableOffset == 0 ||
            !RangeOk(fileSize, container.Header.PathTableOffset, container.Header.PathTableSize)) {
            return std::nullopt;
        }
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

    if (container.Header.PathTableSize > 0) {
        container.PathTableBlob.resize(static_cast<size_t>(container.Header.PathTableSize));
        f.seekg(static_cast<std::streamoff>(container.Header.PathTableOffset));
        f.read(reinterpret_cast<char*>(container.PathTableBlob.data()),
            static_cast<std::streamsize>(container.Header.PathTableSize));
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
