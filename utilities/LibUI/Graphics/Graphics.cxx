#include "LibUI/Graphics/Graphics.hxx"
#include <cmath>
#include <algorithm>

namespace LibUI::Graphics {

// Helper to check initialization
static bool CheckInitialized() {
    if (!Core::IsInitialized()) {
        return false;
    }
    ImGui::SetCurrentContext(Core::GetContext());
    return true;
}

// Draw list access
ImDrawList* GetDrawList() {
    if (!CheckInitialized()) return nullptr;
    try {
        return ImGui::GetWindowDrawList();
    } catch (...) {
        return nullptr;
    }
}

ImDrawList* GetBackgroundDrawList() {
    if (!CheckInitialized()) return nullptr;
    try {
        return ImGui::GetBackgroundDrawList();
    } catch (...) {
        return nullptr;
    }
}

// Drawing primitives
void DrawLine(const ImVec2& p1, const ImVec2& p2, ImU32 col, float thickness) {
    ImDrawList* draw_list = GetBackgroundDrawList();
    if (!draw_list) return;
    try {
        draw_list->AddLine(p1, p2, col, thickness);
    } catch (...) {
    }
}

void DrawRect(const ImVec2& p_min, const ImVec2& p_max, ImU32 col, float rounding, ImDrawFlags flags, float thickness) {
    ImDrawList* draw_list = GetBackgroundDrawList();
    if (!draw_list) return;
    try {
        draw_list->AddRect(p_min, p_max, col, rounding, flags, thickness);
    } catch (...) {
    }
}

void DrawRectFilled(const ImVec2& p_min, const ImVec2& p_max, ImU32 col, float rounding, ImDrawFlags flags) {
    ImDrawList* draw_list = GetBackgroundDrawList();
    if (!draw_list) return;
    try {
        draw_list->AddRectFilled(p_min, p_max, col, rounding, flags);
    } catch (...) {
    }
}

void DrawCircle(const ImVec2& center, float radius, ImU32 col, int num_segments, float thickness) {
    ImDrawList* draw_list = GetBackgroundDrawList();
    if (!draw_list) return;
    try {
        draw_list->AddCircle(center, radius, col, num_segments, thickness);
    } catch (...) {
    }
}

void DrawCircleFilled(const ImVec2& center, float radius, ImU32 col, int num_segments) {
    ImDrawList* draw_list = GetBackgroundDrawList();
    if (!draw_list) return;
    try {
        draw_list->AddCircleFilled(center, radius, col, num_segments);
    } catch (...) {
    }
}

void DrawPolyline(const ImVec2* points, int num_points, ImU32 col, ImDrawFlags flags, float thickness) {
    ImDrawList* draw_list = GetBackgroundDrawList();
    if (!draw_list || !points || num_points <= 0) return;
    try {
        draw_list->AddPolyline(points, num_points, col, flags, thickness);
    } catch (...) {
    }
}

void DrawPolylineConvex(const ImVec2* points, int num_points, ImU32 col, float thickness) {
    ImDrawList* draw_list = GetBackgroundDrawList();
    if (!draw_list || !points || num_points <= 0) return;
    try {
        draw_list->AddConvexPolyFilled(points, num_points, col);
    } catch (...) {
    }
}

void DrawPolygonFilled(const ImVec2* points, int num_points, ImU32 col) {
    ImDrawList* draw_list = GetBackgroundDrawList();
    if (!draw_list || !points || num_points <= 0) return;
    try {
        draw_list->AddConvexPolyFilled(points, num_points, col);
    } catch (...) {
    }
}

void DrawBezierCubic(const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, ImU32 col, float thickness, int num_segments) {
    ImDrawList* draw_list = GetBackgroundDrawList();
    if (!draw_list) return;
    try {
        draw_list->AddBezierCubic(p1, p2, p3, p4, col, thickness, num_segments);
    } catch (...) {
    }
}

void DrawBezierQuadratic(const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, ImU32 col, float thickness, int num_segments) {
    ImDrawList* draw_list = GetBackgroundDrawList();
    if (!draw_list) return;
    try {
        draw_list->AddBezierQuadratic(p1, p2, p3, col, thickness, num_segments);
    } catch (...) {
    }
}

// Text rendering
void DrawText(const ImVec2& pos, ImU32 col, const char* text_begin, const char* text_end) {
    ImDrawList* draw_list = GetBackgroundDrawList();
    if (!draw_list || !text_begin) return;
    try {
        draw_list->AddText(pos, col, text_begin, text_end);
    } catch (...) {
    }
}

void DrawTextCentered(const ImVec2& center, ImU32 col, const char* text) {
    if (!text) return;
    ImDrawList* draw_list = GetBackgroundDrawList();
    if (!draw_list) return;
    try {
        ImVec2 text_size = ImGui::CalcTextSize(text);
        ImVec2 text_pos(center.x - text_size.x * 0.5f, center.y - text_size.y * 0.5f);
        draw_list->AddText(text_pos, col, text);
    } catch (...) {
    }
}

void DrawTextShadowed(const ImVec2& pos, ImU32 col, ImU32 shadow_col, const char* text, float shadow_offset_x, float shadow_offset_y) {
    if (!text) return;
    ImDrawList* draw_list = GetBackgroundDrawList();
    if (!draw_list) return;
    try {
        // Draw shadow first
        ImVec2 shadow_pos(pos.x + shadow_offset_x, pos.y + shadow_offset_y);
        draw_list->AddText(shadow_pos, shadow_col, text);
        // Draw text on top
        draw_list->AddText(pos, col, text);
    } catch (...) {
    }
}

void DrawTextWrapped(const ImVec2& pos, ImU32 col, const char* text, float wrap_width) {
    if (!text) return;
    ImDrawList* draw_list = GetBackgroundDrawList();
    if (!draw_list) return;
    try {
        // Simple wrapping - split by spaces and draw line by line
        std::string text_str(text);
        float y = pos.y;
        float line_height = ImGui::GetTextLineHeight();

        size_t start = 0;
        while (start < text_str.length()) {
            size_t end = start;
            float line_width = 0.0f;

            // Find where to break the line
            while (end < text_str.length() && text_str[end] != '\n') {
                char c = text_str[end];
                if (c == ' ' && line_width > wrap_width && end > start) {
                    break;
                }
                line_width += ImGui::CalcTextSize(&c, &c + 1).x;
                end++;
            }

            // Draw this line
            std::string line = text_str.substr(start, end - start);
            draw_list->AddText(ImVec2(pos.x, y), col, line.c_str());

            y += line_height;
            start = (end < text_str.length() && text_str[end] == '\n') ? end + 1 : end;
            if (start < text_str.length() && text_str[start] == ' ') {
                start++; // Skip space at start of next line
            }
        }
    } catch (...) {
    }
}

// Image rendering
void DrawImage(ImTextureID user_texture_id, const ImVec2& p_min, const ImVec2& p_max, const ImVec2& uv_min, const ImVec2& uv_max, ImU32 col) {
    ImDrawList* draw_list = GetBackgroundDrawList();
    if (!draw_list) return;
    try {
        draw_list->AddImage(user_texture_id, p_min, p_max, uv_min, uv_max, col);
    } catch (...) {
    }
}

void DrawImageScaled(ImTextureID user_texture_id, const ImVec2& pos, const ImVec2& size, const ImVec2& uv_min, const ImVec2& uv_max, ImU32 col) {
    ImDrawList* draw_list = GetBackgroundDrawList();
    if (!draw_list) return;
    try {
        ImVec2 p_max(pos.x + size.x, pos.y + size.y);
        draw_list->AddImage(user_texture_id, pos, p_max, uv_min, uv_max, col);
    } catch (...) {
    }
}

void DrawImageRotated(ImTextureID user_texture_id, const ImVec2& center, const ImVec2& size, float angle, const ImVec2& uv_min, const ImVec2& uv_max, ImU32 col) {
    ImDrawList* draw_list = GetBackgroundDrawList();
    if (!draw_list) return;
    try {
        // Calculate rotated corners
        float cos_a = std::cos(angle);
        float sin_a = std::sin(angle);
        ImVec2 half_size(size.x * 0.5f, size.y * 0.5f);

        ImVec2 corners[4] = {
            ImVec2(-half_size.x, -half_size.y),
            ImVec2(half_size.x, -half_size.y),
            ImVec2(half_size.x, half_size.y),
            ImVec2(-half_size.x, half_size.y)
        };

        for (int i = 0; i < 4; i++) {
            float x = corners[i].x * cos_a - corners[i].y * sin_a;
            float y = corners[i].x * sin_a + corners[i].y * cos_a;
            corners[i] = ImVec2(center.x + x, center.y + y);
        }

        draw_list->AddImageQuad(user_texture_id, corners[0], corners[1], corners[2], corners[3],
                                ImVec2(uv_min.x, uv_min.y), ImVec2(uv_max.x, uv_min.y),
                                ImVec2(uv_max.x, uv_max.y), ImVec2(uv_min.x, uv_max.y), col);
    } catch (...) {
    }
}

void DrawImageTiled(ImTextureID user_texture_id, const ImVec2& p_min, const ImVec2& p_max, const ImVec2& uv_min, const ImVec2& uv_max, ImU32 col, float tile_size) {
    ImDrawList* draw_list = GetBackgroundDrawList();
    if (!draw_list) return;
    try {
        // Draw tiled pattern
        for (float y = p_min.y; y < p_max.y; y += tile_size) {
            for (float x = p_min.x; x < p_max.x; x += tile_size) {
                ImVec2 tile_max(std::min(x + tile_size, p_max.x), std::min(y + tile_size, p_max.y));
                draw_list->AddImage(user_texture_id, ImVec2(x, y), tile_max, uv_min, uv_max, col);
            }
        }
    } catch (...) {
    }
}

// Color utilities
ImU32 Color(float r, float g, float b, float a) {
    r = std::max(0.0f, std::min(1.0f, r));
    g = std::max(0.0f, std::min(1.0f, g));
    b = std::max(0.0f, std::min(1.0f, b));
    a = std::max(0.0f, std::min(1.0f, a));
    return IM_COL32((int)(r * 255), (int)(g * 255), (int)(b * 255), (int)(a * 255));
}

ImU32 ColorHSV(float h, float s, float v, float a) {
    float r, g, b;
    ColorConvertHSVtoRGB(h, s, v, r, g, b);
    return Color(r, g, b, a);
}

ImVec4 ColorLerp(const ImVec4& a, const ImVec4& b, float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    return ImVec4(
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
        a.w + (b.w - a.w) * t
    );
}

ImU32 ColorAlpha(ImU32 col, float alpha) {
    alpha = std::max(0.0f, std::min(1.0f, alpha));
    int a = (int)(alpha * 255);
    return (col & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
}

void ColorConvertRGBtoHSV(float r, float g, float b, float& out_h, float& out_s, float& out_v) {
    float min_val = std::min({r, g, b});
    float max_val = std::max({r, g, b});
    float delta = max_val - min_val;

    out_v = max_val;

    if (delta < 0.00001f) {
        out_s = 0.0f;
        out_h = 0.0f;
        return;
    }

    out_s = delta / max_val;

    if (max_val == r) {
        out_h = 60.0f * (((g - b) / delta) + (g < b ? 6.0f : 0.0f));
    } else if (max_val == g) {
        out_h = 60.0f * (((b - r) / delta) + 2.0f);
    } else {
        out_h = 60.0f * (((r - g) / delta) + 4.0f);
    }

    if (out_h < 0.0f) out_h += 360.0f;
    out_h /= 360.0f;
}

void ColorConvertHSVtoRGB(float h, float s, float v, float& out_r, float& out_g, float& out_b) {
    h = std::fmod(h * 360.0f, 360.0f);
    if (h < 0.0f) h += 360.0f;
    h /= 60.0f;

    int i = (int)std::floor(h);
    float f = h - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - s * f);
    float t = v * (1.0f - s * (1.0f - f));

    switch (i) {
        case 0: out_r = v; out_g = t; out_b = p; break;
        case 1: out_r = q; out_g = v; out_b = p; break;
        case 2: out_r = p; out_g = v; out_b = t; break;
        case 3: out_r = p; out_g = q; out_b = v; break;
        case 4: out_r = t; out_g = p; out_b = v; break;
        default: out_r = v; out_g = p; out_b = q; break;
    }
}

// Editor-specific drawing
void DrawGrid(const ImVec2& canvas_p0, const ImVec2& canvas_p1, float grid_size, ImU32 grid_color, float thickness) {
    ImDrawList* draw_list = GetBackgroundDrawList();
    if (!draw_list || grid_size <= 0.0f) return;
    try {
        // Draw vertical lines
        for (float x = canvas_p0.x; x <= canvas_p1.x; x += grid_size) {
            draw_list->AddLine(ImVec2(x, canvas_p0.y), ImVec2(x, canvas_p1.y), grid_color, thickness);
        }
        // Draw horizontal lines
        for (float y = canvas_p0.y; y <= canvas_p1.y; y += grid_size) {
            draw_list->AddLine(ImVec2(canvas_p0.x, y), ImVec2(canvas_p1.x, y), grid_color, thickness);
        }
    } catch (...) {
    }
}

void DrawViewportBorder(const ImVec2& p_min, const ImVec2& p_max, ImU32 border_color, float thickness) {
    ImDrawList* draw_list = GetBackgroundDrawList();
    if (!draw_list) return;
    try {
        draw_list->AddRect(p_min, p_max, border_color, 0.0f, 0, thickness);
    } catch (...) {
    }
}

void DrawSelectionRect(const ImVec2& p_min, const ImVec2& p_max, ImU32 fill_color, ImU32 border_color, float border_thickness) {
    ImDrawList* draw_list = GetBackgroundDrawList();
    if (!draw_list) return;
    try {
        // Draw filled rectangle
        draw_list->AddRectFilled(p_min, p_max, fill_color);
        // Draw border
        draw_list->AddRect(p_min, p_max, border_color, 0.0f, 0, border_thickness);
    } catch (...) {
    }
}

} // namespace LibUI::Graphics

