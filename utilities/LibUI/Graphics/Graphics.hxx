#pragma once

#include "LibUI/Core/Core.hxx"
#include <imgui.h>
#include <string>

namespace LibUI::Graphics {

// Draw list access
LIBUI_API ImDrawList* GetDrawList();
LIBUI_API ImDrawList* GetBackgroundDrawList();

// Drawing primitives
LIBUI_API void DrawLine(const ImVec2& p1, const ImVec2& p2, ImU32 col, float thickness = 1.0f);
LIBUI_API void DrawRect(const ImVec2& p_min, const ImVec2& p_max, ImU32 col, float rounding = 0.0f, ImDrawFlags flags = 0, float thickness = 1.0f);
LIBUI_API void DrawRectFilled(const ImVec2& p_min, const ImVec2& p_max, ImU32 col, float rounding = 0.0f, ImDrawFlags flags = 0);
LIBUI_API void DrawCircle(const ImVec2& center, float radius, ImU32 col, int num_segments = 0, float thickness = 1.0f);
LIBUI_API void DrawCircleFilled(const ImVec2& center, float radius, ImU32 col, int num_segments = 0);
LIBUI_API void DrawPolyline(const ImVec2* points, int num_points, ImU32 col, ImDrawFlags flags, float thickness);
LIBUI_API void DrawPolylineConvex(const ImVec2* points, int num_points, ImU32 col, float thickness);
LIBUI_API void DrawPolygonFilled(const ImVec2* points, int num_points, ImU32 col);
LIBUI_API void DrawBezierCubic(const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, ImU32 col, float thickness, int num_segments = 0);
LIBUI_API void DrawBezierQuadratic(const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, ImU32 col, float thickness, int num_segments = 0);

// Text rendering
LIBUI_API void DrawText(const ImVec2& pos, ImU32 col, const char* text_begin, const char* text_end = nullptr);
LIBUI_API void DrawTextCentered(const ImVec2& center, ImU32 col, const char* text);
LIBUI_API void DrawTextShadowed(const ImVec2& pos, ImU32 col, ImU32 shadow_col, const char* text, float shadow_offset_x = 1.0f, float shadow_offset_y = 1.0f);
LIBUI_API void DrawTextWrapped(const ImVec2& pos, ImU32 col, const char* text, float wrap_width);

// Image rendering (requires texture handle)
LIBUI_API void DrawImage(ImTextureID user_texture_id, const ImVec2& p_min, const ImVec2& p_max, const ImVec2& uv_min = ImVec2(0, 0), const ImVec2& uv_max = ImVec2(1, 1), ImU32 col = IM_COL32_WHITE);
LIBUI_API void DrawImageScaled(ImTextureID user_texture_id, const ImVec2& pos, const ImVec2& size, const ImVec2& uv_min = ImVec2(0, 0), const ImVec2& uv_max = ImVec2(1, 1), ImU32 col = IM_COL32_WHITE);
LIBUI_API void DrawImageRotated(ImTextureID user_texture_id, const ImVec2& center, const ImVec2& size, float angle, const ImVec2& uv_min = ImVec2(0, 0), const ImVec2& uv_max = ImVec2(1, 1), ImU32 col = IM_COL32_WHITE);
LIBUI_API void DrawImageTiled(ImTextureID user_texture_id, const ImVec2& p_min, const ImVec2& p_max, const ImVec2& uv_min, const ImVec2& uv_max, ImU32 col = IM_COL32_WHITE, float tile_size = 32.0f);

// Color utilities
LIBUI_API ImU32 Color(float r, float g, float b, float a = 1.0f);
LIBUI_API ImU32 ColorHSV(float h, float s, float v, float a = 1.0f);
LIBUI_API ImVec4 ColorLerp(const ImVec4& a, const ImVec4& b, float t);
LIBUI_API ImU32 ColorAlpha(ImU32 col, float alpha);
LIBUI_API void ColorConvertRGBtoHSV(float r, float g, float b, float& out_h, float& out_s, float& out_v);
LIBUI_API void ColorConvertHSVtoRGB(float h, float s, float v, float& out_r, float& out_g, float& out_b);

// Editor-specific drawing
LIBUI_API void DrawGrid(const ImVec2& canvas_p0, const ImVec2& canvas_p1, float grid_size, ImU32 grid_color, float thickness = 1.0f);
LIBUI_API void DrawViewportBorder(const ImVec2& p_min, const ImVec2& p_max, ImU32 border_color, float thickness = 2.0f);
LIBUI_API void DrawSelectionRect(const ImVec2& p_min, const ImVec2& p_max, ImU32 fill_color, ImU32 border_color, float border_thickness = 1.0f);

} // namespace LibUI::Graphics

