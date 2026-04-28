#pragma once

#include "../../Solstice.hxx"

#include "../../Math/Vector.hxx"

#include <string>
#include <vector>

namespace Solstice::Game {

/// Runtime match/space-sim gameplay authoring (populated by the **game**; not part of `LibSmf` / SMAL).
struct CaptureVolumeRuntime {
    std::string Name;
    std::string ZoneId;
    Math::Vec3 BoundsMin{};
    Math::Vec3 BoundsMax{};
    bool Enabled{true};
    float TimeToCaptureSec{10.0f};
};

struct CelestialOccluderRuntime {
    std::string Name;
    Math::Vec3 Center{};
    float Radius{1000.0f};
    bool BlocksLineOfSight{true};
};

struct ReconAnchorRuntime {
    std::string Name;
    Math::Vec3 Position{};
    float SensorRadius{500.0f};
};

struct WarpLaneRuntime {
    std::string Name;
    Math::Vec3 Start{};
    Math::Vec3 End{};
    float MaxCFraction{10.0f};
    float ChargeTimeSec{5.0f};
};

class SOLSTICE_API MatchGameplayAuthoringCache {
public:
    static MatchGameplayAuthoringCache& Instance();

    void Clear();

    void SetAuthoring(
        std::vector<CaptureVolumeRuntime> capture,
        std::vector<CelestialOccluderRuntime> occluders,
        std::vector<ReconAnchorRuntime> recon,
        std::vector<WarpLaneRuntime> warp);

    const std::vector<CaptureVolumeRuntime>& CaptureVolumes() const { return m_Capture; }
    const std::vector<CelestialOccluderRuntime>& CelestialOccluders() const { return m_Occluders; }
    const std::vector<ReconAnchorRuntime>& ReconAnchors() const { return m_Recon; }
    const std::vector<WarpLaneRuntime>& WarpLanes() const { return m_Warp; }

private:
    MatchGameplayAuthoringCache() = default;

    std::vector<CaptureVolumeRuntime> m_Capture;
    std::vector<CelestialOccluderRuntime> m_Occluders;
    std::vector<ReconAnchorRuntime> m_Recon;
    std::vector<WarpLaneRuntime> m_Warp;
};

} // namespace Solstice::Game
