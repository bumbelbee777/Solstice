#include "Media/SmmAudioMix.hxx"

#include <Parallax/ParallaxScene.hxx>
#include <Solstice/EditorAudio/EditorAudio.hxx>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <span>
#include <vector>

namespace Smm::Audio {
namespace {

constexpr int kMinSampleRate = 8000;
constexpr int kMaxSampleRate = 192000;
constexpr int kMinAutomationFps = 1;
constexpr int kMaxAutomationFps = 240;

struct DecodedSource {
    Solstice::Parallax::ElementIndex Element{Solstice::Parallax::PARALLAX_INVALID_INDEX};
    std::vector<float> SamplesMono;
    int DecodedRateHz{48000};
};

bool DecodeAudioAssetIntoMono(Solstice::Parallax::DevSessionAssetResolver& resolver, std::uint64_t hash,
    DecodedSource& out, std::string& outError) {
    Solstice::Parallax::AssetData ad{};
    if (!resolver.Resolve(hash, ad) || ad.Bytes.empty()) {
        outError = "asset bytes unavailable";
        return false;
    }
    Solstice::EditorAudio::DecodedPcm pcm;
    const std::span<const std::byte> bytes(ad.Bytes.data(), ad.Bytes.size());
    std::string err;
    if (!pcm.DecodeMemory(bytes, &err)) {
        outError = err.empty() ? "decode failed" : err;
        return false;
    }
    out.SamplesMono = std::move(pcm.samplesMono);
    out.DecodedRateHz = pcm.sampleRateHz > 0 ? pcm.sampleRateHz : 48000;
    return true;
}

/// Linear sample interpolation given a fractional source index.
inline float SampleAt(const std::vector<float>& mono, double indexF) noexcept {
    if (mono.empty()) {
        return 0.f;
    }
    if (indexF <= 0.0) {
        return mono.front();
    }
    const std::size_t hi = static_cast<std::size_t>(indexF) + 1u;
    if (hi >= mono.size()) {
        return mono.back();
    }
    const std::size_t lo = static_cast<std::size_t>(indexF);
    const float t = static_cast<float>(indexF - static_cast<double>(lo));
    const float a = mono[lo];
    const float b = mono[hi];
    return a + (b - a) * t;
}

bool WriteWavMonoF32(const std::filesystem::path& path, const std::vector<float>& samplesMono, int sampleRateHz,
    std::string& errOut) {
    std::error_code ec;
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path(), ec);
    }
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        errOut = "could not open output WAV path";
        return false;
    }

    std::vector<std::int16_t> pcm16;
    pcm16.reserve(samplesMono.size());
    for (float s : samplesMono) {
        if (s > 1.f) s = 1.f;
        if (s < -1.f) s = -1.f;
        pcm16.push_back(static_cast<std::int16_t>(std::lround(s * 32767.f)));
    }
    const std::uint32_t dataBytes = static_cast<std::uint32_t>(pcm16.size() * sizeof(std::int16_t));
    const std::uint32_t fmtChunkSize = 16;
    const std::uint16_t channels = 1;
    const std::uint16_t bitsPerSample = 16;
    const std::uint32_t byteRate = static_cast<std::uint32_t>(sampleRateHz) * channels * (bitsPerSample / 8);
    const std::uint16_t blockAlign = channels * (bitsPerSample / 8);
    const std::uint32_t riffSize = 4 + (8 + fmtChunkSize) + (8 + dataBytes);

    auto write32 = [&](std::uint32_t v) {
        unsigned char b[4] = {
            static_cast<unsigned char>(v & 0xFFu),
            static_cast<unsigned char>((v >> 8) & 0xFFu),
            static_cast<unsigned char>((v >> 16) & 0xFFu),
            static_cast<unsigned char>((v >> 24) & 0xFFu),
        };
        out.write(reinterpret_cast<const char*>(b), 4);
    };
    auto write16 = [&](std::uint16_t v) {
        unsigned char b[2] = {
            static_cast<unsigned char>(v & 0xFFu),
            static_cast<unsigned char>((v >> 8) & 0xFFu),
        };
        out.write(reinterpret_cast<const char*>(b), 2);
    };
    out.write("RIFF", 4);
    write32(riffSize);
    out.write("WAVE", 4);
    out.write("fmt ", 4);
    write32(fmtChunkSize);
    write16(1); // PCM
    write16(channels);
    write32(static_cast<std::uint32_t>(sampleRateHz));
    write32(byteRate);
    write16(blockAlign);
    write16(bitsPerSample);
    out.write("data", 4);
    write32(dataBytes);
    if (!pcm16.empty()) {
        out.write(reinterpret_cast<const char*>(pcm16.data()), static_cast<std::streamsize>(dataBytes));
    }
    if (!out) {
        errOut = "WAV write failed";
        return false;
    }
    return true;
}

} // namespace

