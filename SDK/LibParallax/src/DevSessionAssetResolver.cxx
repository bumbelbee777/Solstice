#include <Parallax/DevSessionAssetResolver.hxx>

#include <fstream>
#include <span>

namespace Solstice::Parallax {

static uint64_t HashPathFNV1a(std::string_view path) {
    uint64_t h = 14695981039346656037ull;
    for (unsigned char c : path) {
        h ^= c;
        h *= 1099511628211ull;
    }
    return h;
}

static uint64_t HashBytesFNV1a(std::span<const std::byte> data) {
    uint64_t h = 14695981039346656037ull;
    for (std::byte b : data) {
        h ^= static_cast<uint8_t>(b);
        h *= 1099511628211ull;
    }
    return h;
}

bool DevSessionAssetResolver::ImportFile(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        return false;
    }
    f.seekg(0, std::ios::end);
    auto sz = f.tellg();
    f.seekg(0);
    std::vector<std::byte> buf(static_cast<size_t>(sz));
    f.read(reinterpret_cast<char*>(buf.data()), sz);
    uint64_t h = HashBytesFNV1a(std::span<const std::byte>(buf.data(), buf.size()));
    AssetData ad;
    ad.Bytes = std::move(buf);
    Register(h, std::move(ad), path.filename().string());
    return true;
}

void DevSessionAssetResolver::Register(uint64_t hash, AssetData data, std::string_view logicalPath) {
    m_ByHash[hash] = std::move(data);
    if (!logicalPath.empty()) {
        m_PathToHash[std::string(logicalPath)] = hash;
    }
}

bool DevSessionAssetResolver::Resolve(uint64_t assetHash, AssetData& outData) {
    auto it = m_ByHash.find(assetHash);
    if (it == m_ByHash.end()) {
        return false;
    }
    outData = it->second;
    return true;
}

uint64_t DevSessionAssetResolver::HashFromPath(std::string_view path) {
    auto it = m_PathToHash.find(std::string(path));
    if (it != m_PathToHash.end()) {
        return it->second;
    }
    return HashPathFNV1a(path);
}

bool DevSessionAssetResolver::IsLoaded(uint64_t assetHash) const {
    return m_ByHash.find(assetHash) != m_ByHash.end();
}

} // namespace Solstice::Parallax
