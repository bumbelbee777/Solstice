#pragma once

#include "AnimationClip.hxx"

namespace Solstice::Arzachel {

// Animation composition operations

// Additive blend: A + B (additive animation)
AnimationClip Add(const AnimationClip& A, const AnimationClip& B);

// Layer blend: Base + Overlay * Weight (weighted overlay)
AnimationClip Layer(const AnimationClip& Base, const AnimationClip& Overlay, float Weight);

// Merge tracks: combine tracks from two clips (later tracks override earlier ones)
AnimationClip MergeTracks(const AnimationClip& A, const AnimationClip& B);

} // namespace Solstice::Arzachel
