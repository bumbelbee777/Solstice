#pragma once

#include "SoundRay.hxx"
#include <unordered_map>
#include <vector>

namespace Solstice::Core::Audio {

struct AudioSource;
class AudioManager;
class PortalAudio;
class FluidAudioCoupler;

class WaveTracer {
public:
    WaveTracer() = default;
    ~WaveTracer() = default;

    void Initialize(const void* Scene);
    void Shutdown();

    void TraceCPU(const std::vector<AudioSource>& Sources, const Math::Vec3& ListenerPos, std::vector<SoundRayResult>& OutResults);
    void TraceGPU(const std::vector<AudioSource>& Sources, const Math::Vec3& ListenerPos, std::vector<SoundRayResult>& OutResults);
    void Trace(const std::vector<AudioSource>& Sources, const Math::Vec3& ListenerPos, std::vector<SoundRayResult>& OutResults);
    void ApplyResults(const std::vector<SoundRayResult>& Results, AudioManager& Audio);

    WaveTraceReverbParams ComputeReverb(const std::vector<SoundRayResult>& Results) const;

    void SetMaxRays(int PerSource) { m_MaxRays = std::max(1, PerSource); }
    void SetMaxBounces(int Bounces) { m_MaxBounces = std::max(0, Bounces); }
    void SetEnableCache(bool Enable) { m_EnableCache = Enable; }
    void SetPortalAudio(PortalAudio* Portal) { m_Portal = Portal; }
    void SetFluidCoupler(FluidAudioCoupler* Fluid) { m_Fluid = Fluid; }

private:
    struct CachedRay {
        Math::Vec3 LastOrigin{0.0f, 0.0f, 0.0f};
        Math::Vec3 LastDirection{0.0f, 0.0f, 0.0f};
        float LastEnergy{0.0f};
        float LastDistance{0.0f};
        int ValidFrames{0};
    };

    const void* m_CurrentScene{nullptr};
    std::unordered_map<uint64_t, CachedRay> m_RayCache;
    int m_MaxRays{128};
    int m_MaxBounces{3};
    bool m_EnableCache{true};
    PortalAudio* m_Portal{nullptr};
    FluidAudioCoupler* m_Fluid{nullptr};

    int GetRayCountForSource(const AudioSource& Source, float Distance) const;
};

} // namespace Solstice::Core::Audio
