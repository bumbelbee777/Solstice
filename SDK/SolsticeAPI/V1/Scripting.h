#pragma once

#include "Common.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_ScriptingCompile(
    const char* Source,
    char* ErrorBuffer,
    size_t ErrorBufferSize);

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_ScriptingExecute(
    const char* Source,
    char* OutputBuffer,
    size_t OutputBufferSize,
    char* ErrorBuffer,
    size_t ErrorBufferSize);

#ifdef __cplusplus
}
#endif
