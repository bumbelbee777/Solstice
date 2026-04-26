#pragma once

#include <Parallax/ParallaxScene.hxx>

#include <string>

namespace Smm::Editing {
struct ParticleEditorState;
}

namespace Solstice::Parallax {
class DevSessionAssetResolver;
}

namespace Smm::Particles {

/// Writes `SMM_ParticleEmitter` element (schema `SmmParticleEmitterElement`) from the editor. Imports sprite file into
/// `resolver` when needed. Returns false on hard failure (e.g. cannot add element).
bool SyncEditorToParallaxScene(Solstice::Parallax::ParallaxScene& scene, const Smm::Editing::ParticleEditorState& st,
    Solstice::Parallax::DevSessionAssetResolver& resolver, std::string& errOut);

/// Reads the emitter element into `st` if present; otherwise leaves `st` unchanged.
void LoadParticleEditorFromScene(const Solstice::Parallax::ParallaxScene& scene, Smm::Editing::ParticleEditorState& st);

} // namespace Smm::Particles
