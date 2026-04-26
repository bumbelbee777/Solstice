#include "LibUI/Viewport/Viewport.hxx"

#include <algorithm>
#include <cmath>

namespace LibUI::Viewport {

static bool CheckInitialized() {
    if (!Core::IsInitialized()) {
        return false;
    }
    ImGui::SetCurrentContext(Core::GetContext());
    return true;
}

bool BeginHost(const char* str_id, const ImVec2& size, bool border) {
    if (!CheckInitialized() || !str_id) {
        return false;
    }
    try {
        // BeginChild may return false when clipped/collapsed; ImGui still requires EndChild() every time (see imgui.h).
        // Do not return that bool to callers — skipping EndHost() corrupts the window stack (End vs EndChild mismatch).
        ImGui::BeginChild(str_id, size, border ? ImGuiChildFlags_Borders : ImGuiChildFlags_None,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings);
        return true;
    } catch (...) {
        return false;
    }
}

bool PollFrame(Frame& out) {
    if (!CheckInitialized()) {
        return false;
    }
    try {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        if (avail.x < 1.0f) {
            avail.x = 1.0f;
        }
        if (avail.y < 1.0f) {
            avail.y = 1.0f;
        }
        ImGui::InvisibleButton("##libui_vp_host", avail);
        out.hovered = ImGui::IsItemHovered();
        out.active = ImGui::IsItemActive();
        out.min = ImGui::GetItemRectMin();
        out.max = ImGui::GetItemRectMax();
        out.size = ImVec2(out.max.x - out.min.x, out.max.y - out.min.y);
        out.draw_list = ImGui::GetWindowDrawList();
        ImGuiIO& io = ImGui::GetIO();
        out.mouse_delta = out.hovered ? io.MouseDelta : ImVec2(0.0f, 0.0f);
        out.mouse_wheel = out.hovered ? io.MouseWheel : 0.0f;
        return true;
    } catch (...) {
        return false;
    }
}

void EndHost() {
    if (!CheckInitialized()) {
        return;
    }
    try {
        ImGui::EndChild();
    } catch (...) {
    }
}

void ComputeLetterbox(const ImVec2& panel_min, const ImVec2& panel_max, float content_w, float content_h,
                      ImVec2& out_min, ImVec2& out_max) {
    const float pw = panel_max.x - panel_min.x;
    const float ph = panel_max.y - panel_min.y;
    if (pw <= 0.0f || ph <= 0.0f || content_w <= 0.0f || content_h <= 0.0f) {
        out_min = panel_min;
        out_max = panel_max;
        return;
    }
    const float ar = content_w / content_h;
    const float par = pw / ph;
    float rw = pw;
    float rh = ph;
    if (par > ar) {
        rh = ph;
        rw = rh * ar;
    } else {
        rw = pw;
        rh = rw / ar;
    }
    const float ox = panel_min.x + (pw - rw) * 0.5f;
    const float oy = panel_min.y + (ph - rh) * 0.5f;
    out_min = ImVec2(ox, oy);
    out_max = ImVec2(ox + rw, oy + rh);
}

void DrawCheckerboard(ImDrawList* dl, const ImVec2& p_min, const ImVec2& p_max, float cell_px, ImU32 col_a,
                      ImU32 col_b) {
    if (!dl || cell_px <= 0.0f) {
        return;
    }
    dl->PushClipRect(p_min, p_max, true);
    const int x0 = static_cast<int>(std::floor(p_min.x / cell_px));
    const int y0 = static_cast<int>(std::floor(p_min.y / cell_px));
    const int x1 = static_cast<int>(std::ceil(p_max.x / cell_px));
    const int y1 = static_cast<int>(std::ceil(p_max.y / cell_px));
    for (int gy = y0; gy < y1; ++gy) {
        for (int gx = x0; gx < x1; ++gx) {
            const float fx = static_cast<float>(gx) * cell_px;
            const float fy = static_cast<float>(gy) * cell_px;
            ImVec2 a(std::max(fx, p_min.x), std::max(fy, p_min.y));
            ImVec2 b(std::min(fx + cell_px, p_max.x), std::min(fy + cell_px, p_max.y));
            if (b.x <= a.x || b.y <= a.y) {
                continue;
            }
            const ImU32 col = ((gx + gy) & 1) ? col_b : col_a;
            dl->AddRectFilled(a, b, col);
        }
    }
    dl->PopClipRect();
}

void DrawTextureLetterboxed(ImDrawList* dl, ImTextureID tex, const ImVec2& panel_min, const ImVec2& panel_max,
                            float tex_w, float tex_h, ImU32 tint) {
    if (!dl || !tex) {
        return;
    }
    ImVec2 a;
    ImVec2 b;
    ComputeLetterbox(panel_min, panel_max, tex_w, tex_h, a, b);
    dl->AddImage(tex, a, b, ImVec2(0, 0), ImVec2(1, 1), tint);
}

bool ScreenToLetterboxUv(const ImVec2& screen_pos, const ImVec2& panel_min, const ImVec2& panel_max, float tex_w,
                         float tex_h, float& out_u, float& out_v) {
    ImVec2 lbmin;
    ImVec2 lbmax;
    ComputeLetterbox(panel_min, panel_max, tex_w, tex_h, lbmin, lbmax);
    if (screen_pos.x < lbmin.x || screen_pos.x > lbmax.x || screen_pos.y < lbmin.y || screen_pos.y > lbmax.y) {
        return false;
    }
    const float rw = lbmax.x - lbmin.x;
    const float rh = lbmax.y - lbmin.y;
    if (rw <= 0.0f || rh <= 0.0f) {
        return false;
    }
    out_u = (screen_pos.x - lbmin.x) / rw;
    out_v = (screen_pos.y - lbmin.y) / rh;
    return true;
}

void ApplyOrbitPanZoom(OrbitPanZoomState& state, const Frame& frame, const OrbitPanZoomParams& params) {
    if (!frame.hovered) {
        return;
    }
    ImGuiIO& io = ImGui::GetIO();

    if (std::abs(frame.mouse_wheel) > 1.0e-6f) {
        const float factor = std::exp(-frame.mouse_wheel * params.zoom_speed);
        state.distance = std::clamp(state.distance * factor, params.min_distance, params.max_distance);
    }

    const int orbitBtn = std::clamp(params.orbit_mouse_button, 0, ImGuiMouseButton_COUNT - 1);
    const bool orbit_drag = ImGui::IsMouseDragging(orbitBtn, 0.0f);
    const bool left_drag = ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f);
    const bool mid_drag = ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f);
    const bool alt = io.KeyAlt;

    const bool persp = state.projection == OrbitProjectionMode::Perspective;
    if (persp && orbit_drag && !alt && !io.KeyCtrl) {
        state.yaw += frame.mouse_delta.x * params.rotate_speed;
        state.pitch += frame.mouse_delta.y * params.rotate_speed;
        state.pitch = std::clamp(state.pitch, params.min_pitch, params.max_pitch);
    } else if ((left_drag && alt) || mid_drag) {
        const float s = params.pan_speed * state.distance;
        state.pan_x += frame.mouse_delta.x * s;
        state.pan_y += frame.mouse_delta.y * s;
    }
}

