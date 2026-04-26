#include "Workflow.hxx"

#include <Parallax/ParallaxEditOps.hxx>

#include <algorithm>
#include <cmath>

namespace Solstice::MovieMaker::Workflow {

const char* NextSuggestedAction(const SessionMilestones& m) {
    if (!m.projectTouched) {
        return "Save or load a .smm.json project (paths are stored there).";
    }
    if (!m.assetsImported) {
        return "Import assets (file or folder) for Parallax.";
    }
    if (!m.sceneEdited) {
        return "Edit the Parallax scene / timeline (MG preview).";
    }
    if (!m.prlxExported) {
        return "Export a .prlx from the Parallax section.";
    }
    if (!m.videoExported) {
        return "Run video export (ffmpeg) when ready.";
    }
    return "All workflow steps reached — iterate or start a new scene.";
}

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

uint64_t ClampPlayheadWithLoop(uint64_t tick, uint64_t timelineDurationTicks, bool loopEnabled, uint64_t loopStartTick,
    uint64_t loopEndTick) {
    uint64_t t = ClampPlayhead(tick, timelineDurationTicks);
    if (!loopEnabled || loopEndTick <= loopStartTick) {
        return t;
    }
    uint64_t le = loopEndTick;
    if (timelineDurationTicks > 0 && le > timelineDurationTicks) {
        le = timelineDurationTicks;
    }
    if (t < loopStartTick) {
        return loopStartTick;
    }
    if (t > le) {
        return le;
    }
    return t;
}

uint64_t TickStepForArrowKey(uint32_t ticksPerSecond, float playbackSpeed) {
    const uint32_t tps = std::max(1u, ticksPerSecond);
    const float sp = std::max(0.0625f, playbackSpeed);
    const double ticksPer30HzFrame = static_cast<double>(tps) / 30.0;
    const double step = std::max(1.0, ticksPer30HzFrame * static_cast<double>(sp));
    return static_cast<uint64_t>(std::llround(step));
}

} // namespace Solstice::MovieMaker::Workflow
