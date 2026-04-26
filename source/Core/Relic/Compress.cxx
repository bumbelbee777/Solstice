#include "Compress.hxx"
#include "Core/System/LZ4.hxx"
#include <zstd.h>
#include <cstring>

namespace Solstice::Core::Relic {

std::vector<std::byte> CompressAsset(std::span<const std::byte> uncompressed, CompressionType type) {
    switch (type) {
    case CompressionType::None: {
        std::vector<std::byte> out(uncompressed.size());
        if (!uncompressed.empty()) {
            std::memcpy(out.data(), uncompressed.data(), uncompressed.size());
        }
        return out;
    }
    case CompressionType::LZ4:
        return Core::LZ4Compress(uncompressed);
    case CompressionType::Zstd: {
        if (uncompressed.empty()) {
            return {};
        }
        const size_t bound = ZSTD_compressBound(uncompressed.size());
        if (ZSTD_isError(bound)) {
            return {};
        }
        std::vector<std::byte> out(bound);
        const size_t z = ZSTD_compress(out.data(), out.size(), uncompressed.data(), uncompressed.size(), 3);
        if (ZSTD_isError(z)) {
            return {};
        }
        out.resize(z);
        return out;
    }
    default:
        return {};
    }
}

} // namespace Solstice::Core::Relic
