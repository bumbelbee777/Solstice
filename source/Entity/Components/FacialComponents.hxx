#pragma once

#include <Arzachel/FacialAnimation.hxx>
#include <Arzachel/TextLipSync.hxx>
#include <Math/Vector.hxx>
#include <string>
#include <unordered_map>
#include <vector>

namespace Solstice::ECS {

enum class FacialIntentState : uint8_t {
    Idle = 0,
    Listen = 1,
    Speak = 2,
    Emote = 3,
};

struct FacialStateMachine {
    FacialIntentState State{FacialIntentState::Idle};
    std::string MoodName{"neutral"};
    std::string EmoteName{};
    std::string DialogueText{};
    bool DialogueDirty{false};

    uint32_t TicksPerSecond{6000};
    uint64_t DialogueStartTick{0};
    uint64_t DialogueEndTick{12000};
    uint32_t MaxVisemeKeys{256};

    bool EnableBlink{true};
    bool EnableSaccade{true};
    float VisemeLeadSec{0.05f};

    Arzachel::Seed Seed{1u};
    Arzachel::ExpressionTimelineState Timeline{};
    Arzachel::ExpressionStack Stack{};
    Arzachel::FacialSolveConfig SolveConfig{};
    Arzachel::LipSyncTextConfig LipSyncConfig{};
    Arzachel::ExpressionEnvelope IdleEnvelope{0.15f, 0.4f, 0.3f, 1.4f};
    Arzachel::ExpressionEnvelope ListenEnvelope{0.08f, 0.3f, 0.2f, 1.2f};
    Arzachel::ExpressionEnvelope SpeakEnvelope{0.04f, 0.15f, 0.1f, 1.0f};
    Arzachel::ExpressionEnvelope EmoteEnvelope{0.03f, 0.35f, 0.2f, 0.9f};

    float TimeSec{0.0f};
    std::vector<Arzachel::VisemeKeyframeTick> VisemeKeys;
    std::unordered_map<std::string, Skeleton::BoneTransform> OutputBoneDeltas;
    std::unordered_map<Arzachel::MorphTargetId, float> OutputMorphs;
};

} // namespace Solstice::ECS