bool RenderEvaluatedMixdownToWav(const Solstice::Parallax::ParallaxScene& scene,
    Solstice::Parallax::DevSessionAssetResolver& resolver, const OfflineMixdownParams& params,
    OfflineMixdownStats& outStats, std::string& errOut) {
    outStats = {};
    errOut.clear();

    const int sampleRate = std::clamp(params.SampleRateHz, kMinSampleRate, kMaxSampleRate);
    const int autoFps = std::clamp(params.AutomationFps, kMinAutomationFps, kMaxAutomationFps);
    const std::uint64_t startTick = params.StartTick;
    std::uint64_t endTick = params.EndTick == 0 ? scene.GetTimelineDurationTicks() : params.EndTick;
    if (endTick <= startTick) {
        errOut = "invalid mixdown tick range";
        return false;
    }
    const std::uint32_t tps = std::max(1u, scene.GetTicksPerSecond());
    const double durationSec = static_cast<double>(endTick - startTick) / static_cast<double>(tps);
    if (!(durationSec > 0.0)) {
        errOut = "mixdown duration is zero";
        return false;
    }
    outStats.DurationSeconds = durationSec;
    outStats.AutomationSamples = static_cast<std::size_t>(std::ceil(durationSec * static_cast<double>(autoFps)));

    if (!Solstice::EditorAudio::Init()) {
        errOut = "editor audio not available; mixdown skipped";
        return false;
    }

    // Collect sources with non-zero AudioAsset.
    struct SourceInfo {
        Solstice::Parallax::ElementIndex Element{Solstice::Parallax::PARALLAX_INVALID_INDEX};
        std::uint64_t AssetHash{0};
    };
    std::vector<SourceInfo> srcs;
    for (Solstice::Parallax::ElementIndex ei = 0; ei < scene.GetElements().size(); ++ei) {
        if (Solstice::Parallax::GetElementSchema(scene, ei) != "AudioSourceElement") {
            continue;
        }
        const Solstice::Parallax::AttributeValue av = Solstice::Parallax::GetAttribute(scene, ei, "AudioAsset");
        const std::uint64_t* h = std::get_if<std::uint64_t>(&av);
        if (!h || *h == 0) {
            ++outStats.SourcesSkippedNoAsset;
            continue;
        }
        ++outStats.SourcesConsidered;
        SourceInfo si;
        si.Element = ei;
        si.AssetHash = *h;
        srcs.push_back(si);
        if (params.MaxSourcesCap > 0 && static_cast<int>(srcs.size()) >= params.MaxSourcesCap) {
            break;
        }
    }

    if (outStats.SourcesConsidered == 0) {
        errOut = "no AudioSourceElement with AudioAsset bytes";
        return false;
    }

    std::vector<DecodedSource> decoded;
    decoded.reserve(srcs.size());
    for (const SourceInfo& si : srcs) {
        DecodedSource d;
        d.Element = si.Element;
        std::string err;
        if (!DecodeAudioAssetIntoMono(resolver, si.AssetHash, d, err)) {
            ++outStats.SourcesSkippedDecode;
            continue;
        }
        decoded.push_back(std::move(d));
    }
    if (decoded.empty()) {
        errOut = "no audio sources could be decoded";
        return false;
    }
    outStats.SourcesMixed = static_cast<std::uint32_t>(decoded.size());

    // Pre-sample volume/pitch automation per source on a fixed grid; lerp between automation samples.
    // Parallelize via OpenMP when available: each automation sample is an independent EvaluateScene
    // call (read-only on the scene), so this is a stateless segment that maps cleanly to a parallel
    // loop with no temporal dependencies.
    std::vector<std::vector<float>> autoVol(decoded.size());
    std::vector<std::vector<float>> autoPit(decoded.size());
    for (std::size_t k = 0; k < decoded.size(); ++k) {
        autoVol[k].resize(outStats.AutomationSamples + 2u, 1.f);
        autoPit[k].resize(outStats.AutomationSamples + 2u, 1.f);
    }
    const std::ptrdiff_t totalAutoSamples = static_cast<std::ptrdiff_t>(outStats.AutomationSamples) + 1;
#if defined(SOLSTICE_HAVE_OPENMP)
    #pragma omp parallel for schedule(dynamic, 8)
#endif
    for (std::ptrdiff_t fi = 0; fi < totalAutoSamples; ++fi) {
        const double tSec = static_cast<double>(fi) / static_cast<double>(autoFps);
        std::uint64_t tick = startTick + static_cast<std::uint64_t>(std::llround(tSec * static_cast<double>(tps)));
        if (tick >= endTick) {
            tick = endTick - 1;
        }
        Solstice::Parallax::SceneEvaluationResult eval{};
        Solstice::Parallax::EvaluateScene(scene, tick, eval);
        for (std::size_t k = 0; k < decoded.size(); ++k) {
            float vol = 1.f;
            float pit = 1.f;
            for (const Solstice::Parallax::AudioSourceState& as : eval.AudioStates) {
                if (as.Element == decoded[k].Element) {
                    vol = as.Volume;
                    pit = as.Pitch;
                    break;
                }
            }
            autoVol[k][static_cast<std::size_t>(fi)] = vol;
            autoPit[k][static_cast<std::size_t>(fi)] = pit;
        }
    }

    const std::size_t totalOutSamples = static_cast<std::size_t>(std::ceil(durationSec * static_cast<double>(sampleRate)));
    std::vector<float> out(totalOutSamples, 0.f);

    // Per-source playhead with running pitch integration in source-rate samples.
    std::vector<double> srcReadPos(decoded.size(), 0.0);

    for (std::size_t i = 0; i < totalOutSamples; ++i) {
        const double tSec = static_cast<double>(i) / static_cast<double>(sampleRate);
        // Automation lookup.
        const double autoIdxF = tSec * static_cast<double>(autoFps);
        const std::size_t aLo = static_cast<std::size_t>(autoIdxF);
        const std::size_t aHi = std::min(aLo + 1u, outStats.AutomationSamples);
        const float aT = static_cast<float>(autoIdxF - static_cast<double>(aLo));

        float mixVal = 0.f;
        for (std::size_t k = 0; k < decoded.size(); ++k) {
            const std::vector<float>& vol = autoVol[k];
            const std::vector<float>& pit = autoPit[k];
            const float v = vol[aLo] + (vol[aHi] - vol[aLo]) * aT;
            const float p = pit[aLo] + (pit[aHi] - pit[aLo]) * aT;
            const double ratio = (static_cast<double>(decoded[k].DecodedRateHz) / static_cast<double>(sampleRate))
                * static_cast<double>(p > 0.001f ? p : 0.001f);
            const float s = SampleAt(decoded[k].SamplesMono, srcReadPos[k]);
            mixVal += s * (v > 0.f ? v : 0.f);
            srcReadPos[k] += ratio;
            if (srcReadPos[k] >= static_cast<double>(decoded[k].SamplesMono.size())) {
                srcReadPos[k] = static_cast<double>(decoded[k].SamplesMono.size());
            }
        }
        if (mixVal > 1.f) mixVal = 1.f;
        if (mixVal < -1.f) mixVal = -1.f;
        out[i] = mixVal;
    }

    std::string werr;
    if (!WriteWavMonoF32(params.OutputWavPath, out, sampleRate, werr)) {
        errOut = werr;
        return false;
    }
    outStats.WroteFile = true;
    return true;
}

} // namespace Smm::Audio
