#pragma once

#include <Smf/SmfMap.hxx>

#include <imgui.h>

#include "LibUI/Viewport/ViewportMath.hxx"

struct ImDrawList;

namespace Jackhammer::ViewportDraw {

void ClearBspTextureTintCaches();

ImU32 ResolveBspPlaneTintFront(const Solstice::Smf::SmfAuthoringBspNode& nd);
ImU32 ResolveBspPlaneTintBack(const Solstice::Smf::SmfAuthoringBspNode& nd);

void DrawAabbOutline(ImDrawList* dl, const ImVec2& panelMin, const ImVec2& panelMax,
    const LibUI::Viewport::Mat4Col& view, const LibUI::Viewport::Mat4Col& proj, float x0, float y0, float z0, float x1,
    float y1, float z1, ImU32 col);

void DrawWorldLine(ImDrawList* dl, const ImVec2& pmin, const ImVec2& pmax, const LibUI::Viewport::Mat4Col& view,
    const LibUI::Viewport::Mat4Col& proj, float ax, float ay, float az, float bx, float by, float bz, ImU32 col,
    float thick);

void DrawWorldCircle(ImDrawList* dl, const ImVec2& pmin, const ImVec2& pmax, const LibUI::Viewport::Mat4Col& view,
    const LibUI::Viewport::Mat4Col& proj, float cx, float cy, float cz, const Solstice::Smf::SmfVec3& u,
    const Solstice::Smf::SmfVec3& v, float radius, ImU32 col, int segs);

void DrawWorldCircleTwoHalf(ImDrawList* dl, const ImVec2& pmin, const ImVec2& pmax,
    const LibUI::Viewport::Mat4Col& view, const LibUI::Viewport::Mat4Col& proj, float cx, float cy, float cz,
    const Solstice::Smf::SmfVec3& u, const Solstice::Smf::SmfVec3& v, float radius, ImU32 col0, ImU32 col1, int segs);

void DrawBspPlaneGizmo(ImDrawList* dl, const ImVec2& pmin, const ImVec2& pmax, const LibUI::Viewport::Mat4Col& view,
    const LibUI::Viewport::Mat4Col& proj, const Solstice::Smf::SmfAuthoringBspNode& nd, ImU32 colFront, ImU32 colBack,
    float discRadius);

void DrawBspTreeOverlay(ImDrawList* dl, const ImVec2& pmin, const ImVec2& pmax, const LibUI::Viewport::Mat4Col& view,
    const LibUI::Viewport::Mat4Col& proj, const Solstice::Smf::SmfAuthoringBsp& b, int nodeIdx, int depth, int maxDepth,
    int selectedIdx, float discR);

void DrawOctreeTreeOverlay(ImDrawList* dl, const ImVec2& pmin, const ImVec2& pmax,
    const LibUI::Viewport::Mat4Col& view, const LibUI::Viewport::Mat4Col& proj, const Solstice::Smf::SmfAuthoringOctree& o,
    int nodeIdx, int depth, int maxDepth, int selectedIdx);

void DrawAuthoringLightOverlay(ImDrawList* dl, const ImVec2& pmin, const ImVec2& pmax,
    const LibUI::Viewport::Mat4Col& view, const LibUI::Viewport::Mat4Col& proj, const Solstice::Smf::SmfAuthoringLight& L,
    bool selected);

/// Simple viewport markers for `ParticleEmitter` entities (authoring preview only).
void DrawParticleEmitterMarkers(ImDrawList* dl, const ImVec2& pmin, const ImVec2& pmax,
    const LibUI::Viewport::Mat4Col& view, const LibUI::Viewport::Mat4Col& proj, const Solstice::Smf::SmfMap& map);

} // namespace Jackhammer::ViewportDraw
