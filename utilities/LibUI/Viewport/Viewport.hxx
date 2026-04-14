#pragma once

#include "LibUI/Core/Core.hxx"
#include <cstdint>
#include <imgui.h>

namespace LibUI::Viewport {

/// Screen-space rectangle and input for one viewport host (child + invisible button).
struct Frame {
    ImVec2 min{0, 0};
    ImVec2 max{0, 0};
    ImVec2 size{0, 0};
    bool hovered{false};
    /// True while the invisible host button is active (typically LMB held after press on the viewport).
    bool active{false};
    ImVec2 mouse_delta{0, 0};
    float mouse_wheel{0};
    ImDrawList* draw_list{nullptr};
};

/// Tunables for SimpleOrbitPanZoom.
struct OrbitPanZoomParams {
    float rotate_speed{0.005f};
    float pan_speed{0.003f};
    float zoom_speed{0.12f};
    float min_distance{0.25f};
    float max_distance{512.f};
    float min_pitch{-1.45f};
    float max_pitch{1.45f};
    /// ImGui mouse button index for orbit rotate (default LMB). Pan remains MMB or Alt+LMB.
    int orbit_mouse_button{0};
};

/// Perspective spherical orbit, or axis-aligned orthographic (top / front / side).
enum class OrbitProjectionMode : std::uint8_t {
    Perspective = 0,
    OrthoTop = 1,
    OrthoFront = 2,
    OrthoSide = 3,
};

/// Minimal spherical orbit + XY pan + dolly distance (radians / world units — consumers map to cameras).
struct OrbitPanZoomState {
    float yaw{0.f};
    float pitch{0.f};
    float distance{8.f};
    float pan_x{0.f};
    float pan_y{0.f};
    OrbitProjectionMode projection{OrbitProjectionMode::Perspective};
};

/// Begin a bordered child with an invisible button filling the content region. Draw using `frame` between BeginHost/EndHost.
LIBUI_API bool BeginHost(const char* str_id, const ImVec2& size = ImVec2(0, 0), bool border = true);

/// After BeginHost, call once to fill `out` (draw list + rects + hover / mouse deltas while hovered).
LIBUI_API bool PollFrame(Frame& out);

LIBUI_API void EndHost();

/// Fit `contentW`:`contentH` inside [panel_min, panel_max] with letterboxing; outputs drawable rect in screen space.
LIBUI_API void ComputeLetterbox(const ImVec2& panel_min, const ImVec2& panel_max, float content_w, float content_h,
                                ImVec2& out_min, ImVec2& out_max);

/// Checkerboard for empty RTs / no texture yet.
LIBUI_API void DrawCheckerboard(ImDrawList* dl, const ImVec2& p_min, const ImVec2& p_max, float cell_px, ImU32 col_a,
                                ImU32 col_b);

/// Draw `AddImage` in a letterboxed rect (full panel uses texture UV 0..1).
LIBUI_API void DrawTextureLetterboxed(ImDrawList* dl, ImTextureID tex, const ImVec2& panel_min, const ImVec2& panel_max,
                                      float tex_w, float tex_h, ImU32 tint = IM_COL32_WHITE);

/// Map normalized 0..1 coords inside letterbox to UV (for picking in texture space).
LIBUI_API bool ScreenToLetterboxUv(const ImVec2& screen_pos, const ImVec2& panel_min, const ImVec2& panel_max,
                                   float tex_w, float tex_h, float& out_u, float& out_v);

/// Orbit with LMB drag (or `orbit_mouse_button`), pan with MMB drag or LMB+Alt, dolly with wheel while hovered.
LIBUI_API void ApplyOrbitPanZoom(OrbitPanZoomState& state, const Frame& frame, const OrbitPanZoomParams& params = {});

/// Default orbit/pan/zoom and perspective projection (editor-style isometric-ish pitch).
LIBUI_API void ResetOrbitPanZoom(OrbitPanZoomState& state);

/// Center orbit pivot on `worldX/worldY/worldZ` relative to `targetX/targetY/targetZ` (sets pan so look-at hits the point).
LIBUI_API void FocusOrbitOnTarget(OrbitPanZoomState& state, float worldX, float worldY, float worldZ, float targetX,
    float targetY, float targetZ);

/// Corner overlay label (e.g. resolution / FPS) inside the viewport, semi-transparent background.
LIBUI_API void DrawViewportLabel(ImDrawList* dl, const ImVec2& panel_min, const ImVec2& panel_max, const char* text,
                                 ImVec2 anchor = ImVec2(1.f, 0.f));

} // namespace LibUI::Viewport
