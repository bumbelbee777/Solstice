#include "Writer.hxx"
#include "Compress.hxx"
#include "PathTable.hxx"
#include <algorithm>
#include <cstring>
#include <fstream>

namespace Solstice::Core::Relic {

namespace {

constexpr uint64_t kMaxManifestEntries = 1024ull * 1024ull;

} // namespace

bool WriteRelic(const std::filesystem::path& path, std::vector<RelicWriteInput> inputs,
    const RelicWriteOptions& options) {
    if (inputs.empty() || inputs.size() > kMaxManifestEntries) {
        return false;
    }

    for (const auto& in : inputs) {
        if (in.Hash == 0) {
            return false;
        }
        if ((in.ExtraFlags & FlagIsDelta) != 0) {
            return false;
        }
    }

    std::sort(inputs.begin(), inputs.end(), [](const RelicWriteInput& a, const RelicWriteInput& b) {
        if (a.ClusterId != b.ClusterId) {
            return a.ClusterId < b.ClusterId;
        }
        return a.Hash < b.Hash;
    });

    for (size_t i = 1; i < inputs.size(); ++i) {
        if (inputs[i - 1].Hash == inputs[i].Hash) {
            return false;
        }
    }

    std::vector<std::byte> depBlob;
    std::vector<RelicManifestEntry> manifest;
    manifest.reserve(inputs.size());

    for (auto& in : inputs) {
        auto& deps = in.Dependencies;
        std::sort(deps.begin(), deps.end());
        deps.erase(std::unique(deps.begin(), deps.end()), deps.end());
    }

    for (auto& in : inputs) {
        RelicManifestEntry e{};
        e.AssetHash = in.Hash;
        e.AssetTypeTag = static_cast<uint16_t>(in.TypeTag);
        e.ClusterId = in.ClusterId;
        e.Flags = static_cast<uint16_t>(WithCompression(in.Compression, in.ExtraFlags));
        e.UncompressedSize = static_cast<uint32_t>(in.Uncompressed.size());

        if (in.Dependencies.empty()) {
            e.DependencyListOffset = 0;
        } else {
            e.DependencyListOffset = static_cast<uint32_t>(depBlob.size());
            const uint32_t c = static_cast<uint32_t>(in.Dependencies.size());
            const size_t off = depBlob.size();
            depBlob.resize(off + sizeof(uint32_t) + c * sizeof(AssetHash));
            std::memcpy(depBlob.data() + off, &c, sizeof(uint32_t));
            std::memcpy(depBlob.data() + off + sizeof(uint32_t), in.Dependencies.data(), c * sizeof(AssetHash));
        }

        std::vector<std::byte> payload = CompressAsset(in.Uncompressed, in.Compression);
        if (in.Uncompressed.empty() && in.Compression != CompressionType::None) {
            return false;
        }
        if (in.Uncompressed.empty()) {
            payload.clear();
        } else if (payload.empty() && in.Compression != CompressionType::None) {
            return false;
        }
        e.CompressedSize = static_cast<uint32_t>(payload.size());

        manifest.push_back(e);
        in.Uncompressed = std::move(payload);
    }

    std::vector<std::byte> dataBlob;
    for (size_t i = 0; i < inputs.size(); ++i) {
        RelicManifestEntry& e = manifest[i];
        e.DataOffset = static_cast<uint64_t>(dataBlob.size());
        const auto& payload = inputs[i].Uncompressed;
        dataBlob.insert(dataBlob.end(), payload.begin(), payload.end());
    }

    std::vector<std::pair<std::string, AssetHash>> pathRows;
    for (const auto& in : inputs) {
        if (!in.LogicalPath.empty()) {
            pathRows.push_back({in.LogicalPath, in.Hash});
        }
    }
    std::vector<std::byte> pathBlob;
    if (!pathRows.empty()) {
        if (!BuildPathTableBlob(pathRows, pathBlob)) {
            return false;
        }
    }

    const uint64_t headerSize = sizeof(RelicFileHeader);
    const uint64_t manifestOff = headerSize;
    const uint64_t manifestSize = manifest.size() * sizeof(RelicManifestEntry);
    const uint64_t depOff = manifestOff + manifestSize;
    const uint64_t depSize = depBlob.size();
    const uint64_t pathOff = depOff + depSize;
    const uint64_t pathSize = pathBlob.size();
    const uint64_t dataOff = pathOff + pathSize;

    RelicFileHeader hdr{};
    hdr.Magic = RELIC_MAGIC;
    hdr.FormatVersion = RELIC_FORMAT_VERSION;
    hdr.ContainerType = static_cast<uint8_t>(options.Container);
    hdr.Reserved = 0;
    hdr.TagSet = options.TagSet;
    hdr.Reserved2 = 0;
    hdr.ManifestOffset = manifestOff;
    hdr.ManifestSize = manifestSize;
    hdr.DependencyTableOffset = depOff;
    hdr.DependencyTableSize = depSize;
    hdr.DataBlobOffset = dataOff;
    hdr.PathTableOffset = pathSize > 0 ? pathOff : 0;
    hdr.PathTableSize = pathSize;

    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) {
        return false;
    }
    f.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    f.write(reinterpret_cast<const char*>(manifest.data()),
        static_cast<std::streamsize>(manifest.size() * sizeof(RelicManifestEntry)));
    if (!depBlob.empty()) {
        f.write(reinterpret_cast<const char*>(depBlob.data()),
            static_cast<std::streamsize>(depBlob.size()));
    }
    if (!pathBlob.empty()) {
        f.write(reinterpret_cast<const char*>(pathBlob.data()),
            static_cast<std::streamsize>(pathBlob.size()));
    }
    if (!dataBlob.empty()) {
        f.write(reinterpret_cast<const char*>(dataBlob.data()),
            static_cast<std::streamsize>(dataBlob.size()));
    }
    return static_cast<bool>(f);
}

} // namespace Solstice::Core::Relic
