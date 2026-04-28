#include "Match/MatchGameplayAuthoringCache.hxx"

#include <utility>

namespace Solstice::Game {

MatchGameplayAuthoringCache& MatchGameplayAuthoringCache::Instance() {
    static MatchGameplayAuthoringCache s_Instance;
    return s_Instance;
}

void MatchGameplayAuthoringCache::Clear() {
    m_Capture.clear();
    m_Occluders.clear();
    m_Recon.clear();
    m_Warp.clear();
}

void MatchGameplayAuthoringCache::SetAuthoring(
    std::vector<CaptureVolumeRuntime> capture,
    std::vector<CelestialOccluderRuntime> occluders,
    std::vector<ReconAnchorRuntime> recon,
    std::vector<WarpLaneRuntime> warp) {
    m_Capture = std::move(capture);
    m_Occluders = std::move(occluders);
    m_Recon = std::move(recon);
    m_Warp = std::move(warp);
}

} // namespace Solstice::Game
