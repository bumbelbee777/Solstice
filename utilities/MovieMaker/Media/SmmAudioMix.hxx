#pragma once

#include <Parallax/DevSessionAssetResolver.hxx>
#include <Parallax/ParallaxScene.hxx>

#include <cstdint>
#include <filesystem>
#include <string>

namespace Smm::Audio {

/// Parameters for an offline timeline-evaluated audio mixdown.
struct OfflineMixdownParams {
    std::uint64_t StartTick{0};
    /// 0 = use scene timeline end.
    std::uint64_t EndTick{0};
    /// Output sample rate; clamped to 8000..192000.
    int SampleRateHz{48000};
    /// Per-frame timeline sampling cadence (matches video FPS so volume/pitch automation tracks the visual).
    int AutomationFps{60};
    /// Output WAV path (PCM s16). Parent directories will be created if missing.
    std::filesystem::path OutputWavPath;
    /// Maximum number of `AudioSourceElement` rows to mix (safety cap; 0 = no cap).
    int MaxSourcesCap{64};
};

struct OfflineMixdownStats {
    std::uint32_t SourcesConsidered{0};
    std::uint32_t SourcesMixed{0};
    std::uint32_t SourcesSkippedNoAsset{0};
    std::uint32_t SourcesSkippedDecode{0};
    double DurationSeconds{0.0};
    std::size_t AutomationSamples{0};
    bool WroteFile{false};
};

/// Renders a deterministic offline mixdown of every `AudioSourceElement` in `scene` over [Start, End],
/// applying `Volume` and `Pitch` automation sampled at `AutomationFps`. Decoding uses the editor audio
/// path (which must have been initialized once at app start). Output is a 48 kHz mono PCM s16 WAV by
/// default; channel count is fixed mono since the current `AudioSourceState` is mono. Returns false on
/// hard failures; soft failures (missing assets, decode errors) populate stats and continue.
bool RenderEvaluatedMixdownToWav(const Solstice::Parallax::ParallaxScene& scene,
    Solstice::Parallax::DevSessionAssetResolver& resolver, const OfflineMixdownParams& params,
    OfflineMixdownStats& outStats, std::string& errOut);

} // namespace Smm::Audio
