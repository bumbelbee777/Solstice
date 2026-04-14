#pragma once

#include "Common.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque decoder instance (single-threaded). Invalid when NULL. */
typedef void* SolsticeV1_VideoDecoder;

#define SolsticeV1_VideoDecoderInvalid ((SolsticeV1_VideoDecoder)0)

/**
 * Open the first video stream from a local file. When the engine was built without FFmpeg,
 * returns SolsticeV1_ResultNotSupported.
 */
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_VideoDecoderOpen(const char* Path, SolsticeV1_VideoDecoder* OutDecoder);

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_VideoDecoderClose(SolsticeV1_VideoDecoder Decoder);

/**
 * Decode the next frame into an internal RGBA8 buffer. Returns SolsticeV1_ResultFailure at end of stream.
 */
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_VideoDecoderAdvanceFrame(SolsticeV1_VideoDecoder Decoder);

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_VideoDecoderGetDimensions(
    SolsticeV1_VideoDecoder Decoder,
    uint32_t* OutWidth,
    uint32_t* OutHeight);

/** Packed RGBA8, row stride = width * 4. Valid until the next AdvanceFrame or Close. */
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_VideoDecoderGetRgba(
    SolsticeV1_VideoDecoder Decoder,
    const uint8_t** OutPixels,
    uint32_t* OutStrideBytes,
    size_t* OutBufferBytes);

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_VideoDecoderGetPtsSeconds(
    SolsticeV1_VideoDecoder Decoder,
    double* OutSeconds);

#ifdef __cplusplus
}
#endif
