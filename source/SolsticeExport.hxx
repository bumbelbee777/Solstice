#pragma once

// DLL Export/Import macros for Windows
#if defined(_WIN32) || defined(_WIN64)
    #ifdef SOLSTICE_EXPORTS
        // Building the DLL - export symbols
        #define SOLSTICE_API __declspec(dllexport)
    #else
        // Using the DLL - import symbols
        #define SOLSTICE_API __declspec(dllimport)
    #endif
#else
    // Non-Windows platforms - no special handling needed
    #define SOLSTICE_API
#endif
