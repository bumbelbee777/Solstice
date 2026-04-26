#include "JackhammerViewportDraw.hxx"
#include "JackhammerParticles.hxx"
#include "JackhammerSpatial.hxx"

#include "LibUI/Tools/RgbaImageFile.hxx"
#include "LibUI/Viewport/Viewport.hxx"

#include <Smf/SmfMap.hxx>

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace Jackhammer::ViewportDraw {

namespace {

std::unordered_map<std::string, ImU32> g_BspPlaneTexTintCache;
std::unordered_map<std::string, ImU32> g_BspPlaneTexBackTintCache;

} // namespace

void ClearBspTextureTintCaches() {
    g_BspPlaneTexTintCache.clear();
    g_BspPlaneTexBackTintCache.clear();
}

ImU32 ResolveBspPlaneTintFront(const Solstice::Smf::SmfAuthoringBspNode& nd) {
    using Solstice::Smf::SmfAuthoringBspNode;
    if (!nd.FrontTexturePath.empty()) {
        const auto it = g_BspPlaneTexTintCache.find(nd.FrontTexturePath);
        if (it != g_BspPlaneTexTintCache.end()) {
            return it->second;
        }
        try {
            std::vector<std::byte> rgba;
            int iw = 0;
            int ih = 0;
            if (LibUI::Tools::LoadImageFileToRgba8(nd.FrontTexturePath, rgba, iw, ih)) {
                float rgb[3]{};
                LibUI::Tools::AverageRgbFromRgba8(rgba.data(), iw, ih, rgb);
                const ImU32 c = IM_COL32(static_cast<int>(std::clamp(rgb[0], 0.f, 1.f) * 255.f),
                    static_cast<int>(std::clamp(rgb[1], 0.f, 1.f) * 255.f),
                    static_cast<int>(std::clamp(rgb[2], 0.f, 1.f) * 255.f), 210);
                g_BspPlaneTexTintCache[nd.FrontTexturePath] = c;
                return c;
            }
        } catch (const std::bad_alloc&) {
        }
    }
    return IM_COL32(255, 150, 70, 160);
}

ImU32 ResolveBspPlaneTintBack(const Solstice::Smf::SmfAuthoringBspNode& nd) {
    if (!nd.BackTexturePath.empty()) {
        const auto it = g_BspPlaneTexBackTintCache.find(nd.BackTexturePath);
        if (it != g_BspPlaneTexBackTintCache.end()) {
            return it->second;
        }
        try {
            std::vector<std::byte> rgba;
            int iw = 0;
            int ih = 0;
            if (LibUI::Tools::LoadImageFileToRgba8(nd.BackTexturePath, rgba, iw, ih)) {
                float rgb[3]{};
                LibUI::Tools::AverageRgbFromRgba8(rgba.data(), iw, ih, rgb);
                const ImU32 c = IM_COL32(static_cast<int>(std::clamp(rgb[0], 0.f, 1.f) * 255.f),
                    static_cast<int>(std::clamp(rgb[1], 0.f, 1.f) * 255.f),
                    static_cast<int>(std::clamp(rgb[2], 0.f, 1.f) * 255.f), 210);
                g_BspPlaneTexBackTintCache[nd.BackTexturePath] = c;
                return c;
            }
        } catch (const std::bad_alloc&) {
        }
    }
    return IM_COL32(90, 190, 255, 160);
}

void DrawAabbOutline(ImDrawList* dl, const ImVec2& panelMin, const ImVec2& panelMax,
    const LibUI::Viewport::Mat4Col& view, const LibUI::Viewport::Mat4Col& proj, float x0, float y0, float z0, float x1,
    float y1, float z1, ImU32 col) {
    const float px[8] = {x0, x1, x1, x0, x0, x1, x1, x0};
    const float py[8] = {y0, y0, y1, y1, y0, y0, y1, y1};
    const float pz[8] = {z0, z0, z0, z0, z1, z1, z1, z1};
    ImVec2 s[8]{};
    for (int i = 0; i < 8; ++i) {
        if (!LibUI::Viewport::WorldToScreen(view, proj, px[i], py[i], pz[i], panelMin, panelMax, s[i])) {
            return;
        }
    }
    const int edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    for (auto& e : edges) {
        dl->AddLine(s[e[0]], s[e[1]], col, 1.5f);
    }
}

void DrawWorldLine(ImDrawList* dl, const ImVec2& pmin, const ImVec2& pmax, const LibUI::Viewport::Mat4Col& view,
    const LibUI::Viewport::Mat4Col& proj, float ax, float ay, float az, float bx, float by, float bz, ImU32 col,
    float thick) {
    ImVec2 sa, sb;
    if (!LibUI::Viewport::WorldToScreen(view, proj, ax, ay, az, pmin, pmax, sa)) {
        return;
    }
    if (!LibUI::Viewport::WorldToScreen(view, proj, bx, by, bz, pmin, pmax, sb)) {
        return;
    }
    dl->AddLine(sa, sb, col, thick);
}

void DrawWorldCircle(ImDrawList* dl, const ImVec2& pmin, const ImVec2& pmax, const LibUI::Viewport::Mat4Col& view,
    const LibUI::Viewport::Mat4Col& proj, float cx, float cy, float cz, const Solstice::Smf::SmfVec3& u,
    const Solstice::Smf::SmfVec3& v, float radius, ImU32 col, int segs) {
    ImVec2 prev{};
    bool havePrev = false;
    for (int i = 0; i <= segs; ++i) {
        const float t = (static_cast<float>(i) / static_cast<float>(segs)) * 6.2831855f;
        const float ct = std::cos(t);
        const float st = std::sin(t);
        const float wx = cx + (u.x * ct + v.x * st) * radius;
        const float wy = cy + (u.y * ct + v.y * st) * radius;
        const float wz = cz + (u.z * ct + v.z * st) * radius;
        ImVec2 sp;
        if (!LibUI::Viewport::WorldToScreen(view, proj, wx, wy, wz, pmin, pmax, sp)) {
            havePrev = false;
            continue;
        }
        if (havePrev) {
            dl->AddLine(prev, sp, col, 1.5f);
        }
        prev = sp;
        havePrev = true;
    }
}

void DrawWorldCircleTwoHalf(ImDrawList* dl, const ImVec2& pmin, const ImVec2& pmax,
    const LibUI::Viewport::Mat4Col& view, const LibUI::Viewport::Mat4Col& proj, float cx, float cy, float cz,
    const Solstice::Smf::SmfVec3& u, const Solstice::Smf::SmfVec3& v, float radius, ImU32 col0, ImU32 col1, int segs) {
    segs = std::max(segs, 4);
    const int half = segs / 2;
    for (int pass = 0; pass < 2; ++pass) {
        const ImU32 col = pass == 0 ? col0 : col1;
        const int i0 = pass == 0 ? 0 : half;
        const int i1 = pass == 0 ? half : segs;
        ImVec2 prev{};
        bool havePrev = false;
        for (int i = i0; i <= i1; ++i) {
            const float t = (static_cast<float>(i) / static_cast<float>(segs)) * 6.2831855f;
            const float ct = std::cos(t);
            const float st = std::sin(t);
            const float wx = cx + (u.x * ct + v.x * st) * radius;
            const float wy = cy + (u.y * ct + v.y * st) * radius;
            const float wz = cz + (u.z * ct + v.z * st) * radius;
            ImVec2 sp;
            if (!LibUI::Viewport::WorldToScreen(view, proj, wx, wy, wz, pmin, pmax, sp)) {
                havePrev = false;
                continue;
            }
            if (havePrev) {
                dl->AddLine(prev, sp, col, 1.5f);
            }
            prev = sp;
            havePrev = true;
        }
    }
}

void DrawBspPlaneGizmo(ImDrawList* dl, const ImVec2& pmin, const ImVec2& pmax, const LibUI::Viewport::Mat4Col& view,
    const LibUI::Viewport::Mat4Col& proj, const Solstice::Smf::SmfAuthoringBspNode& nd, ImU32 colFront, ImU32 colBack,
    float discRadius) {
    using namespace Jackhammer::Spatial;
    const Solstice::Smf::SmfVec3 nraw = nd.PlaneNormal;
    const float px = nraw.x * nd.PlaneD;
    const float py = nraw.y * nd.PlaneD;
    const float pz = nraw.z * nd.PlaneD;
    const Solstice::Smf::SmfVec3 nu = NormalizeSmfVec3(nraw);
    Solstice::Smf::SmfVec3 u{}, v{};
    OrthoBasisFromNormal(nu, u, v);
    DrawWorldCircleTwoHalf(dl, pmin, pmax, view, proj, px, py, pz, u, v, discRadius, colFront, colBack, 24);
    DrawWorldLine(dl, pmin, pmax, view, proj, px, py, pz, px + nu.x * 0.75f, py + nu.y * 0.75f, pz + nu.z * 0.75f,
        colFront, 2.f);
}

void DrawBspTreeOverlay(ImDrawList* dl, const ImVec2& pmin, const ImVec2& pmax, const LibUI::Viewport::Mat4Col& view,
    const LibUI::Viewport::Mat4Col& proj, const Solstice::Smf::SmfAuthoringBsp& b, int nodeIdx, int depth, int maxDepth,
    int selectedIdx, float discR) {
    if (nodeIdx < 0 || nodeIdx >= static_cast<int>(b.Nodes.size()) || depth > maxDepth) {
        return;
    }
    const auto& nd = b.Nodes[static_cast<size_t>(nodeIdx)];
    const ImU32 colFront = (nodeIdx == selectedIdx) ? IM_COL32(255, 230, 90, 255) : ResolveBspPlaneTintFront(nd);
    const ImU32 colBack = (nodeIdx == selectedIdx) ? IM_COL32(255, 230, 90, 255) : ResolveBspPlaneTintBack(nd);
    DrawBspPlaneGizmo(dl, pmin, pmax, view, proj, nd, colFront, colBack, discR);
    if (nd.SlabValid) {
        const float ax0 = std::min(nd.SlabMin.x, nd.SlabMax.x);
        const float ay0 = std::min(nd.SlabMin.y, nd.SlabMax.y);
        const float az0 = std::min(nd.SlabMin.z, nd.SlabMax.z);
        const float ax1 = std::max(nd.SlabMin.x, nd.SlabMax.x);
        const float ay1 = std::max(nd.SlabMin.y, nd.SlabMax.y);
        const float az1 = std::max(nd.SlabMin.z, nd.SlabMax.z);
        DrawAabbOutline(dl, pmin, pmax, view, proj, ax0, ay0, az0, ax1, ay1, az1, IM_COL32(120, 200, 255, 130));
    }
    DrawBspTreeOverlay(dl, pmin, pmax, view, proj, b, nd.FrontChild, depth + 1, maxDepth, selectedIdx, discR * 0.92f);
    DrawBspTreeOverlay(dl, pmin, pmax, view, proj, b, nd.BackChild, depth + 1, maxDepth, selectedIdx, discR * 0.92f);
}

void DrawOctreeTreeOverlay(ImDrawList* dl, const ImVec2& pmin, const ImVec2& pmax,
    const LibUI::Viewport::Mat4Col& view, const LibUI::Viewport::Mat4Col& proj, const Solstice::Smf::SmfAuthoringOctree& o,
    int nodeIdx, int depth, int maxDepth, int selectedIdx) {
    if (nodeIdx < 0 || nodeIdx >= static_cast<int>(o.Nodes.size()) || depth > maxDepth) {
        return;
    }
    const auto& nd = o.Nodes[static_cast<size_t>(nodeIdx)];
    const ImU32 col = (nodeIdx == selectedIdx) ? IM_COL32(140, 255, 200, 255) : IM_COL32(72, 200, 255, 110);
    DrawAabbOutline(dl, pmin, pmax, view, proj, nd.Min.x, nd.Min.y, nd.Min.z, nd.Max.x, nd.Max.y, nd.Max.z, col);
    for (int i = 0; i < 8; ++i) {
        const int ch = nd.Children[static_cast<size_t>(i)];
        if (ch >= 0) {
            DrawOctreeTreeOverlay(dl, pmin, pmax, view, proj, o, ch, depth + 1, maxDepth, selectedIdx);
        }
    }
}

void DrawAuthoringLightOverlay(ImDrawList* dl, const ImVec2& pmin, const ImVec2& pmax,
    const LibUI::Viewport::Mat4Col& view, const LibUI::Viewport::Mat4Col& proj, const Solstice::Smf::SmfAuthoringLight& L,
    bool selected) {
    using Solstice::Smf::SmfAuthoringLightType;
    using Solstice::Smf::SmfVec3;
    const ImU32 accent = selected ? IM_COL32(255, 255, 140, 255) : IM_COL32(255, 210, 120, 200);
    const float r = std::clamp(L.Color.x, 0.f, 1.f);
    const float g = std::clamp(L.Color.y, 0.f, 1.f);
    const float b = std::clamp(L.Color.z, 0.f, 1.f);
    const ImU32 ccol = IM_COL32(static_cast<int>(r * 255.f), static_cast<int>(g * 255.f), static_cast<int>(b * 255.f), 255);

    switch (L.Type) {
    case SmfAuthoringLightType::Point: {
        LibUI::Viewport::DrawWorldCrossXZ(dl, pmin, pmax, view, proj, L.Position.x, L.Position.y, L.Position.z, 0.28f,
            ccol);
        break;
    }
    case SmfAuthoringLightType::Directional: {
        const SmfVec3 du = Jackhammer::Spatial::SmfDirectionUnit(L.Direction);
        const float len = 5.f;
        DrawWorldLine(dl, pmin, pmax, view, proj, 0.f, 0.f, 0.f, du.x * len, du.y * len, du.z * len, accent, 2.5f);
        DrawWorldLine(dl, pmin, pmax, view, proj, 0.f, 0.f, 0.f, -du.x * len * 0.35f, -du.y * len * 0.35f,
            -du.z * len * 0.35f, ccol, 2.f);
        break;
    }
    case SmfAuthoringLightType::Spot:
    default: {
        const float px = L.Position.x;
        const float py = L.Position.y;
        const float pz = L.Position.z;
        LibUI::Viewport::DrawWorldCrossXZ(dl, pmin, pmax, view, proj, px, py, pz, 0.22f, ccol);
        const SmfVec3 du = Jackhammer::Spatial::SmfDirectionUnit(L.Direction);
        const float range = L.Range > 1e-3f ? std::min(L.Range, 18.f) : 8.f;
        const float outerRad = range * std::tan(L.SpotOuterDeg * 3.14159265f / 180.f * 0.5f);
        const float tx = px + du.x * range;
        const float ty = py + du.y * range;
        const float tz = pz + du.z * range;
        DrawWorldLine(dl, pmin, pmax, view, proj, px, py, pz, tx, ty, tz, accent, 2.f);
        SmfVec3 u{}, v{};
        Jackhammer::Spatial::OrthoBasisFromNormal(du, u, v);
        DrawWorldCircle(dl, pmin, pmax, view, proj, tx, ty, tz, u, v, std::max(outerRad, 0.05f), ccol, 28);
        break;
    }
    }
}

void DrawParticleEmitterMarkers(ImDrawList* dl, const ImVec2& pmin, const ImVec2& pmax,
    const LibUI::Viewport::Mat4Col& view, const LibUI::Viewport::Mat4Col& proj, const Solstice::Smf::SmfMap& map) {
    Particles::ForEachParticleEmitterOrigin(map, [&](const Solstice::Smf::SmfVec3& o) {
        LibUI::Viewport::DrawWorldCrossXZ(dl, pmin, pmax, view, proj, o.x, o.y, o.z, 0.32f, IM_COL32(255, 120, 220, 230));
        DrawWorldLine(dl, pmin, pmax, view, proj, o.x, o.y + 0.4f, o.z, o.x, o.y + 1.1f, o.z,
            IM_COL32(255, 160, 230, 200), 2.f);
    });
}

} // namespace Jackhammer::ViewportDraw
