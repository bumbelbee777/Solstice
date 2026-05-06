#include "Systems/FacialSystem.hxx"
#include <Entity/Components/FacialComponents.hxx>
#include <Entity/Registry.hxx>
#include <algorithm>

namespace Solstice::Game {

namespace {

const Arzachel::Expression* ResolveExpression(ECS::FacialStateMachine& facial) {
    switch (facial.State) {
        case ECS::FacialIntentState::Emote:
            if (!facial.EmoteName.empty()) {
                if (const auto* e = Arzachel::BuiltinExpressionByName(facial.EmoteName)) {
                    return e;
                }
            }
            break;
        case ECS::FacialIntentState::Speak:
            if (!facial.MoodName.empty()) {
                if (const auto* e = Arzachel::BuiltinExpressionByName(facial.MoodName)) {
                    return e;
                }
            }
            return Arzachel::BuiltinExpressionByName("neutral");
        case ECS::FacialIntentState::Listen:
            return Arzachel::BuiltinExpressionByName("neutral");
        case ECS::FacialIntentState::Idle:
        default:
            if (!facial.MoodName.empty()) {
                if (const auto* e = Arzachel::BuiltinExpressionByName(facial.MoodName)) {
                    return e;
                }
            }
            return Arzachel::BuiltinExpressionByName("neutral");
    }
}

Arzachel::ExpressionEnvelope ResolveEnvelope(const ECS::FacialStateMachine& facial) {
    switch (facial.State) {
        case ECS::FacialIntentState::Listen: return facial.ListenEnvelope;
        case ECS::FacialIntentState::Speak: return facial.SpeakEnvelope;
        case ECS::FacialIntentState::Emote: return facial.EmoteEnvelope;
        case ECS::FacialIntentState::Idle:
        default: return facial.IdleEnvelope;
    }
}

} // namespace

void FacialSystem::Update(ECS::Registry& registry, float deltaTime) {
    registry.ForEach<ECS::FacialStateMachine>([&](ECS::EntityId, ECS::FacialStateMachine& facial) {
        facial.TimeSec += std::max(0.0f, deltaTime);

        const auto* targetExpr = ResolveExpression(facial);
        if (facial.Timeline.Target != targetExpr) {
            facial.Timeline.Current = facial.Timeline.Target ? facial.Timeline.Target : facial.Timeline.Current;
            facial.Timeline.CurrentEnv = facial.Timeline.Target ? facial.Timeline.TargetEnv : facial.Timeline.CurrentEnv;
            facial.Timeline.CurrentAgeSec = 0.0f;

            facial.Timeline.Target = targetExpr;
            facial.Timeline.TargetEnv = ResolveEnvelope(facial);
            facial.Timeline.TargetAgeSec = 0.0f;
        }
        Arzachel::AdvanceExpressionTimeline(facial.Timeline, deltaTime);
        Arzachel::BuildStackFromTimeline(facial.Stack, facial.Timeline);

        if (facial.DialogueDirty) {
            facial.DialogueDirty = false;
            std::string warn;
            Arzachel::BuildVisemeKeyframesFromEnglishTextEx(
                facial.DialogueText,
                facial.TicksPerSecond,
                facial.DialogueStartTick,
                facial.DialogueEndTick,
                facial.MaxVisemeKeys,
                facial.LipSyncConfig,
                facial.VisemeKeys,
                warn
            );
            Arzachel::CompactVisemeKeyframes(facial.VisemeKeys, 0.03f);
        }

        if (facial.State == ECS::FacialIntentState::Speak && !facial.VisemeKeys.empty()) {
            const float lead = std::max(0.0f, facial.VisemeLeadSec);
            const uint64_t tick = static_cast<uint64_t>(std::max(0.0f, facial.TimeSec + lead) * static_cast<float>(facial.TicksPerSecond));
            std::string cur;
            std::string next;
            float blend = 0.0f;
            float strength = 0.0f;
            Arzachel::SampleVisemeAtTick(facial.VisemeKeys, tick, cur, next, blend, strength);
            facial.Stack.VisemeId = cur;
            facial.Stack.NextVisemeId = next;
            facial.Stack.VisemeBlend = blend;
            facial.Stack.VisemeStrength = strength;
            facial.Stack.NextVisemeStrength = strength;
        } else {
            facial.Stack.VisemeId.clear();
            facial.Stack.NextVisemeId.clear();
            facial.Stack.VisemeBlend = 0.0f;
            facial.Stack.VisemeStrength = 0.0f;
            facial.Stack.NextVisemeStrength = 0.0f;
        }

        Arzachel::EvaluateFacialFromTimelineToMaps(
            facial.OutputBoneDeltas,
            facial.OutputMorphs,
            facial.Timeline,
            facial.Stack,
            Arzachel::VisemeSet::Standard(),
            facial.TimeSec,
            facial.Seed,
            facial.EnableBlink,
            facial.EnableSaccade,
            facial.SolveConfig
        );
    });
}

} // namespace Solstice::Game
