#pragma once

/**
 * Solstice C API — shared types and export macro (version 1).
 * Naming: SolsticeV1_<ApiSet><Action> with PascalCase after SolsticeV1_
 */

#include <stdint.h>

#if defined(_WIN32) || defined(_WIN64)
    #ifdef SOLSTICE_EXPORTS
        #define SOLSTICE_V1_API __declspec(dllexport)
    #else
        #define SOLSTICE_V1_API __declspec(dllimport)
    #endif
#elif defined(__GNUC__) || defined(__clang__)
    #define SOLSTICE_V1_API __attribute__((visibility("default")))
#else
    #define SOLSTICE_V1_API
#endif

typedef uint8_t SolsticeV1_Bool;

#define SolsticeV1_False ((SolsticeV1_Bool)0)
#define SolsticeV1_True ((SolsticeV1_Bool)1)

typedef enum SolsticeV1_ResultCode {
    SolsticeV1_ResultSuccess = 0,
    SolsticeV1_ResultFailure = 1,
    SolsticeV1_ResultNotSupported = 2,
} SolsticeV1_ResultCode;
