#pragma once

#define SOLSTICE_VERSION "0.1.0"
#define SOLSTICE_BUILD_NUM "7"
#define SOLSTICE_DEVELOPMENT_BUILD 1

#if defined(_WIN32) || defined(_WIN64)
    #ifdef SOLSTICE_EXPORTS
        #define SOLSTICE_API __declspec(dllexport)
    #else
        #define SOLSTICE_API __declspec(dllimport)
    #endif
#else
    #define SOLSTICE_API
#endif