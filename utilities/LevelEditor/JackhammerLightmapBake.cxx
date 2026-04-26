#include "JackhammerLightmapBake.hxx"
#include "JackhammerSpatial.hxx"

#include "LibUI/Tools/RgbaImageFile.hxx"

#include <Smf/SmfMap.hxx>
#include <Smf/SmfTypes.hxx>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace {

using Solstice::Smf::SmfAuthoringBspNode;
using Solstice::Smf::SmfAuthoringLight;
using Solstice::Smf::SmfAuthoringLightType;
using Solstice::Smf::SmfMap;
using Solstice::Smf::SmfVec3;

float Dot3(const SmfVec3& a, const SmfVec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

SmfVec3 Scale3(const SmfVec3& a, float s) {
    return {a.x * s, a.y * s, a.z * s};
}

SmfVec3 Add3(const SmfVec3& a, const SmfVec3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

SmfVec3 Sub3(const SmfVec3& a, const SmfVec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

} // namespace

namespace Jackhammer::Lightmap {

namespace {
constexpr float kPi = 3.14159265f;
}

bool BakeSimpleBspFaceLightmapPng(const SmfMap& map, int nodeIndex, int resolution, std::string_view outPathUtf8,
    std::string& errOut, const LightmapBakeOptions* options) {
    errOut.clear();
    if (resolution < 4 || resolution > 2048) {
        errOut = "Lightmap: resolution out of range (4–2048).";
        return false;
    }
    if (!map.Bsp || map.Bsp->Nodes.empty()) {
        errOut = "Lightmap: map has no BSP.";
        return false;
    }
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(map.Bsp->Nodes.size())) {
        errOut = "Lightmap: invalid node index.";
        return false;
    }
    const SmfAuthoringBspNode& nd = map.Bsp->Nodes[static_cast<size_t>(nodeIndex)];
    if (!nd.SlabValid) {
        errOut = "Lightmap: node needs a valid slab (axis-aligned, finite).";
        return false;
    }
    float wW = 0.f;
    float hW = 0.f;
    if (!Jackhammer::Spatial::JhBspSlabFaceWorldSize(nd, wW, hW)) {
        errOut = "Lightmap: only axis-aligned slab faces are supported in v1 (see BSP texture fit).";
        return false;
    }
    SmfVec3 n = Jackhammer::Spatial::NormalizeSmfVec3(nd.PlaneNormal);
    if (options && options->faceSide != 0) {
        n = Scale3(n, -1.f);
    }
    SmfVec3 u, v;
    Jackhammer::Spatial::OrthoBasisFromNormal(n, u, v);
    SmfVec3 lo, hi;
    Jackhammer::Spatial::SmfAabbCanonical(lo, hi, nd.SlabMin, nd.SlabMax);
    const float mx = 0.5f * (lo.x + hi.x);
    const float my = 0.5f * (lo.y + hi.y);
    const float mz = 0.5f * (lo.z + hi.z);
    SmfVec3 center{mx, my, mz};
    {
        const float dist = Dot3(n, center) - nd.PlaneD;
        center = Sub3(center, Scale3(n, dist));
    }

    // Face-local corners: u spans wW, v spans hW (matches JhBspSlabFaceWorldSize order)
    const SmfVec3 c00 = Add3(center, Add3(Scale3(u, -0.5f * wW), Scale3(v, -0.5f * hW)));
    // Du world step per texel
    const SmfVec3 duW = Scale3(u, wW / static_cast<float>(resolution));
    const SmfVec3 dvW = Scale3(v, hW / static_cast<float>(resolution));

    std::vector<std::byte> rgba(static_cast<size_t>(resolution) * static_cast<size_t>(resolution) * 4u, std::byte{0});
    for (int j = 0; j < resolution; ++j) {
        for (int i = 0; i < resolution; ++i) {
            const float ku = (static_cast<float>(i) + 0.5f) / static_cast<float>(resolution);
            const float kv = (static_cast<float>(j) + 0.5f) / static_cast<float>(resolution);
            const SmfVec3 p = Add3(c00, Add3(Scale3(duW, ku), Scale3(dvW, kv)));
            float r = 0.f, g = 0.f, b = 0.f;
            for (const SmfAuthoringLight& L : map.AuthoringLights) {
                const float Ir = L.Color.x * L.Intensity;
                const float Ig = L.Color.y * L.Intensity;
                const float Ib = L.Color.z * L.Intensity; // struct uses x,y,z (SmfVec3)
                if (L.Type == SmfAuthoringLightType::Directional) {
                    const SmfVec3 ld = Jackhammer::Spatial::NormalizeSmfVec3(L.Direction);
                    const SmfVec3 toLight = Scale3(ld, -1.f);
                    const float ndl = std::max(0.f, Dot3(n, toLight));
                    r += Ir * ndl;
                    g += Ig * ndl;
                    b += Ib * ndl;
                } else {
                    const SmfVec3 toL = Sub3(L.Position, p);
                    const float dist2 = std::max(1.0e-4f, Dot3(toL, toL));
                    const float dist = std::sqrt(dist2);
                    const SmfVec3 ldir = Scale3(toL, 1.0f / dist);
                    const float ndl = std::max(0.f, Dot3(n, ldir));
                    float att = 1.f / (1.f + 0.01f * dist2);
                    if (L.Range > 1.0e-3f) {
                        const float rr = L.Range;
                        if (dist > rr) {
                            att = 0.f;
                        }
                    }
                    float coneAtt = 1.f;
                    if (L.Type == SmfAuthoringLightType::Spot) {
                        if (options && options->modelSpotCones) {
                            const SmfVec3 fromLight = Scale3(Sub3(p, L.Position), 1.0f / dist);
                            const SmfVec3 spotForward = Jackhammer::Spatial::NormalizeSmfVec3(L.Direction);
                            const float t = std::max(-1.f, std::min(1.f, Dot3(fromLight, spotForward)));
                            const float chO = std::cos(0.5f * (L.SpotOuterDeg * (kPi / 180.f)));
                            const float chI = std::cos(0.5f * (L.SpotInnerDeg * (kPi / 180.f)));
                            if (t < chO) {
                                coneAtt = 0.f;
                            } else if (chI <= chO + 1.0e-5f) {
                                coneAtt = 1.f;
                            } else if (t >= chI) {
                                coneAtt = 1.f;
                            } else {
                                coneAtt = (t - chO) / (chI - chO);
                            }
                        }
                    }
                    r += Ir * ndl * att * coneAtt;
                    g += Ig * ndl * att * coneAtt;
                    b += Ib * ndl * att * coneAtt;
                }
            }
            if (options) {
                r = std::min(1.f, r + options->ambientR);
                g = std::min(1.f, g + options->ambientG);
                b = std::min(1.f, b + options->ambientB);
            }
            const size_t o = (static_cast<size_t>(j) * static_cast<size_t>(resolution) + static_cast<size_t>(i)) * 4u;
            r = std::sqrt(std::min(1.f, r));
            g = std::sqrt(std::min(1.f, g));
            b = std::sqrt(std::min(1.f, b));
            rgba[o + 0] = static_cast<std::byte>(static_cast<int>(r * 255.f) & 255);
            rgba[o + 1] = static_cast<std::byte>(static_cast<int>(g * 255.f) & 255);
            rgba[o + 2] = static_cast<std::byte>(static_cast<int>(b * 255.f) & 255);
            rgba[o + 3] = std::byte{255};
        }
    }
    {
        const std::filesystem::path outPngPath{std::string(outPathUtf8)};
        std::error_code ec;
        std::filesystem::create_directories(outPngPath.parent_path(), ec);
    }
    if (!LibUI::Tools::SaveRgba8ToPngFile(std::string(outPathUtf8), rgba.data(), resolution, resolution)) {
        errOut = "Lightmap: failed to write PNG (path or I/O).";
        return false;
    }
    return true;
}

} // namespace Jackhammer::Lightmap
