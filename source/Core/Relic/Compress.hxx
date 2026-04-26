#pragma once

#include "Types.hxx"
#include "Solstice.hxx"
#include <cstddef>
#include <span>
#include <vector>

namespace Solstice::Core::Relic {

SOLSTICE_API std::vector<std::byte> CompressAsset(std::span<const std::byte> uncompressed, CompressionType type);

} // namespace Solstice::Core::Relic
