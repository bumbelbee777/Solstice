#pragma once

#include "Common.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*SolsticeV1_ScriptingPrintHook)(const char* LineUtf8, void* UserData);

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

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_ScriptingExecuteExport(
    const char* Source,
    const char* ExportName,
    char* OutputBuffer,
    size_t OutputBufferSize,
    char* ErrorBuffer,
    size_t ErrorBufferSize);

SOLSTICE_V1_API void SolsticeV1_ScriptingSetPrintHook(
    SolsticeV1_ScriptingPrintHook Hook,
    void* UserData);

#ifdef __cplusplus
}
#endif
