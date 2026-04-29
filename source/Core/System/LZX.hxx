#pragma once

#include "Solstice.hxx"
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Solstice::Core {

// In-house LZX block compress/decompress for RELIC streaming assets.
// Format-compatible with Solstice's previous LZ4-style block path.
SOLSTICE_API std::vector<std::byte> LZXDecompress(std::span<const std::byte> compressed, size_t uncompressedSize);
SOLSTICE_API size_t LZXDecompressInto(std::span<const std::byte> compressed, std::span<std::byte> output);
SOLSTICE_API std::vector<std::byte> LZXCompress(std::span<const std::byte> source);

} // namespace Solstice::Core
