#pragma once

#include "LibUI/Viewport/Viewport.hxx"
#include <array>
#include <cstdint>

namespace LibUI::Viewport {

/// Column-major 4x4 (OpenGL `glUniformMatrix4fv` layout, column 0 = indices 0..3).
using Mat4Col = std::array<float, 16>;

/// Orbit camera around `target` (world). Y-up, right-handed.
/// Uses perspective or orthographic (`state.projection`).
/// `fovYDeg` vertical field of view in degrees; aspect = width/height.
LIBUI_API void ComputeOrbitViewProjectionColMajor(const OrbitPanZoomState& state, float targetX, float targetY,
    float targetZ, float fovYDeg, float aspect, float zNear, float zFar, Mat4Col& outView, Mat4Col& outProj);

/// Full inverse for unproject; returns false if singular.
LIBUI_API bool InverseMat4(const Mat4Col& m, Mat4Col& out);

LIBUI_API Mat4Col MultiplyMat4(const Mat4Col& a, const Mat4Col& b);

/// World homogeneous point; returns clip-space (before perspective divide).
LIBUI_API void TransformPoint4(const Mat4Col& m, float x, float y, float z, float w, float& ox, float& oy, float& oz,
    float& ow);

/// XZ ground grid (y = 0), clipped to `panel_min`/`panel_max` in screen space after project.
LIBUI_API void DrawXZGrid(ImDrawList* dl, const ImVec2& panel_min, const ImVec2& panel_max, const Mat4Col& view,
    const Mat4Col& proj, float cellWorld, ImU32 color, int halfCount = 32);

/// Project world position to ImGui screen coords inside the panel; returns false if behind camera or invalid.
LIBUI_API bool WorldToScreen(const Mat4Col& view, const Mat4Col& proj, float wx, float wy, float wz,
    const ImVec2& panel_min, const ImVec2& panel_max, ImVec2& out);

/// Horizontal cross on the XZ plane at (x,y,z) with axis extent in world units.
LIBUI_API void DrawWorldCrossXZ(ImDrawList* dl, const ImVec2& panel_min, const ImVec2& panel_max,
    const Mat4Col& view, const Mat4Col& proj, float x, float y, float z, float extent, ImU32 col);

/// Unproject screen position to a world-space ray (origin + normalized direction). Same VP convention as `WorldToScreen`.
LIBUI_API bool ScreenToWorldRay(const Mat4Col& view, const Mat4Col& proj, const ImVec2& panel_min,
    const ImVec2& panel_max, const ImVec2& screen_pos, float& originX, float& originY, float& originZ, float& dirX,
    float& dirY, float& dirZ);

/// Ray `origin + t * dir` (dir need not be unit) vs plane `dot(n,p)=d`. Returns false if parallel.
LIBUI_API bool IntersectRayPlane(float originX, float originY, float originZ, float dirX, float dirY, float dirZ,
    float planeNx, float planeNy, float planeNz, float planeD, float& outT);

/// Intersection of unprojected ray with horizontal plane y = planeY; outputs world X/Z at that Y.
LIBUI_API bool ScreenToXZPlane(const Mat4Col& view, const Mat4Col& proj, const ImVec2& panel_min,
    const ImVec2& panel_max, const ImVec2& screen_pos, float planeY, float& outX, float& outZ);

} // namespace LibUI::Viewport
