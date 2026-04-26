#pragma once

#include <Parallax/ParallaxScene.hxx>

#include <cstdint>

namespace Solstice::MovieMaker::Workflow {

/// End-to-end authoring milestones (UX checklist; not authoritative state).
struct SessionMilestones {
    bool projectTouched{false};
    bool assetsImported{false};
    bool sceneEdited{false};
    bool prlxExported{false};
    bool videoExported{false};
};

/// Short hint for the next likely step in a linear story (import → edit → .prlx → video).
const char* NextSuggestedAction(const SessionMilestones& m);

/// Clamp tick to [0, duration] when duration > 0.
uint64_t ClampPlayhead(uint64_t tick, uint64_t timelineDurationTicks);

void JumpPlayheadToStart(uint64_t& playhead, uint64_t timelineDurationTicks);
void JumpPlayheadToEnd(uint64_t& playhead, uint64_t timelineDurationTicks);

/// Snap to nearest whole-second boundary in tick space.
uint64_t SnapTickToWholeSeconds(uint64_t tick, uint32_t ticksPerSecond);

/// Editor workflow: shift all keyframes (Parallax edit op).
void ShiftSceneKeyframes(Solstice::Parallax::ParallaxScene& scene, int64_t deltaTicks);

/// Clamp playhead into an optional **loop** region ``[loopStart, loopEnd]`` (inclusive). If ``loopEnd <= loopStart`` or
/// ``!enabled``, returns ``ClampPlayhead(tick, timelineDurationTicks)``.
uint64_t ClampPlayheadWithLoop(uint64_t tick, uint64_t timelineDurationTicks, bool loopEnabled, uint64_t loopStartTick,
    uint64_t loopEndTick);

/// Multi-tick step for Left/Right with a **playback speed** multiplier (1 = ~one 30 Hz frame worth of ticks at 1×).
uint64_t TickStepForArrowKey(uint32_t ticksPerSecond, float playbackSpeed);

} // namespace Solstice::MovieMaker::Workflow
