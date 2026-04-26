#include "TestHarness.hxx"

#include <Arzachel/FacialAnimation.hxx>
#include <Arzachel/TextLipSync.hxx>

#include <Skeleton/Skeleton.hxx>

#include <cmath>
#include <iostream>

namespace A = Solstice::Arzachel;
namespace Sk = Solstice::Skeleton;

int main() {
    {
        const float x = A::BlinkClosedAmount(4.2f, A::Seed(404u));
        const float y = A::BlinkClosedAmount(4.2f, A::Seed(404u));
        SOLSTICE_TEST_ASSERT(x == y, "BlinkClosedAmount must be deterministic for same seed+time");
        SOLSTICE_TEST_PASS("blink deterministic");
    }
    {
        const Solstice::Math::Vec2 u = A::SaccadeOffset(1.1f, A::Seed(7u));
        const Solstice::Math::Vec2 v = A::SaccadeOffset(1.1f, A::Seed(7u));
        SOLSTICE_TEST_ASSERT(u.x == v.x && u.y == v.y, "SaccadeOffset deterministic");
        SOLSTICE_TEST_PASS("saccade deterministic");
    }
    {
        A::VisemeSet vs = A::VisemeSet::Standard();
        A::Viseme a{};
        A::Viseme b{};
        SOLSTICE_TEST_ASSERT(vs.TryGet("aa", a), "standard viseme aa");
        SOLSTICE_TEST_ASSERT(vs.TryGet("sil", b), "standard viseme sil");
        A::Viseme m = vs.Blend(a, b, 0.5f);
        SOLSTICE_TEST_ASSERT(!m.BoneOffsets.empty(), "blended viseme has bones");
        SOLSTICE_TEST_PASS("viseme blend");
    }
    {
        std::vector<A::VisemeKeyframeTick> keys;
        std::string w;
        A::BuildVisemeKeyframesFromEnglishText("abc", 6000, 0, 12000, 64, keys, w);
        SOLSTICE_TEST_ASSERT(keys.size() >= 2, "viseme keys from text");
        SOLSTICE_TEST_PASS("text viseme keys");
    }
    {
        A::ExpressionStack st{};
        const A::Expression* happy = A::BuiltinExpressionByName("happy");
        SOLSTICE_TEST_ASSERT(happy != nullptr, "builtin happy");
        st.Expressions.push_back({happy, 1.f});
        st.VisemeId = "aa";
        st.VisemeStrength = 0.5f;
        std::unordered_map<std::string, Sk::BoneTransform> bones;
        std::unordered_map<A::MorphTargetId, float> morphs;
        A::EvaluateFacialAtTimeToMaps(bones, morphs, st, A::VisemeSet::Standard(), 0.f, A::Seed(1u), false, false);
        SOLSTICE_TEST_ASSERT(!bones.empty() || !morphs.empty(), "facial maps non-empty");
        SOLSTICE_TEST_PASS("EvaluateFacialAtTimeToMaps");
    }
    {
        std::vector<Sk::Bone> bonesV;
        bonesV.push_back(Sk::Bone(Sk::BoneID(0), "jaw", Sk::BoneID(0xFFFFFFFF), Solstice::Math::Matrix4::Identity(),
            Solstice::Math::Matrix4::Identity()));
        const Sk::Skeleton sk(bonesV, Sk::BoneID(0));
        Sk::Pose pose{};
        std::unordered_map<std::string, Sk::BoneTransform> named;
        named["jaw"] = Sk::BoneTransform{};
        named["jaw"].Translation = Solstice::Math::Vec3(0, -0.05f, 0);
        A::ApplyNamedBoneDeltasToPose(sk, pose, named);
        const Sk::BoneTransform t = pose.GetTransform(Sk::BoneID(0));
        SOLSTICE_TEST_ASSERT(std::abs(t.Translation.y + 0.05f) < 1e-5f, "named delta applied to pose");
        SOLSTICE_TEST_PASS("ApplyNamedBoneDeltasToPose");
    }
    {
        A::PhonemeDetector det;
        const auto s = det.DetectFromText("test");
        SOLSTICE_TEST_ASSERT(!s.empty(), "DetectFromText non-empty");
        SOLSTICE_TEST_ASSERT(det.DetectFromAudio(nullptr).empty(), "audio stub empty");
        SOLSTICE_TEST_PASS("PhonemeDetector text path");
    }

    return SolsticeTestMainResult("FacialAnimationTest");
}
