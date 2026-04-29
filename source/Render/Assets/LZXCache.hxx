#pragma once

#include "Core/System/LZX.hxx"
#include <vector>
#include <string>
#include <unordered_map>

namespace Solstice::Render {

enum class CacheLevel {
    L1_Decompressed,
    L2_Compressed,
    L3_Disk
};

template<typename T>
class LZXCache {
public:
    explicit LZXCache(size_t maxDecompressedBytes = 512ull * 1024ull * 1024ull,
                      size_t maxCompressedBytes = 256ull * 1024ull * 1024ull)
        : m_MaxDecompressedBytes(maxDecompressedBytes)
        , m_MaxCompressedBytes(maxCompressedBytes) {
    }

    T* GetAsset(const std::string&) { return nullptr; }
    void PutAsset(const std::string&, const T&, bool = false) {}

    void CompressInPlace(std::vector<std::byte>& data) { data = Core::LZXCompress(data); }
    void DecompressInPlace(std::vector<std::byte>& compressedData, std::vector<std::byte>& outDecompressed) {
        outDecompressed = Core::LZXDecompress(compressedData, outDecompressed.size());
    }

private:
    struct CacheEntry {
        std::vector<std::byte> Data;
        CacheLevel Level{CacheLevel::L2_Compressed};
        uint32_t LastAccessFrame{0};
    };

    std::unordered_map<std::string, CacheEntry> m_Cache;
    size_t m_MaxDecompressedBytes{0};
    size_t m_MaxCompressedBytes{0};
};

} // namespace Solstice::Render
