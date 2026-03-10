#pragma once

#include "AnimationTrack.hxx"
#include <Skeleton/Skeleton.hxx>
#include <algorithm>
#include <vector>
#include <cmath>

namespace Solstice::Arzachel {

// Immutable animation clip (evaluates to Skeleton::Pose)
class AnimationClip {
public:
    AnimationClip() : m_Duration(0.0f) {}

    explicit AnimationClip(const std::vector<AnimationTrack>& TracksParam)
        : m_Tracks(TracksParam) {
        CalculateDuration();
    }

    // Evaluate clip at a time point to produce a pose
    [[nodiscard]] ::Solstice::Skeleton::Pose Evaluate(float Time, const ::Solstice::Skeleton::Skeleton& SkeletonParam) const {
        ::Solstice::Skeleton::Pose Result;

        // Clamp time to duration
        if (m_Duration > 0.0f) {
            Time = std::fmod(Time, m_Duration);
            if (Time < 0.0f) { Time += m_Duration; }
        }

        // Evaluate each track
        for (const auto& TrackObj : m_Tracks) {
            std::vector<::Solstice::Skeleton::BoneID> MatchedBones = TrackObj.ResolveBones(SkeletonParam);

            for (::Solstice::Skeleton::BoneID BoneIDVal : MatchedBones) {
                ::Solstice::Skeleton::BoneTransform TransformObj = Result.GetTransform(BoneIDVal);

                // Evaluate translation
                if (!TrackObj.Translation.GetKeyframes().empty()) {
                    TransformObj.Translation = TrackObj.Translation.Evaluate(Time);
                }

                // Evaluate rotation
                if (!TrackObj.Rotation.GetKeyframes().empty()) {
                    TransformObj.Rotation = TrackObj.Rotation.Evaluate(Time);
                }

                // Evaluate scale
                if (!TrackObj.Scale.GetKeyframes().empty()) {
                    TransformObj.Scale = TrackObj.Scale.Evaluate(Time);
                }

                Result.SetTransform(BoneIDVal, TransformObj);
            }
        }

        return Result;
    }

    [[nodiscard]] const std::vector<AnimationTrack>& GetTracks() const { return m_Tracks; }
    [[nodiscard]] float GetDuration() const { return m_Duration; }

    void AddTrack(const AnimationTrack& TrackParam) {
        m_Tracks.push_back(TrackParam);
        CalculateDuration();
    }

private:
    void CalculateDuration() {
        m_Duration = 0.0f;
        for (const auto& TrackObj : m_Tracks) {
            float TrackDuration = std::max({
                TrackObj.Translation.GetDuration(),
                TrackObj.Rotation.GetDuration(),
                TrackObj.Scale.GetDuration()
            });
            m_Duration = std::max(TrackDuration, m_Duration);
        }
    }

    std::vector<AnimationTrack> m_Tracks;
    float m_Duration;
};

} // namespace Solstice::Arzachel
