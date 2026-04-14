#pragma once

#include "Solstice.hxx"
#include "Types.hxx"
#include <vector>
#include <span>
#include <cstddef>

namespace Solstice::Core::Relic {

// Delta algorithm ID (stored in delta payload header)
enum class DeltaAlgorithm : uint8_t {
    ChunkedXOR = 0,  // output = base ^ delta (byte-wise), same length
    Count
};

// Apply delta to base bytes. Returns applied result or empty on error.
// Delta payload: algorithm (1), output_size (4), then algorithm-specific payload.
SOLSTICE_API std::vector<std::byte> ApplyDelta(
    std::span<const std::byte> baseBytes,
    std::span<const std::byte> deltaPayload);

} // namespace Solstice::Core::Relic
