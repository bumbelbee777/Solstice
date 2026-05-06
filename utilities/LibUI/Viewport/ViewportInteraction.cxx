#include "LibUI/Viewport/ViewportInteraction.hxx"

#include <algorithm>
#include <cmath>

namespace LibUI::Viewport {

bool PickClosestMergedAlongRay(float rayOriginX, float rayOriginY, float rayOriginZ, float rayDirX, float rayDirY,
    float rayDirZ, const LibUI::Tools::AxisAlignedBox3* boxes, const PickToken* tokens, int count, PickToken& outToken,
    float& outT) {
    outToken = kPickTokenNone;
    outT = 0.f;
    if (!boxes || !tokens || count <= 0) {
        return false;
    }
    float bestT = 1.0e30f;
    PickToken bestTok = kPickTokenNone;
    for (int i = 0; i < count; ++i) {
        const LibUI::Tools::AxisAlignedBox3& b = boxes[i];
        if (b.minX > b.maxX || b.minY > b.maxY || b.minZ > b.maxZ) {
            continue;
        }
        float th = 0.f;
        if (!IntersectRayAxisAlignedBox(rayOriginX, rayOriginY, rayOriginZ, rayDirX, rayDirY, rayDirZ, b.minX, b.minY,
                b.minZ, b.maxX, b.maxY, b.maxZ, th)) {
            continue;
        }
        if (th < bestT) {
            bestT = th;
            bestTok = tokens[i];
        }
    }
    if (bestTok == kPickTokenNone) {
        return false;
    }
    outToken = bestTok;
    outT = bestT;
    return true;
}

bool ScreenMarqueeIntersectsWorldAabb(const ImVec2& panel_min, const ImVec2& panel_max, const Mat4Col& view,
    const Mat4Col& proj, float bminX, float bminY, float bminZ, float bmaxX, float bmaxY, float bmaxZ,
    const ImVec2& rectMin, const ImVec2& rectMax) {
    const float rx0 = (std::min)(rectMin.x, rectMax.x);
    const float ry0 = (std::min)(rectMin.y, rectMax.y);
    const float rx1 = (std::max)(rectMin.x, rectMax.x);
    const float ry1 = (std::max)(rectMin.y, rectMax.y);
    if (rx1 - rx0 < 1e-4f || ry1 - ry0 < 1e-4f) {
        return false;
    }

    const float px[8] = {bminX, bmaxX, bmaxX, bminX, bminX, bmaxX, bmaxX, bminX};
    const float py[8] = {bminY, bminY, bmaxY, bmaxY, bminY, bminY, bmaxY, bmaxY};
    const float pz[8] = {bminZ, bminZ, bminZ, bminZ, bmaxZ, bmaxZ, bmaxZ, bmaxZ};

    float sx0 = 1.0e30f;
    float sy0 = 1.0e30f;
    float sx1 = -1.0e30f;
    float sy1 = -1.0e30f;
    int valid = 0;
    for (int i = 0; i < 8; ++i) {
        ImVec2 sp{};
        if (!WorldToScreen(view, proj, px[i], py[i], pz[i], panel_min, panel_max, sp)) {
            continue;
        }
        ++valid;
        sx0 = (std::min)(sx0, sp.x);
        sy0 = (std::min)(sy0, sp.y);
        sx1 = (std::max)(sx1, sp.x);
        sy1 = (std::max)(sy1, sp.y);
    }
    if (valid == 0) {
        return false;
    }

    // Axis-aligned overlap of projected hull vs marquee.
    if (sx1 < rx0 || sx0 > rx1 || sy1 < ry0 || sy0 > ry1) {
        return false;
    }
    return true;
}

bool PickUvRectNormalized(float u, float v, float u0, float v0, float u1, float v1) {
    const float a = (std::min)(u0, u1);
    const float b = (std::max)(u0, u1);
    const float c = (std::min)(v0, v1);
    const float d = (std::max)(v0, v1);
    return u >= a && u <= b && v >= c && v <= d;
}

const char* TransformToolLabel(TransformTool tool) {
    switch (tool) {
    case TransformTool::Translate:
        return "Move";
    case TransformTool::Rotate:
        return "Rotate";
    case TransformTool::Scale:
        return "Resize";
    default:
        return "Move";
    }
}

const char* TransformAxisLabel(TransformAxis axis) {
    switch (axis) {
    case TransformAxis::None:
        return "Free";
    case TransformAxis::X:
        return "X";
    case TransformAxis::Y:
        return "Y";
    case TransformAxis::Z:
        return "Z";
    default:
        return "Free";
    }
}

const char* TransformToolUsageHint(bool includeShiftSnap) {
    return includeShiftSnap ? "W move · R rotate · E resize (Shift snap)" : "W move · R rotate · E resize";
}

bool ApplyStandardTransformToolHotkeys(TransformTool& ioTool, bool viewportHovered) {
    if (!viewportHovered) {
        return false;
    }
    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput) {
        return false;
    }
    TransformTool old = ioTool;
    if (ImGui::IsKeyPressed(ImGuiKey_W, false)) {
        ioTool = TransformTool::Translate;
    } else if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
        ioTool = TransformTool::Rotate;
    } else if (ImGui::IsKeyPressed(ImGuiKey_E, false)) {
        ioTool = TransformTool::Scale;
    }
    return old != ioTool;
}

