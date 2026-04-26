#pragma once

#include "LibUI/Core/Core.hxx"

namespace LibUI::Tools {

/// Axis-aligned box in world space (min corner, max corner).
struct AxisAlignedBox3 {
    float minX = 0.f;
    float minY = 0.f;
    float minZ = 0.f;
    float maxX = 0.f;
    float maxY = 0.f;
    float maxZ = 0.f;
};

/// Picks the **closest** box along a normalized view ray (e.g. from ``Viewport::ScreenToWorldRay``). The ImGui panel
/// rect for unproject must match the **same** sub-rect used for drawing the 3D image (``ComputeLetterbox`` when the
/// preview is letterboxed). See ``ViewportGizmo.hxx`` for selection wireframes.
/// Skips boxes where ``min > max`` on any axis. ``outIndex`` is the **index in the ``boxes`` array**; map to scene
/// elements via a parallel list. ``outIndex`` is ``-1`` when nothing hits.
LIBUI_API bool PickClosestAxisAlignedBoxAlongRay(float rayOriginX, float rayOriginY, float rayOriginZ, float rayDirX,
    float rayDirY, float rayDirZ, const AxisAlignedBox3* boxes, int boxCount, int& outIndex, float& outT);

} // namespace LibUI::Tools
