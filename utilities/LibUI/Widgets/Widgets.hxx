#pragma once

#include "LibUI/Core/Core.hxx"
#include <imgui.h>
#include <string>
#include <functional>

namespace LibUI::Widgets {

// Basic widgets
LIBUI_API bool Button(const char* label, const ImVec2& size = ImVec2(0, 0));
LIBUI_API bool SmallButton(const char* label);
LIBUI_API bool InvisibleButton(const char* str_id, const ImVec2& size);
LIBUI_API void Text(const char* text);
LIBUI_API void TextColored(const ImVec4& color, const char* text);
LIBUI_API void TextDisabled(const char* text);
LIBUI_API void TextWrapped(const char* text);
LIBUI_API void LabelText(const char* label, const char* text);
LIBUI_API void BulletText(const char* text);

// Input widgets
LIBUI_API bool InputText(const char* label, char* buf, size_t buf_size, ImGuiInputTextFlags flags = 0);
LIBUI_API bool InputTextMultiline(const char* label, char* buf, size_t buf_size, const ImVec2& size = ImVec2(0, 0), ImGuiInputTextFlags flags = 0);
LIBUI_API bool InputTextWithHint(const char* label, const char* hint, char* buf, size_t buf_size, ImGuiInputTextFlags flags = 0);
LIBUI_API bool InputFloat(const char* label, float* v, float step = 0.0f, float step_fast = 0.0f, const char* format = "%.3f", ImGuiInputTextFlags flags = 0);
LIBUI_API bool InputInt(const char* label, int* v, int step = 1, int step_fast = 100, ImGuiInputTextFlags flags = 0);
LIBUI_API bool Checkbox(const char* label, bool* v);
LIBUI_API bool RadioButton(const char* label, bool active);
LIBUI_API bool RadioButton(const char* label, int* v, int v_button);
LIBUI_API bool SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format = "%.3f", ImGuiSliderFlags flags = 0);
LIBUI_API bool SliderInt(const char* label, int* v, int v_min, int v_max, const char* format = "%d", ImGuiSliderFlags flags = 0);
LIBUI_API bool SliderFloat2(const char* label, float v[2], float v_min, float v_max, const char* format = "%.3f", ImGuiSliderFlags flags = 0);
LIBUI_API bool SliderFloat3(const char* label, float v[3], float v_min, float v_max, const char* format = "%.3f", ImGuiSliderFlags flags = 0);
LIBUI_API bool SliderFloat4(const char* label, float v[4], float v_min, float v_max, const char* format = "%.3f", ImGuiSliderFlags flags = 0);

// Advanced widgets
LIBUI_API bool Combo(const char* label, int* current_item, const char* const items[], int items_count, int popup_max_height_in_items = -1);
LIBUI_API bool ListBox(const char* label, int* current_item, const char* const items[], int items_count, int height_in_items = -1);
LIBUI_API bool Selectable(const char* label, bool selected = false, ImGuiSelectableFlags flags = 0, const ImVec2& size = ImVec2(0, 0));
LIBUI_API bool ColorEdit3(const char* label, float col[3], ImGuiColorEditFlags flags = 0);
LIBUI_API bool ColorEdit4(const char* label, float col[4], ImGuiColorEditFlags flags = 0);
LIBUI_API bool ColorPicker3(const char* label, float col[3], ImGuiColorEditFlags flags = 0);
LIBUI_API bool ColorPicker4(const char* label, float col[4], ImGuiColorEditFlags flags = 0);
LIBUI_API bool ProgressBar(float fraction, const ImVec2& size_arg = ImVec2(-1, 0), const char* overlay = nullptr);

// Layout
LIBUI_API bool BeginWindow(const char* name, bool* p_open = nullptr, ImGuiWindowFlags flags = 0);
LIBUI_API void EndWindow();
LIBUI_API bool BeginChild(const char* str_id, const ImVec2& size = ImVec2(0, 0), bool border = false, ImGuiWindowFlags flags = 0);
LIBUI_API void EndChild();
LIBUI_API void SameLine(float offset_x = 0.0f, float spacing_w = -1.0f);
LIBUI_API void NewLine();
LIBUI_API void Separator();
LIBUI_API void Spacing();
LIBUI_API void Dummy(const ImVec2& size);
LIBUI_API void Indent(float indent_w = 0.0f);
LIBUI_API void Unindent(float indent_w = 0.0f);
LIBUI_API void BeginGroup();
LIBUI_API void EndGroup();
LIBUI_API ImVec2 GetCursorPos();
LIBUI_API float GetCursorPosX();
LIBUI_API float GetCursorPosY();
LIBUI_API void SetCursorPos(const ImVec2& local_pos);
LIBUI_API void SetCursorPosX(float x);
LIBUI_API void SetCursorPosY(float y);
LIBUI_API ImVec2 GetCursorStartPos();
LIBUI_API ImVec2 GetCursorScreenPos();
LIBUI_API void SetCursorScreenPos(const ImVec2& pos);
LIBUI_API void AlignTextToFramePadding();
LIBUI_API float GetTextLineHeight();
LIBUI_API float GetTextLineHeightWithSpacing();
LIBUI_API float GetFrameHeight();
LIBUI_API float GetFrameHeightWithSpacing();
LIBUI_API void SetNextItemWidth(float item_width);
LIBUI_API float CalcItemWidth();
LIBUI_API void PushItemWidth(float item_width);
LIBUI_API void PopItemWidth();
LIBUI_API float GetItemWidth();
LIBUI_API void SetItemDefaultFocus();
LIBUI_API void SetKeyboardFocusHere(int offset = 0);

