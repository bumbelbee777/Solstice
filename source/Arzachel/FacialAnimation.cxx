#include "FacialAnimation.hxx"
#include "TextLipSync.hxx"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>

namespace Solstice::Arzachel {

namespace Math = Solstice::Math;

MorphTargetId MorphNameHash(std::string_view name) {
    uint64_t h = 1469598103934665603ull;
    for (unsigned char c : name) {
        h ^= static_cast<uint64_t>(c);
        h *= 1099511628211ull;
    }
    return static_cast<MorphTargetId>(static_cast<uint32_t>(h ^ (h >> 32)));
}

static Skeleton::BoneTransform MakeBT(const Math::Vec3& t, const Math::Quaternion& r, const Math::Vec3& s) {
    Skeleton::BoneTransform bt;
    bt.Translation = t;
    bt.Rotation = r;
    bt.Scale = s;
    return bt;
}

static Viseme MakeViseme(std::string id, float jawY, float mouthWide = 0.f) {
    Viseme v;
    v.Id = std::move(id);
    v.BoneOffsets["jaw"] = MakeBT(Math::Vec3(0, jawY, 0), Math::Quaternion(), Math::Vec3(1, 1, 1));
    if (mouthWide != 0.f) {
        v.BoneOffsets["lip_corner_L"] =
            MakeBT(Math::Vec3(0, 0, mouthWide), Math::Quaternion::Lerp(Math::Quaternion(), Math::Quaternion(0.98f, 0, 0, 0.15f), 0.3f),
                Math::Vec3(1, 1, 1));
        v.BoneOffsets["lip_corner_R"] =
            MakeBT(Math::Vec3(0, 0, -mouthWide), Math::Quaternion::Lerp(Math::Quaternion(), Math::Quaternion(0.98f, 0, 0, -0.15f), 0.3f),
                Math::Vec3(1, 1, 1));
    }
    v.MorphWeights["jaw_open"] = std::clamp(-jawY * 25.f, 0.f, 1.f);
    return v;
}

VisemeSet VisemeSet::Standard() {
    VisemeSet set;
    auto& m = set.m_Visemes;
    m["sil"] = MakeViseme("sil", 0.02f);
    m["PP"] = MakeViseme("PP", 0.0f, 0.01f);
    m["FF"] = MakeViseme("FF", 0.015f, 0.02f);
    m["TH"] = MakeViseme("TH", 0.04f, 0.01f);
    m["DD"] = MakeViseme("DD", 0.05f, 0.015f);
    m["kk"] = MakeViseme("kk", 0.03f, 0.01f);
    m["CH"] = MakeViseme("CH", 0.045f, 0.025f);
    m["SS"] = MakeViseme("SS", 0.025f, 0.03f);
    m["nn"] = MakeViseme("nn", 0.035f, 0.005f);
    m["RR"] = MakeViseme("RR", 0.04f, 0.02f);
    m["aa"] = MakeViseme("aa", -0.06f, 0.02f);
    m["E"] = MakeViseme("E", -0.04f, 0.025f);
    m["I"] = MakeViseme("I", -0.03f, 0.03f);
    m["O"] = MakeViseme("O", -0.05f, 0.015f);
    m["U"] = MakeViseme("U", -0.035f, 0.01f);
    return set;
}

bool VisemeSet::TryGet(std::string_view id, Viseme& out) const {
    std::string key(id);
    auto it = m_Visemes.find(key);
    if (it == m_Visemes.end()) {
        return false;
    }
    out = it->second;
    return true;
}

Viseme VisemeSet::Blend(const Viseme& a, const Viseme& b, float t) const {
    t = std::clamp(t, 0.f, 1.f);
    Viseme r;
    r.Id = (t < 0.5f) ? a.Id : b.Id;
    auto blendBT = [&](const Skeleton::BoneTransform& x, const Skeleton::BoneTransform& y) {
        Skeleton::BoneTransform z;
        z.Translation = x.Translation * (1.f - t) + y.Translation * t;
        z.Rotation = Math::Quaternion::Slerp(x.Rotation, y.Rotation, t);
        z.Scale = x.Scale * (1.f - t) + y.Scale * t;
        return z;
    };
    for (const auto& kv : a.BoneOffsets) {
        auto it = b.BoneOffsets.find(kv.first);
        if (it != b.BoneOffsets.end()) {
            r.BoneOffsets[kv.first] = blendBT(kv.second, it->second);
        } else {
            r.BoneOffsets[kv.first] = kv.second;
            r.BoneOffsets[kv.first].Translation *= (1.f - t);
        }
    }
    for (const auto& kv : b.BoneOffsets) {
        if (r.BoneOffsets.find(kv.first) == r.BoneOffsets.end()) {
            Skeleton::BoneTransform bt = kv.second;
            bt.Translation *= t;
            r.BoneOffsets[kv.first] = bt;
        }
    }
    for (const auto& kv : a.MorphWeights) {
        float y = 0.f;
        auto it = b.MorphWeights.find(kv.first);
        if (it != b.MorphWeights.end()) {
            y = it->second;
        }
        r.MorphWeights[kv.first] = kv.second * (1.f - t) + y * t;
    }
    for (const auto& kv : b.MorphWeights) {
        if (r.MorphWeights.find(kv.first) == r.MorphWeights.end()) {
            r.MorphWeights[kv.first] = kv.second * t;
        }
    }
    return r;
}

ExpressionBuilder::ExpressionBuilder(std::string name) {
    m_Expr.Name = std::move(name);
}

ExpressionBuilder& ExpressionBuilder::Bone(std::string name, const Math::Vec3& t, const Math::Quaternion& r, const Math::Vec3& s) {
    m_Expr.BoneOffsets[std::move(name)] = MakeBT(t, r, s);
    return *this;
}

ExpressionBuilder& ExpressionBuilder::Morph(std::string name, float w) {
    m_Expr.MorphWeights[std::move(name)] = w;
    return *this;
}

ExpressionBuilder& ExpressionBuilder::Pattern(ExpressionPattern p, float periodSec) {
    m_Expr.Pattern = p;
    m_Expr.PatternPeriodSec = periodSec;
    return *this;
}

ExpressionBuilder& ExpressionBuilder::VariationSeed(Seed s) {
    m_Expr.VariationSeed = s;
    return *this;
}

Expression ExpressionBuilder::Build() const {
    return m_Expr;
}

float PatternWeight(ExpressionPattern pattern, float patternPeriodSec, float timeSec, Seed variationSeed) {
    constexpr float kPi = 3.14159265f;
    switch (pattern) {
    case ExpressionPattern::Static:
        return 1.f;
    case ExpressionPattern::Pulse: {
        const float period = std::max(0.05f, patternPeriodSec);
        const float ph = NextFloat(variationSeed.Derive(0xF00D)) * 2.f * kPi;
        return 0.82f + 0.18f * std::sin(timeSec * (2.f * kPi / period) + ph);
    }
    case ExpressionPattern::Blink:
        return 1.f;
    case ExpressionPattern::Jitter: {
        const uint64_t step = static_cast<uint64_t>(timeSec * 60.f);
        const float j = NextFloat(variationSeed.Derive(step ^ 0xBADC0DEu)) - 0.5f;
        return std::clamp(1.f + 0.04f * j, 0.85f, 1.15f);
    }
    case ExpressionPattern::Sequence:
        return 1.f;
    }
    return 1.f;
}

float BlinkClosedAmount(float timeSec, Seed blinkSeed) {
    constexpr float slice = 0.28f;
    constexpr float blinkDur = 0.11f;
    const int64_t i = static_cast<int64_t>(std::floor(timeSec / slice));
    const float local = timeSec - static_cast<float>(i) * slice;
    const Seed si = blinkSeed.Derive(static_cast<uint64_t>(i) ^ 0xB1EEu);
    float closed = 0.f;
    if (NextFloat(si) > 0.86f) {
        const float start = NextFloat(si.Derive(1)) * std::max(0.f, slice - blinkDur);
        if (local >= start && local < start + blinkDur) {
            const float u = (local - start) / blinkDur;
            closed = std::sin(u * 3.14159265f);
        }
    }
    return std::clamp(closed, 0.f, 1.f);
}

Math::Vec2 SaccadeOffset(float timeSec, Seed saccadeSeed) {
    const uint64_t step = static_cast<uint64_t>(timeSec * 12.f);
    const Seed s0 = saccadeSeed.Derive(step);
    const float ax = NextFloat(s0) * 2.f * 3.14159265f;
    const float ay = NextFloat(s0.Derive(1)) * 2.f * 3.14159265f;
    const float slow = timeSec * 0.7f;
    return Math::Vec2(std::sin(slow + ax) * 0.35f, std::cos(slow * 0.83f + ay) * 0.35f);
}

static void AccumulateBoneOffsetMap(std::unordered_map<std::string, Skeleton::BoneTransform>& m,
    const std::unordered_map<std::string, Skeleton::BoneTransform>& offsets, float weight) {
    weight = std::clamp(weight, 0.f, 1.f);
    if (weight <= 1e-5f) {
        return;
    }
    for (const auto& kv : offsets) {
        const Skeleton::BoneTransform& off = kv.second;
        auto it = m.find(kv.first);
        if (it == m.end()) {
            Skeleton::BoneTransform cur{};
            cur.Translation = off.Translation * weight;
            cur.Rotation = Math::Quaternion::Slerp(Math::Quaternion(), off.Rotation.Normalized(), weight);
            cur.Scale = Math::Vec3(std::pow(off.Scale.x, weight), std::pow(off.Scale.y, weight), std::pow(off.Scale.z, weight));
            m.emplace(kv.first, cur);
        } else {
            Skeleton::BoneTransform& cur = it->second;
            cur.Translation += off.Translation * weight;
            const Math::Quaternion qOff = Math::Quaternion::Slerp(Math::Quaternion(), off.Rotation.Normalized(), weight);
            cur.Rotation = (cur.Rotation * qOff).Normalized();
            cur.Scale = Math::Vec3(cur.Scale.x * std::pow(off.Scale.x, weight), cur.Scale.y * std::pow(off.Scale.y, weight),
                cur.Scale.z * std::pow(off.Scale.z, weight));
        }
    }
}

void ApplyBoneOffsetsToPose(const Skeleton::Skeleton& sk, Skeleton::Pose& ioPose,
    const std::unordered_map<std::string, Skeleton::BoneTransform>& offsets, float weight) {
    weight = std::clamp(weight, 0.f, 1.f);
    if (weight <= 1e-5f) {
        return;
    }
    for (const auto& kv : offsets) {
        const Skeleton::Bone* b = sk.FindBoneByName(kv.first);
        if (!b) {
            continue;
        }
        const Skeleton::BoneTransform& off = kv.second;
        Skeleton::BoneTransform cur = ioPose.GetTransform(b->ID);
        cur.Translation = cur.Translation + off.Translation * weight;
        const Math::Quaternion qOff = Math::Quaternion::Slerp(Math::Quaternion(), off.Rotation.Normalized(), weight);
        cur.Rotation = (cur.Rotation * qOff).Normalized();
        cur.Scale = Math::Vec3(cur.Scale.x * std::pow(off.Scale.x, weight), cur.Scale.y * std::pow(off.Scale.y, weight),
            cur.Scale.z * std::pow(off.Scale.z, weight));
        ioPose.SetTransform(b->ID, cur);
    }
}

void ApplyMorphOffsets(std::unordered_map<MorphTargetId, float>& ioMorphs,
    const std::unordered_map<std::string, float>& offsets, float weight) {
    weight = std::clamp(weight, 0.f, 1.f);
    if (weight <= 1e-5f) {
        return;
    }
    for (const auto& kv : offsets) {
        const MorphTargetId id = MorphNameHash(kv.first);
        ioMorphs[id] += kv.second * weight;
    }
}

static const std::unordered_map<std::string, Expression>& BuiltinExpressionTable() {
    static const std::unordered_map<std::string, Expression> kTable = [] {
        std::unordered_map<std::string, Expression> t;
        t["happy"] = ExpressionBuilder("happy")
                         .Bone("jaw", Math::Vec3(0, -0.02f, 0), Math::Quaternion())
                         .Bone("lip_corner_L", Math::Vec3(0, 0.01f, 0.02f),
                             Math::Quaternion::Lerp(Math::Quaternion(), Math::Quaternion(0.97f, 0, 0, 0.18f), 0.35f))
                         .Bone("lip_corner_R", Math::Vec3(0, 0.01f, -0.02f),
                             Math::Quaternion::Lerp(Math::Quaternion(), Math::Quaternion(0.97f, 0, 0, -0.18f), 0.35f))
                         .Bone("cheek_L", Math::Vec3(0.01f, 0.01f, 0), Math::Quaternion())
                         .Bone("cheek_R", Math::Vec3(-0.01f, 0.01f, 0), Math::Quaternion())
                         .Morph("mouth_corner_up", 0.7f)
                         .Morph("cheek_raise", 0.5f)
                         .Morph("eye_squint", 0.3f)
                         .Pattern(ExpressionPattern::Pulse, 2.0f)
                         .Build();
        t["sad"] = ExpressionBuilder("sad")
                       .Bone("jaw", Math::Vec3(0, 0.01f, 0), Math::Quaternion())
                       .Bone("lip_corner_L", Math::Vec3(0, -0.01f, 0.01f),
                           Math::Quaternion::Lerp(Math::Quaternion(), Math::Quaternion(0.99f, 0, 0, -0.08f), 0.2f))
                       .Bone("lip_corner_R", Math::Vec3(0, -0.01f, -0.01f),
                           Math::Quaternion::Lerp(Math::Quaternion(), Math::Quaternion(0.99f, 0, 0, 0.08f), 0.2f))
                       .Bone("brow_inner_L", Math::Vec3(0, 0.005f, 0), Math::Quaternion())
                       .Bone("brow_inner_R", Math::Vec3(0, 0.005f, 0), Math::Quaternion())
                       .Morph("brow_lower", 0.6f)
                       .Morph("lip_corner_down", 0.8f)
                       .Pattern(ExpressionPattern::Static)
                       .Build();
        t["angry"] = ExpressionBuilder("angry")
                         .Bone("jaw", Math::Vec3(0, -0.01f, 0), Math::Quaternion())
                         .Bone("brow_inner_L", Math::Vec3(-0.005f, -0.005f, 0),
                             Math::Quaternion::Lerp(Math::Quaternion(), Math::Quaternion(0.96f, 0, 0, -0.22f), 0.45f))
                         .Bone("brow_inner_R", Math::Vec3(0.005f, -0.005f, 0),
                             Math::Quaternion::Lerp(Math::Quaternion(), Math::Quaternion(0.96f, 0, 0, 0.22f), 0.45f))
                         .Bone("nose", Math::Vec3(0, 0, 0.005f), Math::Quaternion())
                         .Morph("nose_wrinkle", 0.7f)
                         .Morph("brow_furrow", 0.9f)
                         .Morph("lip_press", 0.5f)
                         .Pattern(ExpressionPattern::Pulse, 0.5f)
                         .Build();
        t["surprise"] = ExpressionBuilder("surprise")
                            .Bone("jaw", Math::Vec3(0, -0.03f, 0), Math::Quaternion())
                            .Bone("brow_outer_L", Math::Vec3(0, 0.01f, 0),
                                Math::Quaternion::Lerp(Math::Quaternion(), Math::Quaternion(0.95f, 0, 0, 0.25f), 0.5f))
                            .Bone("brow_outer_R", Math::Vec3(0, 0.01f, 0),
                                Math::Quaternion::Lerp(Math::Quaternion(), Math::Quaternion(0.95f, 0, 0, -0.25f), 0.5f))
                            .Morph("eye_widen", 0.8f)
                            .Morph("mouth_open", 0.6f)
                            .Pattern(ExpressionPattern::Static)
                            .Build();
        t["neutral"] = ExpressionBuilder("neutral").Pattern(ExpressionPattern::Static).Build();
        return t;
    }();
    return kTable;
}

const Expression* BuiltinExpressionByName(std::string_view name) {
    std::string key(name);
    const auto& tab = BuiltinExpressionTable();
    auto it = tab.find(key);
    if (it == tab.end()) {
        return nullptr;
    }
    return &it->second;
}

void EvaluateFacialAtTimeToMaps(std::unordered_map<std::string, Skeleton::BoneTransform>& outBoneDeltas,
    std::unordered_map<MorphTargetId, float>& outMorphs, const ExpressionStack& stack, const VisemeSet& visemes,
    float timeSec, Seed facialSeed, bool enableBlink, bool enableSaccade) {
    outBoneDeltas.clear();
    outMorphs.clear();
    for (const auto& layer : stack.Expressions) {
        if (!layer.Expr || layer.Weight <= 1e-5f) {
            continue;
        }
        const float pw =
            PatternWeight(layer.Expr->Pattern, layer.Expr->PatternPeriodSec, timeSec, layer.Expr->VariationSeed);
        const float w = std::clamp(layer.Weight * pw, 0.f, 1.f);
        AccumulateBoneOffsetMap(outBoneDeltas, layer.Expr->BoneOffsets, w);
        ApplyMorphOffsets(outMorphs, layer.Expr->MorphWeights, w);
    }

    if (!stack.VisemeId.empty() && stack.VisemeStrength > 1e-5f) {
        Viseme v{};
        if (visemes.TryGet(stack.VisemeId, v)) {
            AccumulateBoneOffsetMap(outBoneDeltas, v.BoneOffsets, stack.VisemeStrength);
            ApplyMorphOffsets(outMorphs, v.MorphWeights, stack.VisemeStrength);
        }
    }

    if (enableBlink) {
        const float b = BlinkClosedAmount(timeSec, facialSeed.Derive(0xE8A1));
        std::unordered_map<std::string, float> blinkMorph;
        blinkMorph["blink"] = b * 0.95f;
        ApplyMorphOffsets(outMorphs, blinkMorph, 1.f);
    }
    if (enableSaccade) {
        const Math::Vec2 sac = SaccadeOffset(timeSec, facialSeed.Derive(0x5ACCADE));
        const float mag = 0.0025f;
        std::unordered_map<std::string, Skeleton::BoneTransform> sacOff;
        sacOff["eye_L"] = MakeBT(Math::Vec3(sac.x * mag, sac.y * mag, 0), Math::Quaternion(), Math::Vec3(1, 1, 1));
        sacOff["eye_R"] = MakeBT(Math::Vec3(sac.x * mag, sac.y * mag, 0), Math::Quaternion(), Math::Vec3(1, 1, 1));
        AccumulateBoneOffsetMap(outBoneDeltas, sacOff, 1.f);
    }
}

void ApplyNamedBoneDeltasToPose(const Skeleton::Skeleton& sk, Skeleton::Pose& ioPose,
    const std::unordered_map<std::string, Skeleton::BoneTransform>& namedDeltas) {
    ApplyBoneOffsetsToPose(sk, ioPose, namedDeltas, 1.f);
}

void EvaluateFacialAtTime(const Skeleton::Skeleton& sk, Skeleton::Pose& ioPose,
    std::unordered_map<MorphTargetId, float>& ioMorphs, const ExpressionStack& stack, const VisemeSet& visemes,
    float timeSec, Seed facialSeed, bool enableBlink, bool enableSaccade) {
    std::unordered_map<std::string, Skeleton::BoneTransform> named;
    EvaluateFacialAtTimeToMaps(named, ioMorphs, stack, visemes, timeSec, facialSeed, enableBlink, enableSaccade);
    ApplyBoneOffsetsToPose(sk, ioPose, named, 1.f);
}

std::vector<std::pair<std::string, float>> PhonemeDetector::DetectFromText(const std::string& text) {
    return TextToVisemeStrengthSamples(text);
}

} // namespace Solstice::Arzachel
