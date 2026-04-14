#include "SolsticeAPI/V1/Video.h"

#include <new>
#include <string>

#if defined(SOLSTICE_HAVE_FFMPEG) && SOLSTICE_HAVE_FFMPEG
#include <Multimedia/FfmpegVideoDecoder.hxx>
#endif

extern "C" {

#if defined(SOLSTICE_HAVE_FFMPEG) && SOLSTICE_HAVE_FFMPEG

namespace {

Solstice::Multimedia::FfmpegVideoDecoder* AsDecoder(SolsticeV1_VideoDecoder d) {
    return reinterpret_cast<Solstice::Multimedia::FfmpegVideoDecoder*>(d);
}

} // namespace

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_VideoDecoderOpen(const char* Path, SolsticeV1_VideoDecoder* OutDecoder) {
    if (!Path || !OutDecoder) {
        return SolsticeV1_ResultFailure;
    }
    *OutDecoder = SolsticeV1_VideoDecoderInvalid;
    auto* dec = new (std::nothrow) Solstice::Multimedia::FfmpegVideoDecoder();
    if (!dec) {
        return SolsticeV1_ResultFailure;
    }
    std::string err;
    if (!dec->Open(Path, err)) {
        delete dec;
        return SolsticeV1_ResultFailure;
    }
    *OutDecoder = reinterpret_cast<SolsticeV1_VideoDecoder>(dec);
    return SolsticeV1_ResultSuccess;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_VideoDecoderClose(SolsticeV1_VideoDecoder Decoder) {
    if (!Decoder) {
        return SolsticeV1_ResultSuccess;
    }
    delete AsDecoder(Decoder);
    return SolsticeV1_ResultSuccess;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_VideoDecoderAdvanceFrame(SolsticeV1_VideoDecoder Decoder) {
    if (!Decoder) {
        return SolsticeV1_ResultFailure;
    }
    std::string err;
    if (!AsDecoder(Decoder)->DecodeNextFrame(err)) {
        return SolsticeV1_ResultFailure;
    }
    return SolsticeV1_ResultSuccess;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_VideoDecoderGetDimensions(
    SolsticeV1_VideoDecoder Decoder,
    uint32_t* OutWidth,
    uint32_t* OutHeight) {
    if (!Decoder || !OutWidth || !OutHeight) {
        return SolsticeV1_ResultFailure;
    }
    *OutWidth = AsDecoder(Decoder)->Width();
    *OutHeight = AsDecoder(Decoder)->Height();
    return SolsticeV1_ResultSuccess;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_VideoDecoderGetRgba(
    SolsticeV1_VideoDecoder Decoder,
    const uint8_t** OutPixels,
    uint32_t* OutStrideBytes,
    size_t* OutBufferBytes) {
    if (!Decoder || !OutPixels || !OutStrideBytes || !OutBufferBytes) {
        return SolsticeV1_ResultFailure;
    }
    const auto* dec = AsDecoder(Decoder);
    const uint8_t* p = dec->RgbaData();
    if (!p || dec->Width() == 0 || dec->Height() == 0) {
        return SolsticeV1_ResultFailure;
    }
    *OutPixels = p;
    *OutStrideBytes = dec->RgbaStrideBytes();
    *OutBufferBytes = static_cast<size_t>(dec->Width()) * static_cast<size_t>(dec->Height()) * 4u;
    return SolsticeV1_ResultSuccess;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_VideoDecoderGetPtsSeconds(
    SolsticeV1_VideoDecoder Decoder,
    double* OutSeconds) {
    if (!Decoder || !OutSeconds) {
        return SolsticeV1_ResultFailure;
    }
    *OutSeconds = AsDecoder(Decoder)->PtsSeconds();
    return SolsticeV1_ResultSuccess;
}

#else

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_VideoDecoderOpen(const char* Path, SolsticeV1_VideoDecoder* OutDecoder) {
    if (!Path || !OutDecoder) {
        return SolsticeV1_ResultFailure;
    }
    *OutDecoder = SolsticeV1_VideoDecoderInvalid;
    return SolsticeV1_ResultNotSupported;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_VideoDecoderClose(SolsticeV1_VideoDecoder Decoder) {
    (void)Decoder;
    return SolsticeV1_ResultSuccess;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_VideoDecoderAdvanceFrame(SolsticeV1_VideoDecoder Decoder) {
    if (!Decoder) {
        return SolsticeV1_ResultFailure;
    }
    return SolsticeV1_ResultNotSupported;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_VideoDecoderGetDimensions(
    SolsticeV1_VideoDecoder Decoder,
    uint32_t* OutWidth,
    uint32_t* OutHeight) {
    if (!OutWidth || !OutHeight) {
        return SolsticeV1_ResultFailure;
    }
    if (!Decoder) {
        return SolsticeV1_ResultFailure;
    }
    return SolsticeV1_ResultNotSupported;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_VideoDecoderGetRgba(
    SolsticeV1_VideoDecoder Decoder,
    const uint8_t** OutPixels,
    uint32_t* OutStrideBytes,
    size_t* OutBufferBytes) {
    if (!OutPixels || !OutStrideBytes || !OutBufferBytes) {
        return SolsticeV1_ResultFailure;
    }
    if (!Decoder) {
        return SolsticeV1_ResultFailure;
    }
    return SolsticeV1_ResultNotSupported;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_VideoDecoderGetPtsSeconds(
    SolsticeV1_VideoDecoder Decoder,
    double* OutSeconds) {
    if (!OutSeconds) {
        return SolsticeV1_ResultFailure;
    }
    if (!Decoder) {
        return SolsticeV1_ResultFailure;
    }
    return SolsticeV1_ResultNotSupported;
}

#endif

} // extern "C"
