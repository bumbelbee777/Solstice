#pragma once

#include "../Solstice.hxx"
#include <cstddef>
#include <cstdint>
#include <vector>
#include <span>

namespace Solstice::Core {

// In-house LZ4 block compress/decompress (no frame). Compatible with LZ4 block format.
// Used by RELIC for streaming assets.

// Decompress LZ4 block. Returns empty vector on error.
SOLSTICE_API std::vector<std::byte> LZ4Decompress(std::span<const std::byte> compressed, size_t uncompressedSize);

// Decompress into existing buffer. Returns number of bytes written, or 0 on error.
SOLSTICE_API size_t LZ4DecompressInto(std::span<const std::byte> compressed, std::span<std::byte> output);

// Compress to LZ4 block. Returns compressed bytes (empty on error).
SOLSTICE_API std::vector<std::byte> LZ4Compress(std::span<const std::byte> source);

} // namespace Solstice::Core
