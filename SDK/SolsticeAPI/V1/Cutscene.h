#pragma once

#include "Common.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Parse cutscene JSON (same shape as CutscenePlayer::LoadFromJSONString).
 * On failure, ErrBuffer contains a short message when ErrBufferSize > 0.
 */
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_CutsceneValidateJSON(
    const char* Json,
    char* ErrBuffer,
    size_t ErrBufferSize);

#ifdef __cplusplus
}
#endif