void ResetOrbitPanZoom(OrbitPanZoomState& state) {
    state.yaw = 0.f;
    state.pitch = 0.35f;
    state.distance = 8.f;
    state.pan_x = 0.f;
    state.pan_y = 0.f;
    state.projection = OrbitProjectionMode::Perspective;
}

void FocusOrbitOnTarget(OrbitPanZoomState& state, float worldX, float worldY, float worldZ, float targetX, float targetY,
    float targetZ) {
    state.pan_x = worldX - targetX;
    state.pan_y = worldY - targetY;
    (void)worldZ;
    (void)targetZ;
}

void DrawViewportLabel(ImDrawList* dl, const ImVec2& panel_min, const ImVec2& panel_max, const char* text,
                       ImVec2 anchor) {
    if (!dl || !text || !text[0]) {
        return;
    }
    ImVec2 ts = ImGui::CalcTextSize(text);
    const float pad = 6.0f;
    const float inner_w = panel_max.x - panel_min.x - 2.0f * pad;
    const float inner_h = panel_max.y - panel_min.y - 2.0f * pad;
    anchor.x = std::clamp(anchor.x, 0.0f, 1.0f);
    anchor.y = std::clamp(anchor.y, 0.0f, 1.0f);
    ImVec2 pos(panel_min.x + pad + (inner_w - ts.x) * anchor.x, panel_min.y + pad + (inner_h - ts.y) * anchor.y);
    ImVec2 p0(pos.x - 4.0f, pos.y - 2.0f);
    ImVec2 p1(pos.x + ts.x + 4.0f, pos.y + ts.y + 2.0f);
    dl->AddRectFilled(p0, p1, IM_COL32(0, 0, 0, 160), 3.0f);
    dl->AddText(pos, IM_COL32(255, 255, 255, 255), text);
}

} // namespace LibUI::Viewport
