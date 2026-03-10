#pragma once

#include "../../Solstice.hxx"
#include "Types.hxx"
#include <vector>
#include <span>
#include <cstddef>

namespace Solstice::Core::Relic {

// Decompress RELIC asset bytes according to compression type (None, LZ4, Zstd).
// Returns decompressed bytes or empty on error.
SOLSTICE_API std::vector<std::byte> DecompressAsset(
    std::span<const std::byte> compressed,
    CompressionType compressionType,
    size_t uncompressedSize);

} // namespace Solstice::Core::Relic
