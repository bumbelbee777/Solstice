#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace Solstice::Fuzz {

inline bool EnvFlag(const char* name) {
    const char* v = std::getenv(name);
    return v && v[0] != '\0' && std::strcmp(v, "0") != 0;
}

inline uint64_t SeedFromEnv() {
    const char* s = std::getenv("SOLSTICE_FUZZ_SEED");
    if (s && s[0] != '\0') {
        char* end = nullptr;
        const unsigned long long v = std::strtoull(s, &end, 10);
        if (end != s) {
            return static_cast<uint64_t>(v);
        }
    }
    return static_cast<uint64_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
}

inline int IterationsFromEnv(int defaultIterations, int tortureIterations) {
    if (EnvFlag("SOLSTICE_FUZZ_TORTURE")) {
        return tortureIterations;
    }
    const char* e = std::getenv("SOLSTICE_FUZZ_ITERATIONS");
    if (e && e[0] != '\0') {
        char* end = nullptr;
        const long v = std::strtol(e, &end, 10);
        if (end != e && v > 0) {
            return static_cast<int>(v);
        }
    }
    return defaultIterations;
}

class Rng {
public:
    explicit Rng(uint64_t seed) : m_Gen(seed) {}

    uint64_t NextU64() { return m_Dist(m_Gen); }

    size_t NextIndex(size_t n) {
        if (n == 0) {
            return 0;
        }
        return static_cast<size_t>(m_Dist(m_Gen) % n);
    }

    void MutateBytes(std::vector<uint8_t>& buf, size_t maxLen) {
        if (maxLen == 0) {
            buf.clear();
            return;
        }
        const uint32_t op = static_cast<uint32_t>(NextU64() % 5);
        if (buf.empty()) {
            buf.resize(std::min<size_t>(1 + NextIndex(maxLen), maxLen), static_cast<uint8_t>(NextU64()));
            return;
        }
        switch (op) {
            case 0: // flip bit
                buf[NextIndex(buf.size())] ^= static_cast<uint8_t>(1u << (NextU64() % 8));
                break;
            case 1: // add random byte
                if (buf.size() < maxLen) {
                    buf.insert(buf.begin() + static_cast<std::ptrdiff_t>(NextIndex(buf.size() + 1)),
                               static_cast<uint8_t>(NextU64()));
                }
                break;
            case 2: // remove byte
                if (buf.size() > 1) {
                    buf.erase(buf.begin() + static_cast<std::ptrdiff_t>(NextIndex(buf.size())));
                }
                break;
            case 3: // splice copy
                if (buf.size() < maxLen && buf.size() >= 2) {
                    const size_t i = NextIndex(buf.size());
                    buf.insert(buf.begin() + static_cast<std::ptrdiff_t>(i), buf[i]);
                }
                break;
            default: // random overwrite chunk
                for (size_t n = 1 + NextIndex(std::min(buf.size(), size_t{8})); n > 0; --n) {
                    buf[NextIndex(buf.size())] = static_cast<uint8_t>(NextU64());
                }
                break;
        }
        if (buf.size() > maxLen) {
            buf.resize(maxLen);
        }
    }

    std::string MutateUtf8(std::string s, size_t maxLen) {
        std::vector<uint8_t> buf(s.begin(), s.end());
        MutateBytes(buf, maxLen);
        return std::string(buf.begin(), buf.end());
    }

private:
    std::mt19937_64 m_Gen;
    std::uniform_int_distribution<uint64_t> m_Dist;
};

template<class Fn>
int RunFuzzIterations(const char* suite, int iterations, uint64_t seed, Fn&& fn) {
    Rng rng(seed);
    for (int i = 0; i < iterations; ++i) {
        try {
            fn(rng, i);
        } catch (const std::exception& ex) {
            std::cerr << "[" << suite << "] Fuzz iteration " << i << " exception: " << ex.what() << "\n"
                      << "  seed=" << seed << std::endl;
            return 1;
        } catch (...) {
            std::cerr << "[" << suite << "] Fuzz iteration " << i << " unknown exception. seed=" << seed << std::endl;
            return 1;
        }
    }
    return 0;
}

} // namespace Solstice::Fuzz
