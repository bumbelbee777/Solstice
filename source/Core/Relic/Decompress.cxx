#include "Decompress.hxx"
#include "Core/System/LZ4.hxx"
#include "Core/Debug/Debug.hxx"
#include <zstd.h>
#include <cstring>

namespace Solstice::Core::Relic {

std::vector<std::byte> DecompressAsset(
    std::span<const std::byte> compressed,
    CompressionType compressionType,
    size_t uncompressedSize) {

    switch (compressionType) {
    case CompressionType::None: {
        std::vector<std::byte> out(compressed.size());
        std::memcpy(out.data(), compressed.data(), compressed.size());
        return out;
    }
    case CompressionType::LZ4: {
        return Core::LZ4Decompress(compressed, uncompressedSize);
    }
    case CompressionType::Zstd: {
        std::vector<std::byte> out(uncompressedSize);
        size_t result = ZSTD_decompress(out.data(), uncompressedSize,
                                        compressed.data(), compressed.size());
        if (ZSTD_isError(result) || result != uncompressedSize) {
            return {};
        }
        return out;
    }
    default:
        return {};
    }
}

} // namespace Solstice::Core::Relic
