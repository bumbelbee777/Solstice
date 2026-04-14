#include "TestHarness.hxx"
#include "Solstice.hxx"

#include <cstdlib>
#include <cstring>

namespace {

bool EnvFlag(const char* name) {
    const char* v = std::getenv(name);
    return v && v[0] != '\0' && std::strcmp(v, "0") != 0;
}

int EnvInt(const char* name, int defaultValue) {
    const char* e = std::getenv(name);
    if (!e || e[0] == '\0') {
        return defaultValue;
    }
    char* end = nullptr;
    const long v = std::strtol(e, &end, 10);
    if (end == e || v < 1) {
        return defaultValue;
    }
    return static_cast<int>(v);
}

bool RunLifecycle() {
    const bool torture = EnvFlag("SOLSTICE_STRESS_TORTURE");
    const int cycles = EnvInt("SOLSTICE_LIFECYCLE_ROUNDS", torture ? 64 : 16);

    for (int i = 0; i < cycles; ++i) {
        SOLSTICE_TEST_ASSERT(Solstice::Initialize(), "Solstice::Initialize");
        SOLSTICE_TEST_ASSERT(Solstice::Reinitialize(), "Solstice::Reinitialize");
        Solstice::Shutdown();
    }

    SOLSTICE_TEST_PASS("Lifecycle rounds");
    return true;
}

} // namespace

int main() {
    if (!RunLifecycle()) {
        return 1;
    }
    return SolsticeTestMainResult("EngineLifecycleTest");
}
