#include <Multimedia/FfmpegVideoDecoder.hxx>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

#ifndef AV_ERROR_MAX_STRING_SIZE
#define AV_ERROR_MAX_STRING_SIZE 64
#endif

#include <algorithm>
#include <cstring>
#include <utility>
#include <vector>

namespace Solstice::Multimedia {

struct FfmpegVideoDecoder::Impl {
    AVFormatContext* Fmt = nullptr;
    AVCodecContext* Codec = nullptr;
    const AVCodec* CodecPtr = nullptr;
    int VideoStreamIndex = -1;
    AVPacket* Pkt = nullptr;
    AVFrame* Frame = nullptr;
    SwsContext* Sws = nullptr;

    int SrcW = 0;
    int SrcH = 0;
    AVPixelFormat SrcFmt = AV_PIX_FMT_NONE;

    std::vector<uint8_t> Rgba;
    uint32_t OutW = 0;
    uint32_t OutH = 0;
    double PtsSec = 0.0;
    bool Eof = false;

    void ResetSws() {
        if (Sws) {
            sws_freeContext(Sws);
            Sws = nullptr;
        }
        SrcW = 0;
        SrcH = 0;
        SrcFmt = AV_PIX_FMT_NONE;
    }

    void FreeAll() {
        ResetSws();
        if (Frame) {
            av_frame_free(&Frame);
        }
        if (Pkt) {
            av_packet_free(&Pkt);
        }
        if (Codec) {
            avcodec_free_context(&Codec);
        }
        if (Fmt) {
            avformat_close_input(&Fmt);
        }
        Rgba.clear();
        OutW = 0;
        OutH = 0;
        PtsSec = 0.0;
        Eof = false;
        VideoStreamIndex = -1;
        CodecPtr = nullptr;
    }

    static void FfErr(int Code, std::string& Err, const char* What) {
        char Buf[AV_ERROR_MAX_STRING_SIZE];
        if (av_strerror(Code, Buf, sizeof(Buf)) == 0) {
            Err = std::string(What) + ": " + Buf;
        } else {
            Err = std::string(What) + ": error " + std::to_string(Code);
        }
    }

    bool EnsureSws(AVFrame* Src, std::string& Err) {
        const int w = Src->width;
        const int h = Src->height;
        const AVPixelFormat fmt = static_cast<AVPixelFormat>(Src->format);
        if (w <= 0 || h <= 0 || fmt == AV_PIX_FMT_NONE) {
            Err = "invalid decoded frame dimensions/format";
            return false;
        }
        if (Sws && (SrcW == w && SrcH == h && SrcFmt == fmt)) {
            return true;
        }
        ResetSws();
        Sws = sws_getContext(
            w,
            h,
            fmt,
            w,
            h,
            AV_PIX_FMT_RGBA,
            SWS_BILINEAR,
            nullptr,
            nullptr,
            nullptr);
        if (!Sws) {
            Err = "sws_getContext failed";
            return false;
        }
        SrcW = w;
        SrcH = h;
        SrcFmt = fmt;
        OutW = static_cast<uint32_t>(w);
        OutH = static_cast<uint32_t>(h);
        const int need = av_image_get_buffer_size(AV_PIX_FMT_RGBA, w, h, 1);
        if (need < 0) {
            Err = "av_image_get_buffer_size failed";
            return false;
        }
        Rgba.resize(static_cast<size_t>(need));
        return true;
    }

