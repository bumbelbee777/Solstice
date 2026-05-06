#include "TestHarness.hxx"

#include <Arzachel/FacialAnimation.hxx>
#include <Arzachel/TextLipSync.hxx>
#include <Game/Systems/FacialSystem.hxx>
#include <Entity/Registry.hxx>
#include <Entity/Components/FacialComponents.hxx>

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
        std::vector<A::VisemeKeyframeTick> keys;
        std::string w;
        A::LipSyncTextConfig cfg{};
        cfg.EnableDigraphs = true;
        cfg.Coarticulation = 0.6f;
        A::BuildVisemeKeyframesFromEnglishTextEx("the ship moves!", 6000, 0, 12000, 64, cfg, keys, w);
        SOLSTICE_TEST_ASSERT(!keys.empty(), "enhanced viseme keys");
        bool hasPause = false;
        for (const auto& k : keys) {
            if (k.VisemeId == "sil") {
                hasPause = true;
                break;
            }
        }
        SOLSTICE_TEST_ASSERT(hasPause, "punctuation pause viseme present");
        A::CompactVisemeKeyframes(keys, 0.05f);
        SOLSTICE_TEST_PASS("text viseme keys extended");
    }
    {
        std::vector<A::VisemeKeyframeTick> keys{
            {0, "aa", 0.6f},
            {50, "O", 0.9f}
        };
        std::string cur;
        std::string next;
        float blend = 0.0f;
        float strength = 0.0f;
        A::SampleVisemeAtTick(keys, 25, cur, next, blend, strength);
        SOLSTICE_TEST_ASSERT(cur == "aa", "sample viseme current");
        SOLSTICE_TEST_ASSERT(next == "O", "sample viseme next");
        SOLSTICE_TEST_ASSERT(blend > 0.45f && blend < 0.55f, "sample viseme blend");
        SOLSTICE_TEST_PASS("viseme sampling");
    }
    {
        A::ExpressionStack st{};
        const A::Expression* happy = A::BuiltinExpressionByName("happy");
        SOLSTICE_TEST_ASSERT(happy != nullptr, "builtin happy");
        st.Expressions.push_back({happy, 1.f});
        st.VisemeId = "aa";
        st.VisemeStrength = 0.5f;
        st.NextVisemeId = "O";
        st.NextVisemeStrength = 0.75f;
        st.VisemeBlend = 0.5f;
        std::unordered_map<std::string, Sk::BoneTransform> bones;
        std::unordered_map<A::MorphTargetId, float> morphs;
        A::FacialSolveConfig config{};
        config.MorphSoftClamp = 1.0f;
        config.EnableVisemeCoarticulation = true;
        A::EvaluateFacialAtTimeToMapsEx(bones, morphs, st, A::VisemeSet::Standard(), 0.f, A::Seed(1u), false, false, config);
        SOLSTICE_TEST_ASSERT(!bones.empty() || !morphs.empty(), "facial maps non-empty");
        for (const auto& kv : morphs) {
            SOLSTICE_TEST_ASSERT(kv.second <= 1.0f + 1e-4f, "morph soft clamp bounded");
        }
        SOLSTICE_TEST_PASS("EvaluateFacialAtTimeToMaps");
    }
    {
        A::ExpressionTimelineState timeline{};
        timeline.Current = A::BuiltinExpressionByName("neutral");
        timeline.Target = A::BuiltinExpressionByName("happy");
        timeline.CurrentEnv = A::ExpressionEnvelope{0.05f, 0.2f, 0.3f, 1.0f};
        timeline.TargetEnv = A::ExpressionEnvelope{0.1f, 0.4f, 0.3f, 1.0f};
        A::AdvanceExpressionTimeline(timeline, 0.12f);
        A::ExpressionStack stack{};
        A::BuildStackFromTimeline(stack, timeline);
        SOLSTICE_TEST_ASSERT(!stack.Expressions.empty(), "timeline builds expression stack");
        std::unordered_map<std::string, Sk::BoneTransform> bones;
        std::unordered_map<A::MorphTargetId, float> morphs;
        A::FacialSolveConfig cfg{};
        A::EvaluateFacialFromTimelineToMaps(bones, morphs, timeline, stack, A::VisemeSet::Standard(), 0.12f, A::Seed(9u), true, true, cfg);
        SOLSTICE_TEST_ASSERT(!bones.empty() || !morphs.empty(), "timeline facial solve output");
        SOLSTICE_TEST_PASS("timeline facial evaluation");
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
    {
        ECS::Registry registry;
        auto e = registry.Create();
        auto& facial = registry.Add<ECS::FacialStateMachine>(e);
        facial.State = ECS::FacialIntentState::Speak;
        facial.MoodName = "happy";
        facial.DialogueText = "hello there!";
        facial.DialogueDirty = true;

        Game::FacialSystem system;
        system.Update(registry, 1.0f / 30.0f);
        auto* f = registry.TryGet<ECS::FacialStateMachine>(e);
        SOLSTICE_TEST_ASSERT(f != nullptr, "facial component still exists");
        SOLSTICE_TEST_ASSERT(!f->OutputBoneDeltas.empty() || !f->OutputMorphs.empty(), "facial system outputs generated");
        SOLSTICE_TEST_PASS("FacialSystem ECS update");
    }

    return SolsticeTestMainResult("FacialAnimationTest");
}
