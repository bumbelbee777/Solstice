#pragma once

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

namespace Solstice::Test {

inline bool Verbose() {
    const char* v = std::getenv("SOLSTICE_TEST_VERBOSE");
    return v && v[0] != '\0' && std::strcmp(v, "0") != 0;
}

struct Counters {
    std::atomic<int> Passed{0};
    std::atomic<int> Failed{0};
};

inline Counters& GlobalCounters() {
    static Counters c;
    return c;
}

} // namespace Solstice::Test

#define SOLSTICE_TEST_LOG(msg) \
    do { \
        if (Solstice::Test::Verbose()) { \
            std::cout << (msg) << std::endl; \
        } \
    } while (0)

#define SOLSTICE_TEST_FAIL_MSG(message) \
    do { \
        std::cerr << "FAIL: " << (message) << " (" << __FILE__ << ":" << __LINE__ << ")" << std::endl; \
        Solstice::Test::GlobalCounters().Failed.fetch_add(1, std::memory_order_relaxed); \
    } while (0)

#define SOLSTICE_TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            SOLSTICE_TEST_FAIL_MSG(message); \
            return false; \
        } \
    } while (0)

#define SOLSTICE_TEST_ASSERT_VOID(condition, message) \
    do { \
        if (!(condition)) { \
            SOLSTICE_TEST_FAIL_MSG(message); \
            return; \
        } \
    } while (0)

#define SOLSTICE_TEST_PASS(message) \
    do { \
        SOLSTICE_TEST_LOG(std::string("PASS: ") + (message)); \
        Solstice::Test::GlobalCounters().Passed.fetch_add(1, std::memory_order_relaxed); \
    } while (0)

inline int SolsticeTestMainResult(const char* suiteName) {
    auto& c = Solstice::Test::GlobalCounters();
    const int failed = c.Failed.load(std::memory_order_relaxed);
    const int passed = c.Passed.load(std::memory_order_relaxed);
    if (failed > 0) {
        std::cerr << "[" << suiteName << "] FAILED: " << failed << " check(s), " << passed << " passed." << std::endl;
        return 1;
    }
    std::cout << "[" << suiteName << "] PASS (" << passed << " checks)" << std::endl;
    return 0;
}
