#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace Solstice::Multimedia {

/**
 * Single-threaded software decode of the first video stream to packed RGBA8 (top-left origin).
 * Non-goals: hardware decode, network URLs, audio, seeking (see follow-ups in implementation file).
 */
class FfmpegVideoDecoder {
public:
    FfmpegVideoDecoder();
    ~FfmpegVideoDecoder();

    FfmpegVideoDecoder(const FfmpegVideoDecoder&) = delete;
    FfmpegVideoDecoder& operator=(const FfmpegVideoDecoder&) = delete;
    FfmpegVideoDecoder(FfmpegVideoDecoder&& other) noexcept;
    FfmpegVideoDecoder& operator=(FfmpegVideoDecoder&& other) noexcept;

    bool Open(const std::string& Path, std::string& Err);
    void Close();
    bool IsOpen() const;

    /** Demux + decode until one video frame is ready in RGBA, or return false on end/error. */
    bool DecodeNextFrame(std::string& Err);

    uint32_t Width() const;
    uint32_t Height() const;
    const uint8_t* RgbaData() const;
    uint32_t RgbaStrideBytes() const;
    double PtsSeconds() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

} // namespace Solstice::Multimedia
