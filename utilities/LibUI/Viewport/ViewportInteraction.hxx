#pragma once

#include "LibUI/Core/Core.hxx"
#include "LibUI/Tools/ViewportSpatialPick.hxx"
#include "LibUI/Viewport/ViewportMath.hxx"
#include <cstdint>
#include <imgui.h>

namespace LibUI::Viewport {

/// Opaque pick id: high 16 bits = layer (app-defined), low 48 bits = item index (or packed id).
using PickToken = std::uint64_t;

constexpr PickToken kPickTokenNone = 0;

inline PickToken MakePickToken(std::uint16_t layer, std::uint64_t itemIndex) {
    return (static_cast<std::uint64_t>(layer) << 48) | (itemIndex & 0x0000FFFFFFFFFFFFull);
}

inline std::uint16_t PickTokenLayer(PickToken t) {
    return static_cast<std::uint16_t>(t >> 48);
}

inline std::uint64_t PickTokenItem(PickToken t) {
    return t & 0x0000FFFFFFFFFFFFull;
}

/// Ray vs many axis-aligned boxes; each box has a token. Returns closest forward hit.
LIBUI_API bool PickClosestMergedAlongRay(float rayOriginX, float rayOriginY, float rayOriginZ, float rayDirX, float rayDirY,
    float rayDirZ, const LibUI::Tools::AxisAlignedBox3* boxes, const PickToken* tokens, int count, PickToken& outToken,
    float& outT);

/// World AABB (8 corners) projected to panel; 2D rect test vs screen-space [r0,r1] (from marquee drag).
LIBUI_API bool ScreenMarqueeIntersectsWorldAabb(const ImVec2& panel_min, const ImVec2& panel_max, const Mat4Col& view,
    const Mat4Col& proj, float bminX, float bminY, float bminZ, float bmaxX, float bmaxY, float bmaxZ,
    const ImVec2& rectMin, const ImVec2& rectMax);

/// `rect` in normalized 0..1 UV space (u0,v0)-(u1,v1) inside full texture; tests `uv` from `ScreenToLetterboxUv`.
LIBUI_API bool PickUvRectNormalized(float u, float v, float u0, float v0, float u1, float v1);

/// Typical viewport transform tools (move / rotate / resize-scale).
enum class TransformTool : std::uint8_t {
    Translate = 0,
    Rotate = 1,
    Scale = 2,
};

/// Optional axis lock for transform drags.
enum class TransformAxis : std::uint8_t {
    None = 0,
    X = 1,
    Y = 2,
    Z = 3,
};

/// Sensitivity + snap settings used by drag-based viewport transform edits.
struct TransformToolTuning {
    float rotateDegPerPixel{0.35f};
    float scalePerPixel{0.01f};
    float rotateSnapDegrees{15.0f};
    float scaleSnapStep{0.10f};
};

/// Short UI label for overlays / status chips (`Move`, `Rotate`, `Resize`).
LIBUI_API const char* TransformToolLabel(TransformTool tool);
LIBUI_API const char* TransformAxisLabel(TransformAxis axis);

/// Concise user hint text for overlays (`W move · R rotate · E resize ...`).
LIBUI_API const char* TransformToolUsageHint(bool includeShiftSnap = true);

/// Standard viewport tool keybinds while hovered and not typing into text widgets:
/// W = move, R = rotate, E = resize-scale.
LIBUI_API bool ApplyStandardTransformToolHotkeys(TransformTool& ioTool, bool viewportHovered);

/// Axis hotkeys while hovered and not typing: X / Y / Z set lock, Escape clears lock.
LIBUI_API bool ApplyStandardTransformAxisHotkeys(TransformAxis& ioAxis, bool viewportHovered);

/// Mouse-drag dx (pixels) → yaw/rotation delta in degrees.
LIBUI_API float ComputeTransformRotateDeltaDeg(
    float mouseDxPixels, bool snapEnabled, const TransformToolTuning& tuning = {});

/// Mouse-drag dx (pixels) → uniform scale delta.
LIBUI_API float ComputeTransformScaleDelta(
    float mouseDxPixels, bool snapEnabled, const TransformToolTuning& tuning = {});

/// Orbit with LMB is suppressed when the current left-button stroke **began** on a selectable pick hit.
struct OrbitLeftDragSuppression {
    /// Set from hit test on `ImGui::IsMouseClicked(ImGuiMouseButton_Left)` when viewport is hovered.
    bool strokeBeganOnPick{false};

    void ClearStroke() { strokeBeganOnPick = false; }

    /// Call after `IsMouseClicked(Left)` on the viewport: pass whether a pick hit occurred at the cursor.
    void OnLeftPress(bool viewportHovered, bool pickHit) {
        if (viewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            strokeBeganOnPick = pickHit;
        }
    }

    /// After release, clear so the next click is fresh.
    void OnFrameEnd() {
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            strokeBeganOnPick = false;
        }
    }

    /// If true, pass as ``suppressLmbOrbitRotation`` to ``ApplyOrbitPanZoom`` so LMB does not orbit while the pick stroke is active.
    bool ShouldSuppressOrbitLeftDrag() const {
        return strokeBeganOnPick && ImGui::IsMouseDown(ImGuiMouseButton_Left);
    }
};

/// Open a viewport context menu on RMB; call between `BeginHost` host's `PollFrame` and `EndHost` (same window).
/// Returns true if `BeginPopup` succeeded — then draw items and `ImGui::EndPopup()`.
LIBUI_API bool BeginViewportContextPopup(const char* str_id, bool viewportHovered, ImGuiPopupFlags extra_flags = 0);

/// Draw a screen-space marquee rectangle (e.g. while `ImGui::IsMouseDown(Left)` with a chord).
LIBUI_API void DrawMarqueeRect(ImDrawList* dl, const ImVec2& a, const ImVec2& b, ImU32 fillCol, ImU32 borderCol,
    float borderThick = 1.f);

} // namespace LibUI::Viewport
