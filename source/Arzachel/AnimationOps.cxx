#include "AnimationOps.hxx"
#include <algorithm>

namespace Solstice::Arzachel {

AnimationClip Add(const AnimationClip& A, const AnimationClip& B) {
    // Additive blending: combine transforms additively
    // This is a simplified implementation - proper additive blending requires
    // evaluating both clips and adding transforms
    // For now, merge tracks (proper additive would need pose evaluation)
    return MergeTracks(A, B);
}

AnimationClip Layer(const AnimationClip& Base, const AnimationClip& Overlay, float Weight) {
    // Weighted layer: blend Base and Overlay with weight
    // Simplified - proper implementation would evaluate both and blend
    if (Weight <= 0.0f) return Base;
    if (Weight >= 1.0f) return Overlay;

    // For now, merge tracks (proper blending requires pose evaluation)
    return MergeTracks(Base, Overlay);
}

AnimationClip MergeTracks(const AnimationClip& A, const AnimationClip& B) {
    // Combine tracks - later tracks (B) override earlier ones (A) for same bones
    std::vector<AnimationTrack> MergedTracks = A.GetTracks();

    // Add tracks from B, but skip if pattern already exists in A
    for (const auto& TrackB : B.GetTracks()) {
        bool Found = false;
        for (const auto& TrackA : MergedTracks) {
            if (TrackA.TargetPattern.GetPattern() == TrackB.TargetPattern.GetPattern()) {
                Found = true;
                break;
            }
        }
        if (!Found) {
            MergedTracks.push_back(TrackB);
        }
    }

    return AnimationClip(MergedTracks);
}

} // namespace Solstice::Arzachel
