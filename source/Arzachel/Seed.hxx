#pragma once

#include "../Solstice.hxx"
#include <cstdint>
#include <functional>

namespace Solstice::Arzachel {

// Immutable seed value for deterministic RNG
// Uses hash-based state for pure functional RNG
struct Seed {
    uint64_t Value;

    Seed() : Value(0) {}
    explicit Seed(uint64_t Val) : Value(Val) {}
    Seed(const Seed& Other) : Value(Other.Value) {}

    // Derive a new seed from this one (hierarchical derivation)
    // Ensures different derived seeds are uncorrelated
    Seed Derive(uint64_t DeriveValue) const {
        // Hash combine: mix this seed with the derive value
        uint64_t H = Value;
        H ^= DeriveValue + 0x9e3779b97f4a7c15ULL + (H << 6) + (H >> 2);
        return Seed(H);
    }

    bool operator==(const Seed& Other) const { return Value == Other.Value; }
    bool operator!=(const Seed& Other) const { return Value != Other.Value; }
};

// Hash function for Seed (for use in std::unordered_map, etc.)
inline std::size_t HashSeed(const Seed& S) {
    return std::hash<uint64_t>{}(S.Value);
}

// Pure RNG functions - all deterministic, no mutable state
// Generate next uint32 from seed (does not modify seed)
SOLSTICE_API uint32_t NextUint32(const Seed& S);

// Generate next float in [0, 1) from seed
SOLSTICE_API float NextFloat(const Seed& S);

// Generate next seed (for chaining RNG calls)
SOLSTICE_API Seed NextSeed(const Seed& S);

} // namespace Solstice::Arzachel

// Hash specialization for std::unordered_map
namespace std {
    template<>
    struct hash<Solstice::Arzachel::Seed> {
        std::size_t operator()(const Solstice::Arzachel::Seed& S) const {
            return Solstice::Arzachel::HashSeed(S);
        }
    };
}
