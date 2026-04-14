#include "SolsticeAPI/V1/Profiler.h"
#include <cstdio>
#include <cstdlib>
#include <cstdint>

int main() {
    if (SolsticeV1_ProfilerSetEnabled(SolsticeV1_True) != SolsticeV1_ResultSuccess) {
        std::fprintf(stderr, "[ProfilerTest] Failed to enable profiler.\n");
        return 1;
    }

    for (int i = 0; i < 16; ++i) {
        if (SolsticeV1_ProfilerBeginScope("ProfilerTest.Scope") != SolsticeV1_ResultSuccess) {
            std::fprintf(stderr, "[ProfilerTest] BeginScope failed.\n");
            return 2;
        }
        if (SolsticeV1_ProfilerSetCounter("ProfilerTest.Counter", i) != SolsticeV1_ResultSuccess) {
            std::fprintf(stderr, "[ProfilerTest] SetCounter failed.\n");
            return 3;
        }
        if (SolsticeV1_ProfilerEndScope("ProfilerTest.Scope") != SolsticeV1_ResultSuccess) {
            std::fprintf(stderr, "[ProfilerTest] EndScope failed.\n");
            return 4;
        }
    }

    int64_t counterValue = -1;
    if (SolsticeV1_ProfilerGetCounter("ProfilerTest.Counter", &counterValue) != SolsticeV1_ResultSuccess) {
        std::fprintf(stderr, "[ProfilerTest] GetCounter failed.\n");
        return 5;
    }
    if (counterValue != 15) {
        std::fprintf(stderr, "[ProfilerTest] Counter mismatch: %lld\n", static_cast<long long>(counterValue));
        return 6;
    }

    float frameMs = 0.0f;
    float fps = 0.0f;
    if (SolsticeV1_ProfilerGetLastFrame(&frameMs, &fps) != SolsticeV1_ResultSuccess) {
        std::fprintf(stderr, "[ProfilerTest] GetLastFrame failed.\n");
        return 7;
    }

    std::printf("[ProfilerTest] PASS\n");
    return EXIT_SUCCESS;
}
