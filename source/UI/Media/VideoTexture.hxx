#pragma once

#include <bgfx/bgfx.h>
#include <string>

#include <Multimedia/FfmpegVideoDecoder.hxx>

namespace Solstice::UI {

/**
 * Decodes a video file to RGBA and uploads each frame via bgfx::updateTexture2D.
 * Single-threaded; call after bgfx init. Requires engine built with FFmpeg.
 */
class VideoTexture {
public:
    VideoTexture() = default;
    VideoTexture(const VideoTexture&) = delete;
    VideoTexture& operator=(const VideoTexture&) = delete;
    VideoTexture(VideoTexture&&) = delete;
    VideoTexture& operator=(VideoTexture&&) = delete;
    ~VideoTexture();

    bool Open(const std::string& Path, std::string& Err);
    void Close();

    bool IsOpen() const { return m_Decoder.IsOpen() && bgfx::isValid(m_Texture); }

    /** Decode next frame and upload to GPU. Returns false on EOF or error. */
    bool StepFrame(std::string& Err);

    bgfx::TextureHandle Handle() const { return m_Texture; }
    uint32_t Width() const { return m_Decoder.Width(); }
    uint32_t Height() const { return m_Decoder.Height(); }
    double PtsSeconds() const { return m_Decoder.PtsSeconds(); }

private:
    void EnsureTexture(uint32_t W, uint32_t H);

    Solstice::Multimedia::FfmpegVideoDecoder m_Decoder;
    bgfx::TextureHandle m_Texture = BGFX_INVALID_HANDLE;
    uint32_t m_AllocW = 0;
    uint32_t m_AllocH = 0;
};

} // namespace Solstice::UI
