#pragma once

#include "Keyframe.hxx"
#include "Skeleton.hxx"
#include <string>
#include <vector>
#include <regex>

namespace Solstice::Arzachel {

// Bone pattern for matching bones by name
// Supports simple wildcards: "LeftArm.*", "Spine[0-9]+", etc.
class BonePattern {
public:
    BonePattern() : PatternStr(".*"), PatternRegex(".*") {}
    explicit BonePattern(const std::string& Pattern) : PatternStr(Pattern) {
        // Convert simple patterns to regex
        // "LeftArm.*" -> "LeftArm.*" (regex)
        // For now, simple string matching - can be enhanced with full regex
        PatternRegex = std::regex(PatternStr);
    }

    bool Matches(const std::string& BoneName) const {
        return std::regex_match(BoneName, PatternRegex);
    }

    const std::string& GetPattern() const { return PatternStr; }

private:
    std::string PatternStr;
    std::regex PatternRegex;
};

// Animation track targeting bones via pattern
struct AnimationTrack {
    BonePattern TargetPattern;
    KeyframeTrack<Math::Vec3> Translation;
    KeyframeTrack<Math::Quaternion> Rotation;
    KeyframeTrack<Math::Vec3> Scale;

    AnimationTrack() = default;
    AnimationTrack(const BonePattern& Pattern)
        : TargetPattern(Pattern) {}

    // Resolve pattern to actual bone IDs in skeleton
    std::vector<BoneID> ResolveBones(const Skeleton& SkeletonParam) const {
        std::vector<BoneID> MatchedBones;
        for (const auto& BoneObj : SkeletonParam.GetBones()) {
            if (TargetPattern.Matches(BoneObj.Name)) {
                MatchedBones.push_back(BoneObj.ID);
            }
        }
        return MatchedBones;
    }
};

} // namespace Solstice::Arzachel
