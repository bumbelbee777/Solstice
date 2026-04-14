#pragma once

#include <atomic>

#define SOLSTICE_VERSION "1.0.0"
#define SOLSTICE_BUILD_NUM_MAJOR "13"
#define SOLSTICE_BUILD_NUM_MINOR "0"
#define SOLSTICE_BUILD_NUM_PATCH "0"
#ifndef SOLSTICE_BUILD_GIT_COMMIT
    #define SOLSTICE_BUILD_GIT_COMMIT "UNKNOWN" // To be set via CMake compiler options
#endif
enum {
SOLSTICE_DEVELOPMENT_BUILD = 1
};

#if defined(_WIN32) || defined(_WIN64)
    #ifdef SOLSTICE_EXPORTS
        #define SOLSTICE_API __declspec(dllexport)
    #elif defined(SOLSTICE_STATIC_LINK)
        /* Static libs (e.g. Math.lib) + TUs that only consume headers: avoid dllimport __imp_ refs. */
        #define SOLSTICE_API
    #else
        #define SOLSTICE_API __declspec(dllimport)
    #endif
#elif defined(__GNUC__) || defined(__clang__)
    #ifdef SOLSTICE_EXPORTS
        #define SOLSTICE_API __attribute__((visibility("default")))
    #else
        #define SOLSTICE_API
    #endif
#else
    #define SOLSTICE_API
#endif

namespace Solstice {
extern std::atomic<bool> Initialized;

SOLSTICE_API bool Initialize();
SOLSTICE_API bool Reinitialize();
SOLSTICE_API void Shutdown();
}
