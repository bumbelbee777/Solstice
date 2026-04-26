#include "PathTable.hxx"
#include <cstring>

namespace Solstice::Core::Relic {

namespace {

constexpr uint32_t kMaxPathTableEntries = 1024 * 1024;
constexpr uint32_t kMaxPathLen = 4096;

} // namespace

bool ParsePathTableBlob(std::span<const std::byte> blob,
    std::vector<std::pair<std::string, AssetHash>>& outEntries) {
    outEntries.clear();
    if (blob.size() < sizeof(uint32_t)) {
        return false;
    }
    uint32_t count = 0;
    std::memcpy(&count, blob.data(), sizeof(uint32_t));
    if (count > kMaxPathTableEntries) {
        return false;
    }
    const std::byte* p = blob.data() + sizeof(uint32_t);
    const std::byte* end = blob.data() + blob.size();
    outEntries.reserve(static_cast<size_t>(count));
    for (uint32_t i = 0; i < count; ++i) {
        if (p + sizeof(uint16_t) > end) {
            return false;
        }
        uint16_t pathLen = 0;
        std::memcpy(&pathLen, p, sizeof(uint16_t));
        p += sizeof(uint16_t);
        if (pathLen > kMaxPathLen || p + pathLen + sizeof(AssetHash) > end) {
            return false;
        }
        std::string path(reinterpret_cast<const char*>(p), pathLen);
        p += pathLen;
        AssetHash h = 0;
        std::memcpy(&h, p, sizeof(AssetHash));
        p += sizeof(AssetHash);
        outEntries.push_back({std::move(path), h});
    }
    return p == end;
}

bool BuildPathTableBlob(const std::vector<std::pair<std::string, AssetHash>>& entries,
    std::vector<std::byte>& outBlob) {
    outBlob.clear();
    if (entries.size() > kMaxPathTableEntries) {
        return false;
    }
    const uint32_t count = static_cast<uint32_t>(entries.size());
    outBlob.resize(sizeof(uint32_t));
    std::memcpy(outBlob.data(), &count, sizeof(uint32_t));
    for (const auto& [path, hash] : entries) {
        if (path.size() > kMaxPathLen) {
            outBlob.clear();
            return false;
        }
        const uint16_t plen = static_cast<uint16_t>(path.size());
        const size_t off = outBlob.size();
        outBlob.resize(off + sizeof(uint16_t) + plen + sizeof(AssetHash));
        std::memcpy(outBlob.data() + off, &plen, sizeof(uint16_t));
        if (plen > 0) {
            std::memcpy(outBlob.data() + off + sizeof(uint16_t), path.data(), plen);
        }
        std::memcpy(outBlob.data() + off + sizeof(uint16_t) + plen, &hash, sizeof(AssetHash));
    }
    return true;
}

} // namespace Solstice::Core::Relic
