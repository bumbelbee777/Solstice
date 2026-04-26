#pragma once

#include <Smf/SmfSpatial.hxx>

#include <cstdint>
#include <string>

namespace Jackhammer::BspTex {

/// Unwrap: shift (0,0), scale 1, rotate 0 (BXT1 still written when caller sets ``Has*`` = true).
void ApplyTextureAlignToDefaultFace(Solstice::Smf::SmfBspFaceTextureXform& outXf);

void ApplyTextureAlignToWorld(Solstice::Smf::SmfBspFaceTextureXform& outXf, const Solstice::Smf::SmfAuthoringBspNode& node,
    int faceSide, const Solstice::Smf::SmfVec3& worldHintAxis);

void ApplyTextureAlignToView(Solstice::Smf::SmfBspFaceTextureXform& outXf, const Solstice::Smf::SmfAuthoringBspNode& node,
    int faceSide, const float viewColMajor4x4[16]);

} // namespace Jackhammer::BspTex
