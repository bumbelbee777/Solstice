#include "ViewportGizmo.hxx"

#include <cmath>

namespace LibUI::Viewport {

namespace {
struct Edge3 {
    int a;
    int b;
};

static const Edge3 kBoxEdges[12] = {Edge3{0, 1},
    Edge3{0, 2},
    Edge3{1, 3},
    Edge3{2, 3},
    Edge3{4, 5},
    Edge3{4, 6},
    Edge3{5, 7},
    Edge3{6, 7},
    Edge3{0, 4},
    Edge3{1, 5},
    Edge3{2, 6},
    Edge3{3, 7}};
} // namespace

void DrawWorldAxisAlignedBoxWireframeImGui(ImDrawList* drawList, const ImVec2& panel_min, const ImVec2& panel_max,
    const Mat4Col& view, const Mat4Col& proj, float bminX, float bminY, float bminZ, float bmaxX, float bmaxY, float bmaxZ, ImU32 color,
    float thickness, float screenScalePad) {
    if (!drawList) {
        return;
    }
    if (bminX > bmaxX) {
        std::swap(bminX, bmaxX);
    }
    if (bminY > bmaxY) {
        std::swap(bminY, bmaxY);
    }
    if (bminZ > bmaxZ) {
        std::swap(bminZ, bmaxZ);
    }
    const float px[8] = {bminX, bmaxX, bminX, bmaxX, bminX, bmaxX, bminX, bmaxX};
    const float py[8] = {bminY, bminY, bmaxY, bmaxY, bminY, bminY, bmaxY, bmaxY};
    const float pz[8] = {bminZ, bminZ, bminZ, bminZ, bmaxZ, bmaxZ, bmaxZ, bmaxZ};

    ImVec2 sp[8]{};
    for (int i = 0; i < 8; ++i) {
        if (!WorldToScreen(view, proj, px[static_cast<size_t>(i)], py[static_cast<size_t>(i)], pz[static_cast<size_t>(i)], panel_min, panel_max,
                sp[static_cast<size_t>(i)])) {
            sp[static_cast<size_t>(i)] = ImVec2(-1e9f, -1e9f);
        }
    }
    if (screenScalePad > 1e-4f) {
        const float cx = 0.5f * (bminX + bmaxX);
        const float cy = 0.5f * (bminY + bmaxY);
        const float cz = 0.5f * (bminZ + bmaxZ);
        ImVec2 csp{};
        if (WorldToScreen(view, proj, cx, cy, cz, panel_min, panel_max, csp)) {
            for (int i = 0; i < 8; ++i) {
                if (sp[static_cast<size_t>(i)].x < -1e8f) {
                    continue;
                }
                const float dx = sp[static_cast<size_t>(i)].x - csp.x;
                const float dy = sp[static_cast<size_t>(i)].y - csp.y;
                const float len = std::sqrt(dx * dx + dy * dy);
                if (len > 1e-3f) {
                    const float s = (len + screenScalePad) / len;
                    sp[static_cast<size_t>(i)].x = csp.x + dx * s;
                    sp[static_cast<size_t>(i)].y = csp.y + dy * s;
                }
            }
        }
    }
    const float t = (std::max)(thickness, 1.0f);
    for (const auto& e : kBoxEdges) {
        if (sp[static_cast<size_t>(e.a)].x < -1e8f || sp[static_cast<size_t>(e.b)].x < -1e8f) {
            continue;
        }
        drawList->AddLine(sp[static_cast<size_t>(e.a)], sp[static_cast<size_t>(e.b)], color, t);
    }
}

void DrawWorldAxisAlignedBoxSelectionOutlineUniformImGui(ImDrawList* drawList, const ImVec2& panel_min, const ImVec2& panel_max,
    const Mat4Col& view, const Mat4Col& proj, float cx, float cy, float cz, float halfExtent, ImU32 innerColor, float innerThickness,
    float outerPad, ImU32 outerColor, float outerThickness) {
    if (!drawList) {
        return;
    }
    const float h = halfExtent;
    DrawWorldAxisAlignedBoxWireframeImGui(drawList, panel_min, panel_max, view, proj, cx - h, cy - h, cz - h, cx + h, cy + h, cz + h,
        outerColor, outerThickness, outerPad);
    DrawWorldAxisAlignedBoxWireframeImGui(drawList, panel_min, panel_max, view, proj, cx - h, cy - h, cz - h, cx + h, cy + h, cz + h,
        innerColor, innerThickness, 0.35f);
}

void DrawScreenTransformGizmo(ImDrawList* drawList, const ImVec2& center, TransformTool activeTool, TransformAxis activeAxis,
    float axisLengthPx, float rotateRingRadiusPx) {
    if (!drawList) {
        return;
    }
    const float axisLen = (std::max)(12.0f, axisLengthPx);
    const float ringR = (std::max)(8.0f, rotateRingRadiusPx);
    const ImU32 colX = (activeAxis == TransformAxis::X) ? IM_COL32(255, 145, 145, 255) : IM_COL32(235, 95, 95, 240);
    const ImU32 colY = (activeAxis == TransformAxis::Y) ? IM_COL32(145, 245, 165, 255) : IM_COL32(105, 220, 120, 240);
    const ImU32 colZ = (activeAxis == TransformAxis::Z) ? IM_COL32(145, 195, 255, 255) : IM_COL32(105, 165, 255, 240);
    const ImU32 colCenter = IM_COL32(245, 220, 140, 245);
    const float thick = (activeTool == TransformTool::Translate) ? 2.6f : 1.7f;

    drawList->AddLine(center, ImVec2(center.x + axisLen, center.y), colX, thick);
    drawList->AddLine(center, ImVec2(center.x, center.y - axisLen), colY, thick);
    drawList->AddLine(center, ImVec2(center.x - axisLen * 0.7f, center.y + axisLen * 0.7f), colZ, 1.5f);
    drawList->AddCircleFilled(center, 4.2f, colCenter, 16);
    drawList->AddCircle(center, 4.2f, IM_COL32(22, 18, 10, 220), 16, 1.1f);

    if (activeTool == TransformTool::Rotate) {
        drawList->AddCircle(center, ringR, IM_COL32(255, 212, 125, 245), 0, 2.2f);
    } else if (activeTool == TransformTool::Scale) {
        const float s = 4.2f;
        drawList->AddRectFilled(ImVec2(center.x + axisLen - s, center.y - s), ImVec2(center.x + axisLen + s, center.y + s), colX);
        drawList->AddRectFilled(ImVec2(center.x - s, center.y - axisLen - s), ImVec2(center.x + s, center.y - axisLen + s), colY);
        drawList->AddRectFilled(ImVec2(center.x - axisLen * 0.7f - s, center.y + axisLen * 0.7f - s),
            ImVec2(center.x - axisLen * 0.7f + s, center.y + axisLen * 0.7f + s), colZ);
    }
}

void DrawWorldTransformGizmo(ImDrawList* drawList, const ImVec2& panel_min, const ImVec2& panel_max, const Mat4Col& view,
    const Mat4Col& proj, float wx, float wy, float wz, TransformTool activeTool, TransformAxis activeAxis, float axisLengthWorld,
    float rotateRingRadiusPx) {
    if (!drawList) {
        return;
    }
    ImVec2 c{};
    if (!WorldToScreen(view, proj, wx, wy, wz, panel_min, panel_max, c)) {
        return;
    }
    const float wlen = (std::max)(0.01f, axisLengthWorld);
    ImVec2 px{}, py{}, pz{};
    const bool hasX = WorldToScreen(view, proj, wx + wlen, wy, wz, panel_min, panel_max, px);
    const bool hasY = WorldToScreen(view, proj, wx, wy + wlen, wz, panel_min, panel_max, py);
    const bool hasZ = WorldToScreen(view, proj, wx, wy, wz + wlen, panel_min, panel_max, pz);
    if (hasX) {
        const ImU32 cx = (activeAxis == TransformAxis::X) ? IM_COL32(255, 145, 145, 255) : IM_COL32(235, 95, 95, 245);
        drawList->AddLine(c, px, cx, (activeTool == TransformTool::Translate) ? 2.6f : 1.6f);
    }
    if (hasY) {
        const ImU32 cy = (activeAxis == TransformAxis::Y) ? IM_COL32(145, 245, 165, 255) : IM_COL32(105, 220, 120, 245);
        drawList->AddLine(c, py, cy, (activeTool == TransformTool::Translate) ? 2.6f : 1.6f);
    }
    if (hasZ) {
        const ImU32 cz = (activeAxis == TransformAxis::Z) ? IM_COL32(145, 195, 255, 255) : IM_COL32(105, 165, 255, 245);
        drawList->AddLine(c, pz, cz, (activeTool == TransformTool::Translate) ? 2.6f : 1.6f);
    }
    DrawScreenTransformGizmo(drawList, c, activeTool, activeAxis, 14.0f, rotateRingRadiusPx);
}

} // namespace LibUI::Viewport
