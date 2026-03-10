#include "Delta.hxx"
#include "../Debug.hxx"
#include <cstring>

namespace Solstice::Core::Relic {

std::vector<std::byte> ApplyDelta(
    std::span<const std::byte> baseBytes,
    std::span<const std::byte> deltaPayload) {

    if (deltaPayload.size() < 5) return {};  // algorithm (1) + output_size (4)
    uint8_t algo = static_cast<uint8_t>(deltaPayload[0]);
    uint32_t outputSize = 0;
    std::memcpy(&outputSize, &deltaPayload[1], sizeof(outputSize));
    std::span<const std::byte> payload = deltaPayload.subspan(5);

    if (algo == static_cast<uint8_t>(DeltaAlgorithm::ChunkedXOR)) {
        if (outputSize != baseBytes.size() || payload.size() != outputSize) {
            return {};
        }
        std::vector<std::byte> out(outputSize);
        for (size_t i = 0; i < outputSize; ++i) {
            out[i] = static_cast<std::byte>(static_cast<uint8_t>(baseBytes[i]) ^ static_cast<uint8_t>(payload[i]));
        }
        return out;
    }
    return {};
}

} // namespace Solstice::Core::Relic
