#include "Workflow.hxx"

#include <Parallax/ParallaxEditOps.hxx>

#include <algorithm>
#include <cmath>

namespace Solstice::MovieMaker::Workflow {

uint64_t ClampPlayhead(uint64_t tick, uint64_t timelineDurationTicks) {
    if (timelineDurationTicks == 0) {
        return 0;
    }
    if (tick > timelineDurationTicks) {
        return timelineDurationTicks;
    }
    return tick;
}

void JumpPlayheadToStart(uint64_t& playhead, uint64_t timelineDurationTicks) {
    (void)timelineDurationTicks;
    playhead = 0;
}

void JumpPlayheadToEnd(uint64_t& playhead, uint64_t timelineDurationTicks) {
    playhead = timelineDurationTicks > 0 ? timelineDurationTicks : 0;
}

uint64_t SnapTickToWholeSeconds(uint64_t tick, uint32_t ticksPerSecond) {
    const uint32_t tps = std::max(1u, ticksPerSecond);
    const double sec = static_cast<double>(tick) / static_cast<double>(tps);
    const double rounded = std::round(sec);
    return static_cast<uint64_t>(std::max(0.0, rounded * static_cast<double>(tps)));
}

void ShiftSceneKeyframes(Solstice::Parallax::ParallaxScene& scene, int64_t deltaTicks) {
    Solstice::Parallax::ShiftAllKeyframeTimes(scene, deltaTicks);
}

} // namespace Solstice::MovieMaker::Workflow
