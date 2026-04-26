#pragma once

#include "Seed.hxx"
#include <Skeleton/Skeleton.hxx>
#include <Math/Vector.hxx>
#include <Math/Quaternion.hxx>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Solstice::Arzachel {

/// Logical morph target id (e.g. hash of shape name); render path may map later.
using MorphTargetId = uint32_t;

MorphTargetId MorphNameHash(std::string_view name);

enum class ExpressionPattern : uint8_t {
    Static = 0,
    Pulse = 1,
    Blink = 2,
    Jitter = 3,
    Sequence = 4,
};

/// Named facial pose offsets relative to bind/neutral (bones + optional morph weights).
struct Expression {
    std::string Name;
    std::unordered_map<std::string, Skeleton::BoneTransform> BoneOffsets;
    std::unordered_map<std::string, float> MorphWeights;
    Seed VariationSeed{};
    ExpressionPattern Pattern{ExpressionPattern::Static};
    /// Seconds (pulse period, blink cadence hint, etc.); ignored for Static.
    float PatternPeriodSec{1.f};
};

/// Mouth shape for a phoneme / viseme id (Oculus-style ids: sil, PP, aa, …).
struct Viseme {
    std::string Id;
    std::unordered_map<std::string, Skeleton::BoneTransform> BoneOffsets;
    std::unordered_map<std::string, float> MorphWeights;
};

class VisemeSet {
public:
    static VisemeSet Standard();

    [[nodiscard]] bool TryGet(std::string_view id, Viseme& out) const;
    [[nodiscard]] Viseme Blend(const Viseme& a, const Viseme& b, float t) const;

private:
    std::unordered_map<std::string, Viseme> m_Visemes;
};

/// Fluent builder for `Expression`.
class ExpressionBuilder {
public:
    explicit ExpressionBuilder(std::string name);

    ExpressionBuilder& Bone(std::string name, const Math::Vec3& t, const Math::Quaternion& r,
        const Math::Vec3& s = Math::Vec3(1.f, 1.f, 1.f));
    ExpressionBuilder& Morph(std::string name, float w);
    ExpressionBuilder& Pattern(ExpressionPattern p, float periodSec = 1.f);
    ExpressionBuilder& VariationSeed(Seed s);

    [[nodiscard]] Expression Build() const;

private:
    Expression m_Expr;
};

/// Runtime stack: ordered mood expressions + viseme overlay (last wins for conflicting morphs).
struct ExpressionStack {
    struct Layer {
        const Expression* Expr{nullptr};
        float Weight{0.f};
    };
    std::vector<Layer> Expressions;
    std::string VisemeId;
    float VisemeStrength{0.f};
};

float PatternWeight(ExpressionPattern pattern, float patternPeriodSec, float timeSec, Seed variationSeed);

/// Stateless blink amount in [0,1] (1 = fully closed). Cheap irregular cadence from seed + time.
float BlinkClosedAmount(float timeSec, Seed blinkSeed);

/// Eye look jitter in normalized screen-ish units (small).
Math::Vec2 SaccadeOffset(float timeSec, Seed saccadeSeed);

void ApplyBoneOffsetsToPose(const Skeleton::Skeleton& sk, Skeleton::Pose& ioPose,
    const std::unordered_map<std::string, Skeleton::BoneTransform>& offsets, float weight);

void ApplyMorphOffsets(std::unordered_map<MorphTargetId, float>& ioMorphs,
    const std::unordered_map<std::string, float>& offsets, float weight);

/// Accumulates facial layers into named bone offsets + morph weights (no skeleton required — used by Parallax eval).
void EvaluateFacialAtTimeToMaps(std::unordered_map<std::string, Skeleton::BoneTransform>& outBoneDeltas,
    std::unordered_map<MorphTargetId, float>& outMorphs, const ExpressionStack& stack, const VisemeSet& visemes,
    float timeSec, Seed facialSeed, bool enableBlink, bool enableSaccade);

void ApplyNamedBoneDeltasToPose(const Skeleton::Skeleton& sk, Skeleton::Pose& ioPose,
    const std::unordered_map<std::string, Skeleton::BoneTransform>& namedDeltas);

/// Evaluates stack + standard viseme set; merges onto `ioPose` (typically copy of clip/base pose first).
void EvaluateFacialAtTime(const Skeleton::Skeleton& sk, Skeleton::Pose& ioPose,
    std::unordered_map<MorphTargetId, float>& ioMorphs, const ExpressionStack& stack, const VisemeSet& visemes,
    float timeSec, Seed facialSeed, bool enableBlink, bool enableSaccade);

/// Looks up built-in mood expression by name ("happy", "sad", …). Returns null if unknown.
const Expression* BuiltinExpressionByName(std::string_view name);

/// Stub for future audio-driven phonemes; returns empty.
class PhonemeDetector {
public:
    std::vector<std::pair<std::string, float>> DetectFromAudio(const void* /*audio*/) { return {}; }
    std::vector<std::pair<std::string, float>> DetectFromText(const std::string& text);
};

} // namespace Solstice::Arzachel
