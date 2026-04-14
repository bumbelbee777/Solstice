#pragma once

#include "Common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Parse and validate solstice.narrative.v1 JSON. On failure, ErrBuffer contains messages (newline-separated).
 */
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NarrativeValidateJSON(
    const char* Json,
    char* ErrBuffer,
    size_t ErrBufferSize);

/** JSON narrative document -> YAML text (same semantic content). */
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NarrativeJSONToYAML(
    const char* Json,
    char* OutBuffer,
    size_t OutBufferSize,
    char* ErrBuffer,
    size_t ErrBufferSize);

#ifdef __cplusplus
}
#endif