    bool ConvertFrame(std::string& Err) {
        if (!EnsureSws(Frame, Err)) {
            return false;
        }
        uint8_t* dstSlice[4] = {Rgba.data(), nullptr, nullptr, nullptr};
        int dstLinesize[4] = {static_cast<int>(OutW * 4u), 0, 0, 0};
        const int ret = sws_scale(
            Sws,
            Frame->data,
            Frame->linesize,
            0,
            SrcH,
            dstSlice,
            dstLinesize);
        if (ret < 0) {
            FfErr(ret, Err, "sws_scale");
            return false;
        }

        if (Frame->best_effort_timestamp != AV_NOPTS_VALUE && VideoStreamIndex >= 0 && Fmt && Fmt->streams[VideoStreamIndex]) {
            const AVRational tb = Fmt->streams[VideoStreamIndex]->time_base;
            PtsSec = static_cast<double>(Frame->best_effort_timestamp) * av_q2d(tb);
        } else {
            PtsSec = 0.0;
        }
        return true;
    }
};

FfmpegVideoDecoder::FfmpegVideoDecoder() : m_Impl(std::make_unique<Impl>()) {}

FfmpegVideoDecoder::~FfmpegVideoDecoder() {
    Close();
}

FfmpegVideoDecoder::FfmpegVideoDecoder(FfmpegVideoDecoder&& other) noexcept : m_Impl(std::move(other.m_Impl)) {
    other.m_Impl = std::make_unique<Impl>();
}

FfmpegVideoDecoder& FfmpegVideoDecoder::operator=(FfmpegVideoDecoder&& other) noexcept {
    if (this != &other) {
        Close();
        m_Impl = std::move(other.m_Impl);
        other.m_Impl = std::make_unique<Impl>();
    }
    return *this;
}

bool FfmpegVideoDecoder::Open(const std::string& Path, std::string& Err) {
    Close();
    Err.clear();

    Impl& d = *m_Impl;
    int ret = avformat_open_input(&d.Fmt, Path.c_str(), nullptr, nullptr);
    if (ret < 0) {
        Impl::FfErr(ret, Err, "avformat_open_input");
        d.FreeAll();
        return false;
    }
    ret = avformat_find_stream_info(d.Fmt, nullptr);
    if (ret < 0) {
        Impl::FfErr(ret, Err, "avformat_find_stream_info");
        d.FreeAll();
        return false;
    }

    d.VideoStreamIndex = av_find_best_stream(d.Fmt, AVMEDIA_TYPE_VIDEO, -1, -1, &d.CodecPtr, 0);
    if (d.VideoStreamIndex < 0 || !d.CodecPtr) {
        Err = "no video stream";
        d.FreeAll();
        return false;
    }

    d.Codec = avcodec_alloc_context3(d.CodecPtr);
    if (!d.Codec) {
        Err = "avcodec_alloc_context3 failed";
        d.FreeAll();
        return false;
    }

    AVStream* st = d.Fmt->streams[d.VideoStreamIndex];
    ret = avcodec_parameters_to_context(d.Codec, st->codecpar);
    if (ret < 0) {
        Impl::FfErr(ret, Err, "avcodec_parameters_to_context");
        d.FreeAll();
        return false;
    }

    ret = avcodec_open2(d.Codec, d.CodecPtr, nullptr);
    if (ret < 0) {
        Impl::FfErr(ret, Err, "avcodec_open2");
        d.FreeAll();
        return false;
    }

    d.Pkt = av_packet_alloc();
    d.Frame = av_frame_alloc();
    if (!d.Pkt || !d.Frame) {
        Err = "av_packet_alloc / av_frame_alloc failed";
        d.FreeAll();
        return false;
    }

    d.Eof = false;
    return true;
}

void FfmpegVideoDecoder::Close() {
    if (m_Impl) {
        m_Impl->FreeAll();
    }
}

bool FfmpegVideoDecoder::IsOpen() const {
    return m_Impl && m_Impl->Fmt != nullptr && m_Impl->Codec != nullptr;
}

bool FfmpegVideoDecoder::DecodeNextFrame(std::string& Err) {
    Err.clear();
    if (!IsOpen()) {
        Err = "decoder not open";
        return false;
    }
    Impl& d = *m_Impl;

    for (;;) {
        int ret = avcodec_receive_frame(d.Codec, d.Frame);
        if (ret == 0) {
            return d.ConvertFrame(Err);
        }
        if (ret == AVERROR_EOF) {
            return false;
        }
        if (ret != AVERROR(EAGAIN)) {
            Impl::FfErr(ret, Err, "avcodec_receive_frame");
            return false;
        }

        if (d.Eof) {
            return false;
        }

        ret = av_read_frame(d.Fmt, d.Pkt);
        if (ret == AVERROR_EOF) {
            d.Eof = true;
            ret = avcodec_send_packet(d.Codec, nullptr);
            if (ret < 0 && ret != AVERROR_EOF) {
                Impl::FfErr(ret, Err, "avcodec_send_packet (flush)");
                return false;
            }
            continue;
        }
        if (ret < 0) {
            Impl::FfErr(ret, Err, "av_read_frame");
            av_packet_unref(d.Pkt);
            return false;
        }
        if (d.Pkt->stream_index != d.VideoStreamIndex) {
            av_packet_unref(d.Pkt);
            continue;
        }
        ret = avcodec_send_packet(d.Codec, d.Pkt);
        av_packet_unref(d.Pkt);
        if (ret < 0) {
            Impl::FfErr(ret, Err, "avcodec_send_packet");
            return false;
        }
    }
}

uint32_t FfmpegVideoDecoder::Width() const {
    return m_Impl ? m_Impl->OutW : 0u;
}

uint32_t FfmpegVideoDecoder::Height() const {
    return m_Impl ? m_Impl->OutH : 0u;
}

const uint8_t* FfmpegVideoDecoder::RgbaData() const {
    if (!m_Impl || m_Impl->Rgba.empty()) {
        return nullptr;
    }
    return m_Impl->Rgba.data();
}

uint32_t FfmpegVideoDecoder::RgbaStrideBytes() const {
    return m_Impl ? (m_Impl->OutW * 4u) : 0u;
}

double FfmpegVideoDecoder::PtsSeconds() const {
    return m_Impl ? m_Impl->PtsSec : 0.0;
}

} // namespace Solstice::Multimedia
