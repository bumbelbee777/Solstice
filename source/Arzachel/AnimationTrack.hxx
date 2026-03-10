#pragma once

#include <MinGfx/Keyframe.hxx>
#include <Skeleton/Skeleton.hxx>
#include <string>
#include <utility>
#include <vector>
#include <regex>

namespace Solstice::Arzachel {

// Bone pattern for matching bones by name
// Supports simple wildcards: "LeftArm.*", "Spine[0-9]+", etc.
class BonePattern {
public:
    BonePattern() : m_PatternStr(".*"), m_PatternRegex(".*") {}
    explicit BonePattern(std::string  Pattern) : m_PatternStr(std::move(Pattern)) {
        // Convert simple patterns to regex
        // "LeftArm.*" -> "LeftArm.*" (regex)
        // For now, simple string matching - can be enhanced with full regex
        m_PatternRegex = std::regex(m_PatternStr);
    }

    [[nodiscard]] bool Matches(const std::string& BoneName) const {
        return std::regex_match(BoneName, m_PatternRegex);
    }

    [[nodiscard]] const std::string& GetPattern() const { return m_PatternStr; }

private:
    std::string m_PatternStr;
    std::regex m_PatternRegex;
};

// Animation track targeting bones via pattern (uses MinGfx keyframe tracks)
struct AnimationTrack {
    BonePattern TargetPattern;
    ::Solstice::MinGfx::KeyframeTrack<Math::Vec3> Translation;
    ::Solstice::MinGfx::KeyframeTrack<Math::Quaternion> Rotation;
    ::Solstice::MinGfx::KeyframeTrack<Math::Vec3> Scale;

    AnimationTrack() = default;
    AnimationTrack(BonePattern  Pattern)
        : TargetPattern(std::move(Pattern)) {}

    // Resolve pattern to actual bone IDs in skeleton
    [[nodiscard]] std::vector<::Solstice::Skeleton::BoneID> ResolveBones(const ::Solstice::Skeleton::Skeleton& SkeletonParam) const {
        std::vector<::Solstice::Skeleton::BoneID> MatchedBones;
        for (const auto& BoneObj : SkeletonParam.GetBones()) {
            if (TargetPattern.Matches(BoneObj.Name)) {
                MatchedBones.push_back(BoneObj.ID);
            }
        }
        return MatchedBones;
    }
};

} // namespace Solstice::Arzachel
