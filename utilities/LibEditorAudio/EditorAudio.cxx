#include <Solstice/EditorAudio/EditorAudio.hxx>

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Solstice::EditorAudio {

namespace {

int g_EditorAudioRef = 0;

bool openSdlAudioOnce() {
    if (SDL_WasInit(SDL_INIT_AUDIO)) {
        return true;
    }
    return SDL_InitSubSystem(SDL_INIT_AUDIO);
}

bool decodeDecoderToMonoF32(MIX_AudioDecoder* dec, std::vector<float>& outMono, int& outRate, std::string* outError) {
    outMono.clear();
    if (!dec) {
        if (outError) {
            *outError = "No audio decoder.";
        }
        return false;
    }
    SDL_AudioSpec want{};
    want.format = SDL_AUDIO_F32;
    want.channels = 1;
    want.freq = 48000;

    const int kBuf = 1 << 18;
    std::vector<std::uint8_t> buffer(static_cast<size_t>(kBuf));
    for (;;) {
        const int n = MIX_DecodeAudio(dec, buffer.data(), static_cast<int>(buffer.size()), &want);
        if (n < 0) {
            if (outError) {
                *outError = std::string("MIX_DecodeAudio failed: ") + SDL_GetError();
            }
            return false;
        }
        if (n == 0) {
            break;
        }
        if (want.format != SDL_AUDIO_F32) {
            if (outError) {
                *outError = "Unexpected output format (expected F32).";
            }
            return false;
        }
        const int sampleCount = n / static_cast<int>(sizeof(float));
        const auto* s = reinterpret_cast<const float*>(buffer.data());
        outMono.insert(outMono.end(), s, s + sampleCount);
    }
    outRate = want.freq;
    return !outMono.empty();
}

} // namespace

bool Init() noexcept {
    if (!openSdlAudioOnce()) {
        return false;
    }
    if (!MIX_Init()) {
        return false;
    }
    ++g_EditorAudioRef;
    return true;
}

void Shutdown() noexcept {
    if (g_EditorAudioRef == 0) {
        return;
    }
    --g_EditorAudioRef;
    MIX_Quit();
}

bool IsReady() noexcept {
    return g_EditorAudioRef > 0;
}

bool DecodedPcm::DecodeFileUtf8(const char* path, std::string* outError) {
    Clear();
    if (!path) {
        if (outError) {
            *outError = "Null path.";
        }
        return false;
    }
    if (g_EditorAudioRef < 1) {
        if (outError) {
            *outError = "Call Solstice::EditorAudio::Init() first.";
        }
        return false;
    }
    MIX_AudioDecoder* dec = MIX_CreateAudioDecoder(path, 0);
    if (!dec) {
        if (outError) {
            *outError = std::string("MIX_CreateAudioDecoder failed: ") + SDL_GetError();
        }
        return false;
    }
    int rate = 0;
    if (!decodeDecoderToMonoF32(dec, samplesMono, rate, outError)) {
        MIX_DestroyAudioDecoder(dec);
        return false;
    }
    sampleRateHz = rate;
    MIX_DestroyAudioDecoder(dec);
    return true;
}

bool DecodedPcm::DecodeMemory(std::span<const std::byte> bytes, std::string* outError) {
    Clear();
    if (bytes.empty()) {
        if (outError) {
            *outError = "Empty audio buffer.";
        }
        return false;
    }
    if (g_EditorAudioRef < 1) {
        if (outError) {
            *outError = "Call Solstice::EditorAudio::Init() first.";
        }
        return false;
    }
    std::vector<std::byte> own(bytes.begin(), bytes.end());
    SDL_IOStream* io = SDL_IOFromConstMem(own.data(), own.size());
    if (!io) {
        if (outError) {
            *outError = std::string("SDL_IOFromConstMem failed: ") + SDL_GetError();
        }
        return false;
    }
    MIX_AudioDecoder* dec = MIX_CreateAudioDecoder_IO(io, true, 0);
    if (!dec) {
        if (outError) {
            *outError = std::string("MIX_CreateAudioDecoder_IO failed: ") + SDL_GetError();
        }
        return false;
    }
    int rate = 0;
    if (!decodeDecoderToMonoF32(dec, samplesMono, rate, outError)) {
        MIX_DestroyAudioDecoder(dec);
        return false;
    }
    sampleRateHz = rate;
    MIX_DestroyAudioDecoder(dec);
    return true;
}

double DecodedPcm::DurationSeconds() const {
    if (sampleRateHz <= 0 || samplesMono.empty()) {
        return 0.0;
    }
    return static_cast<double>(samplesMono.size()) / static_cast<double>(sampleRateHz);
}

std::vector<PeakMinMax> BuildMinMaxPeaks(const DecodedPcm& pcm, int columnCount) {
    std::vector<PeakMinMax> out;
    if (columnCount < 1 || pcm.samplesMono.empty()) {
        return out;
    }
    out.resize(static_cast<size_t>(columnCount));
    const size_t n = pcm.samplesMono.size();
    for (int c = 0; c < columnCount; ++c) {
        const size_t i0 = (static_cast<size_t>(c) * n) / static_cast<size_t>(columnCount);
        const size_t i1 = (static_cast<size_t>(c + 1) * n) / static_cast<size_t>(columnCount);
        float lo = 0.f;
        float hi = 0.f;
        for (size_t i = i0; i < i1 && i < n; ++i) {
            const float s = pcm.samplesMono[i];
            lo = std::min(lo, s);
            hi = std::max(hi, s);
        }
        out[static_cast<size_t>(c)] = PeakMinMax{lo, hi};
    }
    return out;
}

ScrubPlayer::ScrubPlayer(ScrubPlayer&& o) noexcept
    : m_Samples(std::move(o.m_Samples))
    , m_Rate(o.m_Rate)
    , m_Stream(o.m_Stream)
    , m_Playhead(o.m_Playhead)
    , m_Playing(o.m_Playing) {
    o.m_Rate = 0;
    o.m_Stream = nullptr;
    o.m_Playhead = 0;
    o.m_Playing = false;
}

ScrubPlayer& ScrubPlayer::operator=(ScrubPlayer&& o) noexcept {
    if (this != &o) {
        Close();
        m_Samples = std::move(o.m_Samples);
        m_Rate = o.m_Rate;
        m_Stream = o.m_Stream;
        m_Playhead = o.m_Playhead;
        m_Playing = o.m_Playing;
        o.m_Rate = 0;
        o.m_Stream = nullptr;
        o.m_Playhead = 0;
        o.m_Playing = false;
    }
    return *this;
}

ScrubPlayer::~ScrubPlayer() {
    Close();
}

void ScrubPlayer::Close() noexcept {
    if (m_Stream) {
        SDL_DestroyAudioStream(m_Stream);
        m_Stream = nullptr;
    }
    m_Samples.clear();
    m_Rate = 0;
    m_Playhead = 0;
    m_Playing = false;
}

void ScrubPlayer::LoadPcm(const DecodedPcm& decoded, std::string* outError) {
    Close();
    if (decoded.samplesMono.empty() || decoded.sampleRateHz <= 0) {
        if (outError) {
            *outError = "Empty decoded PCM.";
        }
        return;
    }
    m_Samples = decoded.samplesMono;
    m_Rate = decoded.sampleRateHz;
    SDL_AudioSpec spec{};
    spec.format = SDL_AUDIO_F32;
    spec.channels = 1;
    spec.freq = m_Rate;
    m_Stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (!m_Stream) {
        m_Samples.clear();
        m_Rate = 0;
        if (outError) {
            *outError = std::string("SDL_OpenAudioDeviceStream failed: ") + SDL_GetError();
        }
        return;
    }
    (void)SDL_ResumeAudioStreamDevice(m_Stream);
    m_Playhead = 0;
    m_Playing = false;
}

void ScrubPlayer::PauseDeviceIfNeeded() noexcept {
    if (m_Stream) {
        (void)SDL_PauseAudioStreamDevice(m_Stream);
    }
}

void ScrubPlayer::ResumeDeviceIfNeeded() noexcept {
    if (m_Stream) {
        (void)SDL_ResumeAudioStreamDevice(m_Stream);
    }
}

void ScrubPlayer::Play() noexcept {
    m_Playing = true;
    ResumeDeviceIfNeeded();
}

void ScrubPlayer::Stop() noexcept {
    m_Playing = false;
    if (m_Stream) {
        (void)SDL_ClearAudioStream(m_Stream);
    }
    PauseDeviceIfNeeded();
}

void ScrubPlayer::Seek01(float t) noexcept {
    if (m_Samples.empty()) {
        return;
    }
    t = std::max(0.f, std::min(1.f, t));
    m_Playhead = (size_t)std::llround(static_cast<double>(t) * static_cast<double>(m_Samples.size() - 1));
    if (m_Stream) {
        (void)SDL_ClearAudioStream(m_Stream);
    }
}

void ScrubPlayer::PumpPlay(float deltaSeconds) noexcept {
    if (!m_Playing || m_Samples.empty() || m_Rate <= 0 || !m_Stream) {
        return;
    }
    if (deltaSeconds <= 0.f) {
        return;
    }
    const size_t maxFrames = m_Samples.size();
    if (m_Playhead >= maxFrames) {
        m_Playing = false;
        return;
    }
    const size_t toCopy = (size_t)std::llround(std::min<double>(0.2, (double)deltaSeconds) * (double)m_Rate) + 1u;
    const size_t n = std::min(toCopy, maxFrames - m_Playhead);
    (void)SDL_PutAudioStreamData(m_Stream, m_Samples.data() + m_Playhead, static_cast<int>(n * sizeof(float)));
    m_Playhead += n;
    if (m_Playhead >= maxFrames) {
        m_Playing = false;
    }
}

void ScrubPlayer::PumpScrub(float t01, float deltaSeconds) noexcept {
    if (m_Samples.empty() || m_Rate <= 0 || !m_Stream) {
        return;
    }
    if (deltaSeconds <= 0.f) {
        return;
    }
    t01 = std::max(0.f, std::min(1.f, t01));
    const size_t maxFrames = m_Samples.size();
    const size_t start = (size_t)std::llround((double)t01 * (double)(maxFrames - 1));
    const size_t toCopy = std::min((size_t)std::llround((double)deltaSeconds * (double)m_Rate) + 1u, maxFrames - start);
    (void)SDL_ClearAudioStream(m_Stream);
    (void)SDL_PutAudioStreamData(m_Stream, m_Samples.data() + start, static_cast<int>(toCopy * sizeof(float)));
    m_Playhead = start + toCopy;
    ResumeDeviceIfNeeded();
}

float ScrubPlayer::Playhead01() const noexcept {
    if (m_Samples.empty()) {
        return 0.f;
    }
    return (float)(static_cast<double>(m_Playhead) / (double)std::max<size_t>(1, m_Samples.size() - 1));
}

} // namespace Solstice::EditorAudio
