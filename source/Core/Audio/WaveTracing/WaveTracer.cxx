#include "WaveTracer.hxx"
#include <Core/Audio/Audio.hxx>
#include <algorithm>

namespace Solstice::Core::Audio {

void WaveTracer::Initialize(const void* Scene) {
    m_CurrentScene = Scene;
    m_RayCache.clear();
}

void WaveTracer::Shutdown() {
    m_CurrentScene = nullptr;
    m_RayCache.clear();
}

int WaveTracer::GetRayCountForSource(const AudioSource&, float Distance) const {
    const float nearT = std::clamp(1.0f - Distance / 120.0f, 0.2f, 1.0f);
    return std::max(8, static_cast<int>(std::round(static_cast<float>(m_MaxRays) * nearT)));
}

void WaveTracer::TraceCPU(const std::vector<AudioSource>& Sources, const Math::Vec3& ListenerPos, std::vector<SoundRayResult>& OutResults) {
    OutResults.clear();
    OutResults.reserve(Sources.size());
    for (size_t i = 0; i < Sources.size(); ++i) {
        const AudioSource& s = Sources[i];
        const Math::Vec3 to = ListenerPos - s.Position;
        const float dist = std::max(0.05f, to.Magnitude());
        const int rays = GetRayCountForSource(s, dist);
        const float hitRatio = std::clamp(1.0f - dist / std::max(1.0f, s.MaxDistance), 0.0f, 1.0f);
        SoundRayResult r{};
        r.SourceId = static_cast<uint32_t>(i + 1);
        r.Distance = dist;
        r.IncidentAngle = std::abs(to.Normalized().Dot(s.Direction.Normalized()));
        r.Energy = hitRatio * (0.35f + 0.65f * (static_cast<float>(rays) / static_cast<float>(std::max(1, m_MaxRays))));
        r.HitOccluder = s.OcclusionFactor > 0.5f;
        OutResults.push_back(r);
    }
}

void WaveTracer::TraceGPU(const std::vector<AudioSource>& Sources, const Math::Vec3& ListenerPos, std::vector<SoundRayResult>& OutResults) {
    // Optional path placeholder; CPU remains authoritative.
    TraceCPU(Sources, ListenerPos, OutResults);
}

void WaveTracer::Trace(const std::vector<AudioSource>& Sources, const Math::Vec3& ListenerPos, std::vector<SoundRayResult>& OutResults) {
    TraceCPU(Sources, ListenerPos, OutResults);
}

void WaveTracer::ApplyResults(const std::vector<SoundRayResult>& Results, AudioManager& Audio) {
    for (const SoundRayResult& r : Results) {
        AudioEmitterHandle handle = static_cast<AudioEmitterHandle>(r.SourceId);
        const float occlusion = r.HitOccluder ? std::clamp(0.15f + (1.0f - r.Energy), 0.0f, 1.0f) : 0.0f;
        Audio.SetEmitterOcclusion(handle, occlusion);
        Audio.SetEmitterImmersion(handle, std::clamp(0.3f + 0.6f * (1.0f - occlusion), 0.0f, 1.0f), 0.4f, 0.25f);
    }
}

WaveTraceReverbParams WaveTracer::ComputeReverb(const std::vector<SoundRayResult>& Results) const {
    WaveTraceReverbParams p{};
    if (Results.empty()) {
        return p;
    }
    float energy = 0.0f;
    float distance = 0.0f;
    for (const SoundRayResult& r : Results) {
        energy += r.Energy;
        distance += r.Distance;
    }
    energy /= static_cast<float>(Results.size());
    distance /= static_cast<float>(Results.size());
    p.DecayTime = std::clamp(0.3f + 2.8f * energy, 0.2f, 5.0f);
    p.EarlyGain = std::clamp(0.2f + 0.7f * energy, 0.0f, 1.0f);
    p.LateReverbGain = std::clamp(0.1f + 0.6f * energy, 0.0f, 1.0f);
    p.LateDelay = std::clamp(distance / 340.0f, 0.01f, 0.25f);
    return p;
}

} // namespace Solstice::Core::Audio
