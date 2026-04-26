#pragma once

#include <Smf/SmfMap.hxx>

#include <cstdint>
#include <string>
#include <string_view>

namespace Jackhammer::Lightmap {

/// Optional tuning for the simple BSP lightmap pass (linear RGB added before tonemap/encode).
struct LightmapBakeOptions {
    float ambientR{0.f};
    float ambientG{0.f};
    float ambientB{0.f};
    /// 0 = light the face using ``+N``; 1 = use ``-N`` (bake the opposite side in the same u×v image).
    int faceSide{0};
    /// If true, `SmfAuthoringLightType::Spot` uses inner/outer cone; if false, spots behave like points (legacy).
    bool modelSpotCones{false};
};

/// Procedural light accumulation on the selected BSP **axis-aligned** face (N·L, optional distance falloff) → RGBA8 PNG.
/// `nodeIndex` indexes ``map.Bsp->Nodes``; `resolution` in pixels per side (square, world-space u×v on the face).
/// `options` may be null (defaults: no ambient, front face, spot = point-like).
bool BakeSimpleBspFaceLightmapPng(const Solstice::Smf::SmfMap& map, int nodeIndex, int resolution, std::string_view outPathUtf8,
    std::string& errOut, const LightmapBakeOptions* options = nullptr);

} // namespace Jackhammer::Lightmap
