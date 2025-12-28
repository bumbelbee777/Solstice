#include "Seed.hxx"
#include <cstdint>
#include <cmath>

namespace Solstice::Arzachel {

// Fast hash function (xxHash-inspired)
static uint64_t Hash64(uint64_t X) {
    X ^= X >> 33;
    X *= 0xff51afd7ed558ccdULL;
    X ^= X >> 33;
    X *= 0xc4ceb9fe1a85ec53ULL;
    X ^= X >> 33;
    return X;
}

// Generate next uint32 from seed
uint32_t NextUint32(const Seed& S) {
    uint64_t H = Hash64(S.Value);
    return static_cast<uint32_t>(H & 0xFFFFFFFFULL);
}

// Generate next float in [0, 1) from seed
float NextFloat(const Seed& S) {
    uint32_t U = NextUint32(S);
    // Convert to float in [0, 1)
    // Use 23 bits for mantissa precision
    return static_cast<float>(U & 0x7FFFFF) / 8388608.0f; // 2^23
}

// Generate next seed (for chaining RNG calls)
Seed NextSeed(const Seed& S) {
    return Seed(Hash64(S.Value));
}

} // namespace Solstice::Arzachel
