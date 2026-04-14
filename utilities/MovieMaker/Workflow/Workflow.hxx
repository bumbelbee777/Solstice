#pragma once

#include <Parallax/ParallaxScene.hxx>

#include <cstdint>

namespace Solstice::MovieMaker::Workflow {

/// Clamp tick to [0, duration] when duration > 0.
uint64_t ClampPlayhead(uint64_t tick, uint64_t timelineDurationTicks);

void JumpPlayheadToStart(uint64_t& playhead, uint64_t timelineDurationTicks);
void JumpPlayheadToEnd(uint64_t& playhead, uint64_t timelineDurationTicks);

/// Snap to nearest whole-second boundary in tick space.
uint64_t SnapTickToWholeSeconds(uint64_t tick, uint32_t ticksPerSecond);

/// Editor workflow: shift all keyframes (Parallax edit op).
void ShiftSceneKeyframes(Solstice::Parallax::ParallaxScene& scene, int64_t deltaTicks);

} // namespace Solstice::MovieMaker::Workflow
