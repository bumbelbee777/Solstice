#pragma once

#include "ParallaxScene.hxx"

namespace Solstice::Parallax {

/// Add `deltaTicks` to every keyframe time in channel + MG tracks (clamped to 0). Useful for batch re-time in editors.
void ShiftAllKeyframeTimes(ParallaxScene& scene, int64_t deltaTicks);

} // namespace Solstice::Parallax
