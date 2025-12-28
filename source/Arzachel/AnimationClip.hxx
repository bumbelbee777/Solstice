#pragma once

#include "AnimationTrack.hxx"
#include "Skeleton.hxx"
#include <vector>
#include <map>
#include <cmath>

namespace Solstice::Arzachel {

// Bone transform at a point in time
struct BoneTransform {
    Math::Vec3 Translation;
    Math::Quaternion Rotation;
    Math::Vec3 Scale;

    BoneTransform()
        : Translation(0, 0, 0)
        , Rotation(1, 0, 0, 0)
        , Scale(1, 1, 1) {}

    BoneTransform(const Math::Vec3& T, const Math::Quaternion& R, const Math::Vec3& S)
        : Translation(T), Rotation(R), Scale(S) {}

    Math::Matrix4 ToMatrix() const {
        Math::Matrix4 TMat = Math::Matrix4::Translation(Translation);
        Math::Matrix4 RMat = Rotation.ToMatrix();
        Math::Matrix4 SMat = Math::Matrix4::Scale(Scale);
        return TMat * RMat * SMat;
    }
};

// Pose - bone transforms at a point in time
class Pose {
public:
    Pose() = default;

    // Get transform for a bone (returns identity if not found)
    BoneTransform GetTransform(BoneID ID) const {
        auto It = BoneTransformsMap.find(ID);
        if (It != BoneTransformsMap.end()) {
            return It->second;
        }
        return BoneTransform{}; // Identity
    }

    // Get world transform for a bone (accumulates parent transforms)
    Math::Matrix4 GetWorldTransform(BoneID ID, const Skeleton& SkeletonParam) const {
        Math::Matrix4 Result = Math::Matrix4::Identity();

        BoneID CurrentID = ID;
        while (CurrentID.IsValid()) {
            const Bone* BonePtr = SkeletonParam.FindBone(CurrentID);
            if (!BonePtr) break;

            BoneTransform TransformObj = GetTransform(CurrentID);
            Math::Matrix4 LocalMatrix = TransformObj.ToMatrix();
            Result = LocalMatrix * Result;

            CurrentID = BonePtr->Parent;
        }

        return Result;
    }

    // Set transform for a bone
    void SetTransform(BoneID ID, const BoneTransform& TransformObj) {
        BoneTransformsMap[ID] = TransformObj;
    }

    const std::map<BoneID, BoneTransform>& GetBoneTransforms() const { return BoneTransformsMap; }

private:
    std::map<BoneID, BoneTransform> BoneTransformsMap;
};

// Immutable animation clip
class AnimationClip {
public:
    AnimationClip() : Duration(0.0f) {}

    explicit AnimationClip(const std::vector<AnimationTrack>& TracksParam)
        : Tracks(TracksParam) {
        CalculateDuration();
    }

    // Evaluate clip at a time point to produce a pose
    Pose Evaluate(float Time, const Skeleton& SkeletonParam) const {
        Pose Result;

        // Clamp time to duration
        if (Duration > 0.0f) {
            Time = std::fmod(Time, Duration);
            if (Time < 0.0f) Time += Duration;
        }

        // Evaluate each track
        for (const auto& TrackObj : Tracks) {
            std::vector<BoneID> MatchedBones = TrackObj.ResolveBones(SkeletonParam);

            for (BoneID BoneIDVal : MatchedBones) {
                BoneTransform TransformObj = Result.GetTransform(BoneIDVal);

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

    const std::vector<AnimationTrack>& GetTracks() const { return Tracks; }
    float GetDuration() const { return Duration; }

    void AddTrack(const AnimationTrack& TrackParam) {
        Tracks.push_back(TrackParam);
        CalculateDuration();
    }

private:
    void CalculateDuration() {
        Duration = 0.0f;
        for (const auto& TrackObj : Tracks) {
            float TrackDuration = std::max({
                TrackObj.Translation.GetDuration(),
                TrackObj.Rotation.GetDuration(),
                TrackObj.Scale.GetDuration()
            });
            if (TrackDuration > Duration) {
                Duration = TrackDuration;
            }
        }
    }

    std::vector<AnimationTrack> Tracks;
    float Duration;
};

} // namespace Solstice::Arzachel
