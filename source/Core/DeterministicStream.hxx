#pragma once

#include <cstdint>

namespace Solstice::Core {

/// Lightweight deterministic RNG streams for procedural gameplay (PCG, maze layouts, props).
/// Use distinct \p streamTag values per subsystem so ordering stays reproducible given one master seed.
class DeterministicStream final {
public:
    /// Combines master seed with a caller-defined stream id (e.g. 'MAZE', 'PROP', ASCII fourcc).
    explicit DeterministicStream(std::uint64_t masterSeed,
                                 std::uint32_t streamTag = 0) noexcept
        : m_State(SplitMix64(SplitMix64(masterSeed ^ (static_cast<std::uint64_t>(streamTag) << 32ULL)))) {}

    [[nodiscard]] std::uint32_t NextU32() noexcept {
        return static_cast<std::uint32_t>(SplitMix64Next(m_State));
    }

    [[nodiscard]] std::uint64_t NextU64() noexcept {
        return SplitMix64Next(m_State);
    }

    /// Uniform in [0,1)
    [[nodiscard]] float UnitFloat() noexcept {
        const uint32_t u = NextU32();
        return static_cast<float>(static_cast<double>(u) * (1.0 / 4294967296.0));
    }

    /// Inclusive-exclusive range [lo, hi)
    [[nodiscard]] int RangeInt(int lo, int hi) noexcept {
        if (hi <= lo) return lo;
        const std::uint64_t span = static_cast<std::uint64_t>(hi - lo);
        return lo + static_cast<int>(NextU64() % span);
    }

    [[nodiscard]] std::uint64_t State() const noexcept { return m_State; }

private:
    static std::uint64_t SplitMix64(std::uint64_t z) noexcept {
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }

    static std::uint64_t SplitMix64Next(std::uint64_t& x) noexcept {
        std::uint64_t z = (x += 0x9e3779b97f4a7c15ULL);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }

    std::uint64_t m_State;
};

/// Common stream tags (compose with your own fourcc as needed).
inline constexpr std::uint32_t StreamTagFourCC(char a, char b, char c, char d) noexcept {
    return static_cast<std::uint32_t>(static_cast<unsigned char>(a))
           | (static_cast<std::uint32_t>(static_cast<unsigned char>(b)) << 8)
           | (static_cast<std::uint32_t>(static_cast<unsigned char>(c)) << 16)
           | (static_cast<std::uint32_t>(static_cast<unsigned char>(d)) << 24);
}

} // namespace Solstice::Core
