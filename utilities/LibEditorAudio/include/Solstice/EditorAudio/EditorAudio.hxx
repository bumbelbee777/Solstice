#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

struct SDL_AudioStream;

namespace Solstice::EditorAudio {

/// Reference-counted SDL3 audio subsystem + ::MIX_Init for decoder APIs. Safe to call if SDL is already initialized.
bool Init() noexcept;
void Shutdown() noexcept;
bool IsReady() noexcept;

struct PeakMinMax {
    float min = 0.f;
    float max = 0.f;
};

struct DecodedPcm {
    std::vector<float> samplesMono;
    int sampleRateHz = 0;

    void Clear() noexcept {
        samplesMono.clear();
        sampleRateHz = 0;
    }

    /// Decodes the whole clip to 48kHz mono F32.
    bool DecodeFileUtf8(const char* path, std::string* outError);
    /// Same as file decode; memory must be detectable (WAV, Ogg, etc.). Buffer is copied.
    bool DecodeMemory(std::span<const std::byte> bytes, std::string* outError);

    [[nodiscard]] double DurationSeconds() const;
};

[[nodiscard]] std::vector<PeakMinMax> BuildMinMaxPeaks(const DecodedPcm& pcm, int columnCount);

/// Float mono playback with seek; supports linear play or per-frame scrub preview.
class ScrubPlayer {
public:
    ScrubPlayer() = default;
    ScrubPlayer(const ScrubPlayer&) = delete;
    ScrubPlayer& operator=(const ScrubPlayer&) = delete;
    ScrubPlayer(ScrubPlayer&&) noexcept;
    ScrubPlayer& operator=(ScrubPlayer&&) noexcept;
    ~ScrubPlayer();

    void Close() noexcept;
    void LoadPcm(const DecodedPcm& decoded, std::string* outError);

    void Play() noexcept;
    void Stop() noexcept;
    void Seek01(float t) noexcept; // 0..1, updates play head for Play mode

    /// Feed playback when not scrubbing (advances play head).
    void PumpPlay(float deltaSeconds) noexcept;
    /// One frame of audio at normalized time; use while dragging a waveform.
    void PumpScrub(float t01, float deltaSeconds) noexcept;

    [[nodiscard]] bool IsLoaded() const noexcept { return !m_Samples.empty() && m_Stream != nullptr; }
    [[nodiscard]] bool IsPlaying() const noexcept { return m_Playing; }
    /// Play head in 0..1 for linear playback.
    [[nodiscard]] float Playhead01() const noexcept;

private:
    void PauseDeviceIfNeeded() noexcept;
    void ResumeDeviceIfNeeded() noexcept;

    std::vector<float> m_Samples;
    int m_Rate = 0;
    SDL_AudioStream* m_Stream = nullptr;
    size_t m_Playhead = 0;
    bool m_Playing = false;
};

} // namespace Solstice::EditorAudio
