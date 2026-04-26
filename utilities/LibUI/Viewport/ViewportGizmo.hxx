#pragma once

#include "LibUI/Core/Core.hxx"
#include "LibUI/Viewport/ViewportMath.hxx"
#include <imgui.h>

namespace LibUI::Viewport {

/// 12 line segments: axis-aligned box ``[bmin, bmax]`` in world space, projected with ``view * proj`` into ``[panel_min, panel_max]``.
/// Skips edges when an endpoint is behind the camera (``WorldToScreen`` returns false) or in degenerate view.
/// ``screenScalePad`` expands 2D bounds in pixels (outline thickness / selection halo).
LIBUI_API void DrawWorldAxisAlignedBoxWireframeImGui(ImDrawList* drawList, const ImVec2& panel_min, const ImVec2& panel_max,
    const Mat4Col& view, const Mat4Col& proj, float bminX, float bminY, float bminZ, float bmaxX, float bmaxY, float bmaxZ,
    ImU32 color, float thickness, float screenScalePad = 0.f);

/// Uniform cube: center ``(cx,cy,cz)`` and ``halfExtent`` on each axis.
inline void DrawWorldAxisAlignedBoxWireframeUniformImGui(ImDrawList* drawList, const ImVec2& panel_min, const ImVec2& panel_max,
    const Mat4Col& view, const Mat4Col& proj, float cx, float cy, float cz, float halfExtent, ImU32 color, float thickness,
    float screenScalePad = 0.f) {
    const float h = halfExtent;
    DrawWorldAxisAlignedBoxWireframeImGui(drawList, panel_min, panel_max, view, proj, cx - h, cy - h, cz - h, cx + h, cy + h, cz + h,
        color, thickness, screenScalePad);
}

/// Double-pass outline (dark halo + bright accent) for selected entities in 3D viewports. Uses the same 12 edge projection as
/// `DrawWorldAxisAlignedBoxWireframeImGui` with `screenScalePad` for each pass.
LIBUI_API void DrawWorldAxisAlignedBoxSelectionOutlineUniformImGui(ImDrawList* drawList, const ImVec2& panel_min,
    const ImVec2& panel_max, const Mat4Col& view, const Mat4Col& proj, float cx, float cy, float cz, float halfExtent, ImU32 innerColor,
    float innerThickness, float outerPad, ImU32 outerColor, float outerThickness);

} // namespace LibUI::Viewport