// Tree nodes
LIBUI_API bool TreeNode(const char* label);
LIBUI_API bool TreeNode(const char* str_id, const char* fmt, ...);
LIBUI_API bool TreeNodeV(const char* str_id, const char* fmt, va_list args);
LIBUI_API bool TreeNodeEx(const char* label, ImGuiTreeNodeFlags flags = 0);
LIBUI_API bool TreeNodeEx(const char* str_id, ImGuiTreeNodeFlags flags, const char* fmt, ...);
LIBUI_API void TreePush(const char* str_id = nullptr);
LIBUI_API void TreePop();
LIBUI_API float GetTreeNodeToLabelSpacing();
LIBUI_API bool CollapsingHeader(const char* label, ImGuiTreeNodeFlags flags = 0);
LIBUI_API bool CollapsingHeader(const char* label, bool* p_open, ImGuiTreeNodeFlags flags = 0);

// Tables
LIBUI_API bool BeginTable(const char* str_id, int column, ImGuiTableFlags flags = 0, const ImVec2& outer_size = ImVec2(0.0f, 0.0f), float inner_width = 0.0f);
LIBUI_API void EndTable();
LIBUI_API void TableNextRow(ImGuiTableRowFlags row_flags = 0, float min_row_height = 0.0f);
LIBUI_API bool TableNextColumn();
LIBUI_API bool TableSetColumnIndex(int column_n);
LIBUI_API void TableSetupColumn(const char* label, ImGuiTableColumnFlags flags = 0, float init_width_or_weight = 0.0f, ImGuiID user_id = 0);
LIBUI_API void TableSetupScrollFreeze(int cols, int rows);
LIBUI_API void TableHeadersRow();
LIBUI_API void TableHeader(const char* label);
LIBUI_API ImGuiTableSortSpecs* TableGetSortSpecs();
LIBUI_API int TableGetColumnCount();
LIBUI_API int TableGetColumnIndex();
LIBUI_API int TableGetRowIndex();
LIBUI_API const char* TableGetColumnName(int column_n = -1);
LIBUI_API ImGuiTableColumnFlags TableGetColumnFlags(int column_n = -1);
LIBUI_API void TableSetColumnEnabled(int column_n, bool v);
LIBUI_API void TableSetBgColor(ImGuiTableBgTarget target, ImU32 color, int column_n = -1);

// Menus
LIBUI_API bool BeginMenuBar();
LIBUI_API void EndMenuBar();
LIBUI_API bool BeginMainMenuBar();
LIBUI_API void EndMainMenuBar();
LIBUI_API bool BeginMenu(const char* label, bool enabled = true);
LIBUI_API void EndMenu();
LIBUI_API bool MenuItem(const char* label, const char* shortcut = nullptr, bool selected = false, bool enabled = true);
LIBUI_API bool MenuItem(const char* label, const char* shortcut, bool* p_selected, bool enabled = true);

// Popups
LIBUI_API void OpenPopup(const char* str_id, ImGuiPopupFlags popup_flags = 0);
LIBUI_API void OpenPopupOnItemClick(const char* str_id = nullptr, ImGuiPopupFlags popup_flags = 1);
LIBUI_API void CloseCurrentPopup();
LIBUI_API bool BeginPopup(const char* str_id, ImGuiWindowFlags flags = 0);
LIBUI_API bool BeginPopupModal(const char* name, bool* p_open = nullptr, ImGuiWindowFlags flags = 0);
LIBUI_API bool BeginPopupContextItem(const char* str_id = nullptr, ImGuiPopupFlags popup_flags = 1);
LIBUI_API bool BeginPopupContextWindow(const char* str_id = nullptr, ImGuiPopupFlags popup_flags = 1);
LIBUI_API bool BeginPopupContextVoid(const char* str_id = nullptr, ImGuiPopupFlags popup_flags = 1);
LIBUI_API void EndPopup();
LIBUI_API bool IsPopupOpen(const char* str_id, ImGuiPopupFlags flags = 0);

// Tooltips
LIBUI_API void SetTooltip(const char* fmt, ...);
LIBUI_API void BeginTooltip();
LIBUI_API void EndTooltip();

// Editor-specific helpers
LIBUI_API bool InputTextMultiline(const char* label, std::string& str, const ImVec2& size = ImVec2(0, 0), ImGuiInputTextFlags flags = 0);

} // namespace LibUI::Widgets

