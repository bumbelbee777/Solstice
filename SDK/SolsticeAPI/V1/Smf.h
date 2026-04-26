#pragma once

#include "Common.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Validate a Solstice Map Format (.smf) v1 binary blob.
 * Null Bytes or zero ByteCount returns failure. On failure, ErrBuffer receives a short message.
 */
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_SmfValidateBinary(
    const void* Bytes,
    size_t ByteCount,
    char* ErrBuffer,
    size_t ErrBufferSize);

/**
 * Load SMF bytes and apply acoustic zones + authoring lights to engine subsystems (same as Jackhammer on load).
 */
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_SmfApplyGameplay(
    const void* Bytes,
    size_t ByteCount,
    char* ErrBuffer,
    size_t ErrBufferSize);

#ifdef __cplusplus
}
#endif