bool ApplyStandardTransformAxisHotkeys(TransformAxis& ioAxis, bool viewportHovered) {
    if (!viewportHovered) {
        return false;
    }
    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput) {
        return false;
    }
    TransformAxis old = ioAxis;
    if (ImGui::IsKeyPressed(ImGuiKey_X, false)) {
        ioAxis = TransformAxis::X;
    } else if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
        ioAxis = TransformAxis::Y;
    } else if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
        ioAxis = TransformAxis::Z;
    } else if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        ioAxis = TransformAxis::None;
    }
    return old != ioAxis;
}

float ComputeTransformRotateDeltaDeg(float mouseDxPixels, bool snapEnabled, const TransformToolTuning& tuning) {
    float d = mouseDxPixels * tuning.rotateDegPerPixel;
    if (snapEnabled && tuning.rotateSnapDegrees > 1e-6f) {
        d = std::round(d / tuning.rotateSnapDegrees) * tuning.rotateSnapDegrees;
    }
    return d;
}

float ComputeTransformScaleDelta(float mouseDxPixels, bool snapEnabled, const TransformToolTuning& tuning) {
    float d = mouseDxPixels * tuning.scalePerPixel;
    if (snapEnabled && tuning.scaleSnapStep > 1e-6f) {
        d = std::round(d / tuning.scaleSnapStep) * tuning.scaleSnapStep;
    }
    return d;
}

bool BeginViewportContextPopup(const char* str_id, bool viewportHovered, ImGuiPopupFlags extra_flags) {
    if (!str_id || !str_id[0]) {
        return false;
    }
    // Allow drawing an already-open menu after the cursor leaves the viewport; fresh opens require hover + RMB.
    if (!viewportHovered && !ImGui::IsPopupOpen(str_id, ImGuiPopupFlags_None)) {
        return false;
    }
    return ImGui::BeginPopupContextWindow(str_id, ImGuiPopupFlags_MouseButtonRight | extra_flags);
}

void DrawMarqueeRect(ImDrawList* dl, const ImVec2& a, const ImVec2& b, ImU32 fillCol, ImU32 borderCol, float borderThick) {
    if (!dl) {
        return;
    }
    const float x0 = (std::min)(a.x, b.x);
    const float y0 = (std::min)(a.y, b.y);
    const float x1 = (std::max)(a.x, b.x);
    const float y1 = (std::max)(a.y, b.y);
    dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), fillCol);
    dl->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), borderCol, 0.f, 0, borderThick);
}

} // namespace LibUI::Viewport
