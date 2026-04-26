#pragma once

#include "Solstice.hxx"
#include "Types.hxx"
#include <cstddef>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace Solstice::Core::Relic {

// Binary blob: uint32_t entryCount, then per entry: uint16_t pathLen, UTF-8 path bytes, uint64_t assetHash.
SOLSTICE_API bool ParsePathTableBlob(std::span<const std::byte> blob,
    std::vector<std::pair<std::string, AssetHash>>& outEntries);

SOLSTICE_API bool BuildPathTableBlob(const std::vector<std::pair<std::string, AssetHash>>& entries,
    std::vector<std::byte>& outBlob);

} // namespace Solstice::Core::Relic
