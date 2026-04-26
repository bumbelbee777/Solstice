#pragma once

#include <Parallax/ParallaxScene.hxx>

#include <cstdint>

namespace Smm::Editing {

/// Docked panel: list and edit `SmmFluidVolumeElement` rows (authoring-only; matches Jackhammer-style NS volume fields).
void DrawFluidVolumesPanel(const char* windowId, bool* pOpen, Solstice::Parallax::ParallaxScene& scene, uint64_t timeTicks,
    int& selectedFluidElementIndex, bool compressPrlx, bool& sceneDirty);

} // namespace Smm::Editing
