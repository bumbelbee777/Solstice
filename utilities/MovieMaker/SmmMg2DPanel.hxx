#pragma once

#include <Parallax/ParallaxScene.hxx>

#include <cstdint>

namespace Smm {

/// After Effects–leaning 2D MG helpers: nominal comp, transform nudge, quick align, MG root layer opacity.
void DrawMg2DCompTools(Solstice::Parallax::ParallaxScene& scene, int& mgElementSelected, bool& sceneDirty, bool compressPrlx,
    float& compW, float& compH);

} // namespace Smm
