#include "LibUI/Widgets/Widgets.hxx"
#include <cstdarg>
#include <iostream>

namespace LibUI::Widgets {

// Helper to check initialization
static bool CheckInitialized() {
    if (!Core::IsInitialized()) {
        return false;
    }
    ImGui::SetCurrentContext(Core::GetContext());
    return true;
}

// Basic widgets
bool Button(const char* label, const ImVec2& size) {
    if (!CheckInitialized() || !label) return false;
    try {
        return ImGui::Button(label, size);
    } catch (...) {
        return false;
    }
}

bool SmallButton(const char* label) {
    if (!CheckInitialized() || !label) return false;
    try {
        return ImGui::SmallButton(label);
    } catch (...) {
        return false;
    }
}

bool InvisibleButton(const char* str_id, const ImVec2& size) {
    if (!CheckInitialized() || !str_id) return false;
    try {
        return ImGui::InvisibleButton(str_id, size);
    } catch (...) {
        return false;
    }
}

void Text(const char* text) {
    if (!CheckInitialized() || !text) return;
    try {
        ImGui::Text("%s", text);
    } catch (...) {
    }
}

void TextColored(const ImVec4& color, const char* text) {
    if (!CheckInitialized() || !text) return;
    try {
        ImGui::TextColored(color, "%s", text);
    } catch (...) {
    }
}

void TextDisabled(const char* text) {
    if (!CheckInitialized() || !text) return;
    try {
        ImGui::TextDisabled("%s", text);
    } catch (...) {
    }
}

void TextWrapped(const char* text) {
    if (!CheckInitialized() || !text) return;
    try {
        ImGui::TextWrapped("%s", text);
    } catch (...) {
    }
}

void LabelText(const char* label, const char* text) {
    if (!CheckInitialized() || !label || !text) return;
    try {
        ImGui::LabelText(label, "%s", text);
    } catch (...) {
    }
}

void BulletText(const char* text) {
    if (!CheckInitialized() || !text) return;
    try {
        ImGui::BulletText("%s", text);
    } catch (...) {
    }
}

bool DrawInlineAlert(const char* id, const char* message, InlineAlertSeverity severity, bool dismissible,
    const char* dismissButtonLabel) {
    if (!CheckInitialized() || !id || !message || message[0] == '\0') {
        return false;
    }
    try {
        ImVec4 color = ImVec4(0.75f, 0.75f, 0.78f, 1.0f);
        switch (severity) {
        case InlineAlertSeverity::Info:
            color = ImVec4(0.55f, 0.75f, 1.0f, 1.0f);
            break;
        case InlineAlertSeverity::Success:
            color = ImVec4(0.5f, 0.9f, 0.5f, 1.0f);
            break;
        case InlineAlertSeverity::Warning:
            color = ImVec4(1.0f, 0.75f, 0.35f, 1.0f);
            break;
        case InlineAlertSeverity::Error:
            color = ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
            break;
        }

        ImGui::TextColored(color, "%s", message);
        if (!dismissible) {
            return true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton((std::string(dismissButtonLabel ? dismissButtonLabel : "Dismiss") + "##" + id).c_str())) {
            return false;
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool DrawFilterableVirtualTable(char* filterBuffer, size_t filterBufferSize, const FilterableVirtualTableOptions& options,
    const std::function<void()>& setupColumns, int rowCount, const std::function<void(int rowIndex)>& drawRow) {
    if (!CheckInitialized() || !filterBuffer || filterBufferSize == 0 || rowCount < 0 || !drawRow) {
        return false;
    }
    try {
        if (options.FilterInputId && options.FilterInputId[0] != '\0') {
            ImGui::InputTextWithHint(options.FilterInputId, options.FilterHint, filterBuffer, filterBufferSize);
            ImGui::SameLine();
            if (ImGui::SmallButton(options.ClearButtonLabel)) {
                filterBuffer[0] = '\0';
            }
        }

        if (!ImGui::BeginTable(options.TableId, options.ColumnCount, options.TableFlags, options.TableSize)) {
            return false;
        }
        if (setupColumns) {
            setupColumns();
        }
        ImGuiListClipper clipper;
        clipper.Begin(rowCount);
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                drawRow(row);
            }
        }
        ImGui::EndTable();
        return true;
    } catch (...) {
        return false;
    }
}

// Input widgets
bool InputText(const char* label, char* buf, size_t buf_size, ImGuiInputTextFlags flags) {
    if (!CheckInitialized() || !label || !buf || buf_size == 0) return false;
    try {
        return ImGui::InputText(label, buf, buf_size, flags);
    } catch (...) {
        return false;
    }
}

bool InputTextMultiline(const char* label, char* buf, size_t buf_size, const ImVec2& size, ImGuiInputTextFlags flags) {
    if (!CheckInitialized() || !label || !buf || buf_size == 0) return false;
    try {
        return ImGui::InputTextMultiline(label, buf, buf_size, size, flags);
    } catch (...) {
        return false;
    }
}

bool InputTextWithHint(const char* label, const char* hint, char* buf, size_t buf_size, ImGuiInputTextFlags flags) {
    if (!CheckInitialized() || !label || !hint || !buf || buf_size == 0) return false;
    try {
        return ImGui::InputTextWithHint(label, hint, buf, buf_size, flags);
    } catch (...) {
        return false;
    }
}

bool InputFloat(const char* label, float* v, float step, float step_fast, const char* format, ImGuiInputTextFlags flags) {
    if (!CheckInitialized() || !label || !v) return false;
    try {
        return ImGui::InputFloat(label, v, step, step_fast, format, flags);
    } catch (...) {
        return false;
    }
}

bool InputInt(const char* label, int* v, int step, int step_fast, ImGuiInputTextFlags flags) {
    if (!CheckInitialized() || !label || !v) return false;
    try {
        return ImGui::InputInt(label, v, step, step_fast, flags);
    } catch (...) {
        return false;
    }
}

bool InputDouble(const char* label, double* v, double step, double step_fast, const char* format, ImGuiInputTextFlags flags) {
    if (!CheckInitialized() || !label || !v) return false;
    try {
        return ImGui::InputDouble(label, v, step, step_fast, format, flags);
    } catch (...) {
        return false;
    }
}

bool InputScalar(const char* label, ImGuiDataType data_type, void* p_data, const void* p_step, const void* p_step_fast, const char* format, ImGuiInputTextFlags flags) {
    if (!CheckInitialized() || !label || !p_data) return false;
    try {
        return ImGui::InputScalar(label, data_type, p_data, p_step, p_step_fast, format, flags);
    } catch (...) {
        return false;
    }
}

bool Checkbox(const char* label, bool* v) {
    if (!CheckInitialized() || !label || !v) return false;
    try {
        return ImGui::Checkbox(label, v);
    } catch (...) {
        return false;
    }
}

bool RadioButton(const char* label, bool active) {
    if (!CheckInitialized() || !label) return false;
    try {
        return ImGui::RadioButton(label, active);
    } catch (...) {
        return false;
    }
}

bool RadioButton(const char* label, int* v, int v_button) {
    if (!CheckInitialized() || !label || !v) return false;
    try {
        return ImGui::RadioButton(label, v, v_button);
    } catch (...) {
        return false;
    }
}

bool DragFloat(const char* label, float* v, float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags) {
    if (!CheckInitialized() || !label || !v) return false;
    try {
        return ImGui::DragFloat(label, v, v_speed, v_min, v_max, format, flags);
    } catch (...) {
        return false;
    }
}

bool DragFloat2(const char* label, float v[2], float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags) {
    if (!CheckInitialized() || !label || !v) return false;
    try {
        return ImGui::DragFloat2(label, v, v_speed, v_min, v_max, format, flags);
    } catch (...) {
        return false;
    }
}

bool DragFloat3(const char* label, float v[3], float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags) {
    if (!CheckInitialized() || !label || !v) return false;
    try {
        return ImGui::DragFloat3(label, v, v_speed, v_min, v_max, format, flags);
    } catch (...) {
        return false;
    }
}

bool DragFloat4(const char* label, float v[4], float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags) {
    if (!CheckInitialized() || !label || !v) return false;
    try {
        return ImGui::DragFloat4(label, v, v_speed, v_min, v_max, format, flags);
    } catch (...) {
        return false;
    }
}

bool DragInt(const char* label, int* v, float v_speed, int v_min, int v_max, const char* format, ImGuiSliderFlags flags) {
    if (!CheckInitialized() || !label || !v) return false;
    try {
        return ImGui::DragInt(label, v, v_speed, v_min, v_max, format, flags);
    } catch (...) {
        return false;
    }
}

bool DragInt2(const char* label, int v[2], float v_speed, int v_min, int v_max, const char* format, ImGuiSliderFlags flags) {
    if (!CheckInitialized() || !label || !v) return false;
    try {
        return ImGui::DragInt2(label, v, v_speed, v_min, v_max, format, flags);
    } catch (...) {
        return false;
    }
}

bool DragInt3(const char* label, int v[3], float v_speed, int v_min, int v_max, const char* format, ImGuiSliderFlags flags) {
    if (!CheckInitialized() || !label || !v) return false;
    try {
        return ImGui::DragInt3(label, v, v_speed, v_min, v_max, format, flags);
    } catch (...) {
        return false;
    }
}

bool DragInt4(const char* label, int v[4], float v_speed, int v_min, int v_max, const char* format, ImGuiSliderFlags flags) {
    if (!CheckInitialized() || !label || !v) return false;
    try {
        return ImGui::DragInt4(label, v, v_speed, v_min, v_max, format, flags);
    } catch (...) {
        return false;
    }
}

bool SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format, ImGuiSliderFlags flags) {
    if (!CheckInitialized() || !label || !v) return false;
    try {
        return ImGui::SliderFloat(label, v, v_min, v_max, format, flags);
    } catch (...) {
        return false;
    }
}

bool SliderInt(const char* label, int* v, int v_min, int v_max, const char* format, ImGuiSliderFlags flags) {
    if (!CheckInitialized() || !label || !v) return false;
    try {
        return ImGui::SliderInt(label, v, v_min, v_max, format, flags);
    } catch (...) {
        return false;
    }
}

bool SliderFloat2(const char* label, float v[2], float v_min, float v_max, const char* format, ImGuiSliderFlags flags) {
    if (!CheckInitialized() || !label || !v) return false;
    try {
        return ImGui::SliderFloat2(label, v, v_min, v_max, format, flags);
    } catch (...) {
        return false;
    }
}

bool SliderFloat3(const char* label, float v[3], float v_min, float v_max, const char* format, ImGuiSliderFlags flags) {
    if (!CheckInitialized() || !label || !v) return false;
    try {
        return ImGui::SliderFloat3(label, v, v_min, v_max, format, flags);
    } catch (...) {
        return false;
    }
}

bool SliderFloat4(const char* label, float v[4], float v_min, float v_max, const char* format, ImGuiSliderFlags flags) {
    if (!CheckInitialized() || !label || !v) return false;
    try {
        return ImGui::SliderFloat4(label, v, v_min, v_max, format, flags);
    } catch (...) {
        return false;
    }
}

// Advanced widgets
bool Combo(const char* label, int* current_item, const char* const items[], int items_count, int popup_max_height_in_items) {
    if (!CheckInitialized() || !label || !current_item || !items || items_count <= 0) return false;
    try {
        return ImGui::Combo(label, current_item, items, items_count, popup_max_height_in_items);
    } catch (...) {
        return false;
    }
}

bool ListBox(const char* label, int* current_item, const char* const items[], int items_count, int height_in_items) {
    if (!CheckInitialized() || !label || !current_item || !items || items_count <= 0) return false;
    try {
        return ImGui::ListBox(label, current_item, items, items_count, height_in_items);
    } catch (...) {
        return false;
    }
}

bool BeginListBox(const char* label, const ImVec2& size) {
    if (!CheckInitialized() || !label) return false;
    try {
        return ImGui::BeginListBox(label, size);
    } catch (...) {
        return false;
    }
}

void EndListBox() {
    if (!CheckInitialized()) return;
    try {
        ImGui::EndListBox();
    } catch (...) {
    }
}

bool Selectable(const char* label, bool selected, ImGuiSelectableFlags flags, const ImVec2& size) {
    if (!CheckInitialized() || !label) return false;
    try {
        return ImGui::Selectable(label, selected, flags, size);
    } catch (...) {
        return false;
    }
}

bool ColorEdit3(const char* label, float col[3], ImGuiColorEditFlags flags) {
    if (!CheckInitialized() || !label || !col) return false;
    try {
        return ImGui::ColorEdit3(label, col, flags);
    } catch (...) {
        return false;
    }
}

bool ColorEdit4(const char* label, float col[4], ImGuiColorEditFlags flags) {
    if (!CheckInitialized() || !label || !col) return false;
    try {
        return ImGui::ColorEdit4(label, col, flags);
    } catch (...) {
        return false;
    }
}

bool ColorPicker3(const char* label, float col[3], ImGuiColorEditFlags flags) {
    if (!CheckInitialized() || !label || !col) return false;
    try {
        return ImGui::ColorPicker3(label, col, flags);
    } catch (...) {
        return false;
    }
}

bool ColorPicker4(const char* label, float col[4], ImGuiColorEditFlags flags) {
    if (!CheckInitialized() || !label || !col) return false;
    try {
        return ImGui::ColorPicker4(label, col, flags);
    } catch (...) {
        return false;
    }
}

bool ProgressBar(float fraction, const ImVec2& size_arg, const char* overlay) {
    if (!CheckInitialized()) return false;
    try {
        ImGui::ProgressBar(fraction, size_arg, overlay);
        return true;
    } catch (...) {
        return false;
    }
}

// Layout
bool BeginWindow(const char* name, bool* p_open, ImGuiWindowFlags flags) {
    if (!CheckInitialized() || !name) return false;
    try {
        return ImGui::Begin(name, p_open, flags);
    } catch (...) {
        return false;
    }
}

void SetNextWindowPos(const ImVec2& pos, ImGuiCond cond, const ImVec2& pivot) {
    if (!CheckInitialized()) return;
    try {
        ImGui::SetNextWindowPos(pos, cond, pivot);
    } catch (...) {
    }
}

void SetNextWindowSize(const ImVec2& size, ImGuiCond cond) {
    if (!CheckInitialized()) return;
    try {
        ImGui::SetNextWindowSize(size, cond);
    } catch (...) {
    }
}

void SetNextWindowViewport(ImGuiID viewport_id) {
    (void)viewport_id;
    // Dear ImGui public API does not expose viewport-ID routing on all supported versions; reserved for future docking glue.
}

void EndWindow() {
    if (!CheckInitialized()) return;
    try {
        ImGui::End();
    } catch (...) {
    }
}

bool BeginChild(const char* str_id, const ImVec2& size, bool border, ImGuiWindowFlags flags) {
    if (!CheckInitialized() || !str_id) return false;
    try {
        return ImGui::BeginChild(str_id, size, border, flags);
    } catch (...) {
        return false;
    }
}

void EndChild() {
    if (!CheckInitialized()) return;
    try {
        ImGui::EndChild();
    } catch (...) {
    }
}

void SameLine(float offset_x, float spacing_w) {
    if (!CheckInitialized()) return;
    try {
        ImGui::SameLine(offset_x, spacing_w);
    } catch (...) {
    }
}

void NewLine() {
    if (!CheckInitialized()) return;
    try {
        ImGui::NewLine();
    } catch (...) {
    }
}

void Separator() {
    if (!CheckInitialized()) return;
    try {
        ImGui::Separator();
    } catch (...) {
    }
}

void Spacing() {
    if (!CheckInitialized()) return;
    try {
        ImGui::Spacing();
    } catch (...) {
    }
}

void Dummy(const ImVec2& size) {
    if (!CheckInitialized()) return;
    try {
        ImGui::Dummy(size);
    } catch (...) {
    }
}

void Indent(float indent_w) {
    if (!CheckInitialized()) return;
    try {
        ImGui::Indent(indent_w);
    } catch (...) {
    }
}

void Unindent(float indent_w) {
    if (!CheckInitialized()) return;
    try {
        ImGui::Unindent(indent_w);
    } catch (...) {
    }
}

void BeginGroup() {
    if (!CheckInitialized()) return;
    try {
        ImGui::BeginGroup();
    } catch (...) {
    }
}

void EndGroup() {
    if (!CheckInitialized()) return;
    try {
        ImGui::EndGroup();
    } catch (...) {
    }
}

ImVec2 GetCursorPos() {
    if (!CheckInitialized()) return ImVec2(0, 0);
    try {
        return ImGui::GetCursorPos();
    } catch (...) {
        return ImVec2(0, 0);
    }
}

float GetCursorPosX() {
    if (!CheckInitialized()) return 0.0f;
    try {
        return ImGui::GetCursorPosX();
    } catch (...) {
        return 0.0f;
    }
}

float GetCursorPosY() {
    if (!CheckInitialized()) return 0.0f;
    try {
        return ImGui::GetCursorPosY();
    } catch (...) {
        return 0.0f;
    }
}

void SetCursorPos(const ImVec2& local_pos) {
    if (!CheckInitialized()) return;
    try {
        ImGui::SetCursorPos(local_pos);
    } catch (...) {
    }
}

void SetCursorPosX(float x) {
    if (!CheckInitialized()) return;
    try {
        ImGui::SetCursorPosX(x);
    } catch (...) {
    }
}

void SetCursorPosY(float y) {
    if (!CheckInitialized()) return;
    try {
        ImGui::SetCursorPosY(y);
    } catch (...) {
    }
}

ImVec2 GetCursorStartPos() {
    if (!CheckInitialized()) return ImVec2(0, 0);
    try {
        return ImGui::GetCursorStartPos();
    } catch (...) {
        return ImVec2(0, 0);
    }
}

ImVec2 GetCursorScreenPos() {
    if (!CheckInitialized()) return ImVec2(0, 0);
    try {
        return ImGui::GetCursorScreenPos();
    } catch (...) {
        return ImVec2(0, 0);
    }
}

void SetCursorScreenPos(const ImVec2& pos) {
    if (!CheckInitialized()) return;
    try {
        ImGui::SetCursorScreenPos(pos);
    } catch (...) {
    }
}

void AlignTextToFramePadding() {
    if (!CheckInitialized()) return;
    try {
        ImGui::AlignTextToFramePadding();
    } catch (...) {
    }
}

float GetTextLineHeight() {
    if (!CheckInitialized()) return 0.0f;
    try {
        return ImGui::GetTextLineHeight();
    } catch (...) {
        return 0.0f;
    }
}

float GetTextLineHeightWithSpacing() {
    if (!CheckInitialized()) return 0.0f;
    try {
        return ImGui::GetTextLineHeightWithSpacing();
    } catch (...) {
        return 0.0f;
    }
}

float GetFrameHeight() {
    if (!CheckInitialized()) return 0.0f;
    try {
        return ImGui::GetFrameHeight();
    } catch (...) {
        return 0.0f;
    }
}

float GetFrameHeightWithSpacing() {
    if (!CheckInitialized()) return 0.0f;
    try {
        return ImGui::GetFrameHeightWithSpacing();
    } catch (...) {
        return 0.0f;
    }
}

void SetNextItemWidth(float item_width) {
    if (!CheckInitialized()) return;
    try {
        ImGui::SetNextItemWidth(item_width);
    } catch (...) {
    }
}

float CalcItemWidth() {
    if (!CheckInitialized()) return 0.0f;
    try {
        return ImGui::CalcItemWidth();
    } catch (...) {
        return 0.0f;
    }
}

void PushItemWidth(float item_width) {
    if (!CheckInitialized()) return;
    try {
        ImGui::PushItemWidth(item_width);
    } catch (...) {
    }
}

void PopItemWidth() {
    if (!CheckInitialized()) return;
    try {
        ImGui::PopItemWidth();
    } catch (...) {
    }
}

float GetItemWidth() {
    if (!CheckInitialized()) return 0.0f;
    try {
        return ImGui::CalcItemWidth();
    } catch (...) {
        return 0.0f;
    }
}

void SetItemDefaultFocus() {
    if (!CheckInitialized()) return;
    try {
        ImGui::SetItemDefaultFocus();
    } catch (...) {
    }
}

void SetKeyboardFocusHere(int offset) {
    if (!CheckInitialized()) return;
    try {
        ImGui::SetKeyboardFocusHere(offset);
    } catch (...) {
    }
}

bool IsItemHovered(ImGuiHoveredFlags flags) {
    if (!CheckInitialized()) return false;
    try {
        return ImGui::IsItemHovered(flags);
    } catch (...) {
        return false;
    }
}

bool IsItemActive() {
    if (!CheckInitialized()) return false;
    try {
        return ImGui::IsItemActive();
    } catch (...) {
        return false;
    }
}

bool IsItemClicked(ImGuiMouseButton mouse_button) {
    if (!CheckInitialized()) return false;
    try {
        return ImGui::IsItemClicked(mouse_button);
    } catch (...) {
        return false;
    }
}

ImVec2 GetItemRectMin() {
    if (!CheckInitialized()) return ImVec2(0, 0);
    try {
        return ImGui::GetItemRectMin();
    } catch (...) {
        return ImVec2(0, 0);
    }
}

ImVec2 GetItemRectMax() {
    if (!CheckInitialized()) return ImVec2(0, 0);
    try {
        return ImGui::GetItemRectMax();
    } catch (...) {
        return ImVec2(0, 0);
    }
}

ImVec2 GetItemRectSize() {
    if (!CheckInitialized()) return ImVec2(0, 0);
    try {
        return ImGui::GetItemRectSize();
    } catch (...) {
        return ImVec2(0, 0);
    }
}

bool IsWindowHovered(ImGuiHoveredFlags flags) {
    if (!CheckInitialized()) return false;
    try {
        return ImGui::IsWindowHovered(flags);
    } catch (...) {
        return false;
    }
}

void PushStyleVar(ImGuiStyleVar idx, float val) {
    if (!CheckInitialized()) return;
    try {
        ImGui::PushStyleVar(idx, val);
    } catch (...) {
    }
}

void PushStyleVar(ImGuiStyleVar idx, const ImVec2& val) {
    if (!CheckInitialized()) return;
    try {
        ImGui::PushStyleVar(idx, val);
    } catch (...) {
    }
}

void PopStyleVar(int count) {
    if (!CheckInitialized()) return;
    try {
        ImGui::PopStyleVar(count);
    } catch (...) {
    }
}

void PushStyleColor(ImGuiCol idx, ImU32 col) {
    if (!CheckInitialized()) return;
    try {
        ImGui::PushStyleColor(idx, col);
    } catch (...) {
    }
}

void PushStyleColor(ImGuiCol idx, const ImVec4& col) {
    if (!CheckInitialized()) return;
    try {
        ImGui::PushStyleColor(idx, col);
    } catch (...) {
    }
}

void PopStyleColor(int count) {
    if (!CheckInitialized()) return;
    try {
        ImGui::PopStyleColor(count);
    } catch (...) {
    }
}

void PushFont(ImFont* font) {
    if (!CheckInitialized()) return;
    try {
        ImGui::PushFont(font);
    } catch (...) {
    }
}

void PopFont() {
    if (!CheckInitialized()) return;
    try {
        ImGui::PopFont();
    } catch (...) {
    }
}

ImVec2 GetContentRegionAvail() {
    if (!CheckInitialized()) return ImVec2(0, 0);
    try {
        return ImGui::GetContentRegionAvail();
    } catch (...) {
        return ImVec2(0, 0);
    }
}

// Tree nodes
bool TreeNode(const char* label) {
    if (!CheckInitialized() || !label) return false;
    try {
        return ImGui::TreeNode(label);
    } catch (...) {
        return false;
    }
}

bool TreeNode(const char* str_id, const char* fmt, ...) {
    if (!CheckInitialized() || !str_id || !fmt) return false;
    try {
        va_list args;
        va_start(args, fmt);
        bool result = ImGui::TreeNodeV(str_id, fmt, args);
        va_end(args);
        return result;
    } catch (...) {
        return false;
    }
}

bool TreeNodeV(const char* str_id, const char* fmt, va_list args) {
    if (!CheckInitialized() || !str_id || !fmt) return false;
    try {
        return ImGui::TreeNodeV(str_id, fmt, args);
    } catch (...) {
        return false;
    }
}

bool TreeNodeEx(const char* label, ImGuiTreeNodeFlags flags) {
    if (!CheckInitialized() || !label) return false;
    try {
        return ImGui::TreeNodeEx(label, flags);
    } catch (...) {
        return false;
    }
}

bool TreeNodeEx(const char* str_id, ImGuiTreeNodeFlags flags, const char* fmt, ...) {
    if (!CheckInitialized() || !str_id || !fmt) return false;
    try {
        va_list args;
        va_start(args, fmt);
        bool result = ImGui::TreeNodeExV(str_id, flags, fmt, args);
        va_end(args);
        return result;
    } catch (...) {
        return false;
    }
}

void TreePush(const char* str_id) {
    if (!CheckInitialized()) return;
    try {
        ImGui::TreePush(str_id);
    } catch (...) {
    }
}

void TreePop() {
    if (!CheckInitialized()) return;
    try {
        ImGui::TreePop();
    } catch (...) {
    }
}

float GetTreeNodeToLabelSpacing() {
    if (!CheckInitialized()) return 0.0f;
    try {
        return ImGui::GetTreeNodeToLabelSpacing();
    } catch (...) {
        return 0.0f;
    }
}

bool CollapsingHeader(const char* label, ImGuiTreeNodeFlags flags) {
    if (!CheckInitialized() || !label) return false;
    try {
        return ImGui::CollapsingHeader(label, flags);
    } catch (...) {
        return false;
    }
}

bool CollapsingHeader(const char* label, bool* p_open, ImGuiTreeNodeFlags flags) {
    if (!CheckInitialized() || !label || !p_open) return false;
    try {
        return ImGui::CollapsingHeader(label, p_open, flags);
    } catch (...) {
        return false;
    }
}

// Tables
bool BeginTable(const char* str_id, int column, ImGuiTableFlags flags, const ImVec2& outer_size, float inner_width) {
    if (!CheckInitialized() || !str_id || column <= 0) return false;
    try {
        return ImGui::BeginTable(str_id, column, flags, outer_size, inner_width);
    } catch (...) {
        return false;
    }
}

void EndTable() {
    if (!CheckInitialized()) return;
    try {
        ImGui::EndTable();
    } catch (...) {
    }
}

void TableNextRow(ImGuiTableRowFlags row_flags, float min_row_height) {
    if (!CheckInitialized()) return;
    try {
        ImGui::TableNextRow(row_flags, min_row_height);
    } catch (...) {
    }
}

bool TableNextColumn() {
    if (!CheckInitialized()) return false;
    try {
        return ImGui::TableNextColumn();
    } catch (...) {
        return false;
    }
}

bool TableSetColumnIndex(int column_n) {
    if (!CheckInitialized()) return false;
    try {
        return ImGui::TableSetColumnIndex(column_n);
    } catch (...) {
        return false;
    }
}

void TableSetupColumn(const char* label, ImGuiTableColumnFlags flags, float init_width_or_weight, ImGuiID user_id) {
    if (!CheckInitialized() || !label) return;
    try {
        ImGui::TableSetupColumn(label, flags, init_width_or_weight, user_id);
    } catch (...) {
    }
}

void TableSetupScrollFreeze(int cols, int rows) {
    if (!CheckInitialized()) return;
    try {
        ImGui::TableSetupScrollFreeze(cols, rows);
    } catch (...) {
    }
}

void TableHeadersRow() {
    if (!CheckInitialized()) return;
    try {
        ImGui::TableHeadersRow();
    } catch (...) {
    }
}

void TableHeader(const char* label) {
    if (!CheckInitialized() || !label) return;
    try {
        ImGui::TableHeader(label);
    } catch (...) {
    }
}

ImGuiTableSortSpecs* TableGetSortSpecs() {
    if (!CheckInitialized()) return nullptr;
    try {
        return ImGui::TableGetSortSpecs();
    } catch (...) {
        return nullptr;
    }
}

int TableGetColumnCount() {
    if (!CheckInitialized()) return 0;
    try {
        return ImGui::TableGetColumnCount();
    } catch (...) {
        return 0;
    }
}

int TableGetColumnIndex() {
    if (!CheckInitialized()) return -1;
    try {
        return ImGui::TableGetColumnIndex();
    } catch (...) {
        return -1;
    }
}

int TableGetRowIndex() {
    if (!CheckInitialized()) return -1;
    try {
        return ImGui::TableGetRowIndex();
    } catch (...) {
        return -1;
    }
}

const char* TableGetColumnName(int column_n) {
    if (!CheckInitialized()) return nullptr;
    try {
        return ImGui::TableGetColumnName(column_n);
    } catch (...) {
        return nullptr;
    }
}

ImGuiTableColumnFlags TableGetColumnFlags(int column_n) {
    if (!CheckInitialized()) return 0;
    try {
        return ImGui::TableGetColumnFlags(column_n);
    } catch (...) {
        return 0;
    }
}

void TableSetColumnEnabled(int column_n, bool v) {
    if (!CheckInitialized()) return;
    try {
        ImGui::TableSetColumnEnabled(column_n, v);
    } catch (...) {
    }
}

void TableSetBgColor(ImGuiTableBgTarget target, ImU32 color, int column_n) {
    if (!CheckInitialized()) return;
    try {
        ImGui::TableSetBgColor(target, color, column_n);
    } catch (...) {
    }
}

// Menus
bool BeginMenuBar() {
    if (!CheckInitialized()) return false;
    try {
        return ImGui::BeginMenuBar();
    } catch (...) {
        return false;
    }
}

void EndMenuBar() {
    if (!CheckInitialized()) return;
    try {
        ImGui::EndMenuBar();
    } catch (...) {
    }
}

bool BeginMainMenuBar() {
    if (!CheckInitialized()) return false;
    try {
        return ImGui::BeginMainMenuBar();
    } catch (...) {
        return false;
    }
}

void EndMainMenuBar() {
    if (!CheckInitialized()) return;
    try {
        ImGui::EndMainMenuBar();
    } catch (...) {
    }
}

bool BeginMenu(const char* label, bool enabled) {
    if (!CheckInitialized() || !label) return false;
    try {
        return ImGui::BeginMenu(label, enabled);
    } catch (...) {
        return false;
    }
}

void EndMenu() {
    if (!CheckInitialized()) return;
    try {
        ImGui::EndMenu();
    } catch (...) {
    }
}

bool MenuItem(const char* label, const char* shortcut, bool selected, bool enabled) {
    if (!CheckInitialized() || !label) return false;
    try {
        return ImGui::MenuItem(label, shortcut, selected, enabled);
    } catch (...) {
        return false;
    }
}

bool MenuItem(const char* label, const char* shortcut, bool* p_selected, bool enabled) {
    if (!CheckInitialized() || !label || !p_selected) return false;
    try {
        return ImGui::MenuItem(label, shortcut, p_selected, enabled);
    } catch (...) {
        return false;
    }
}

// Tabs
bool BeginTabBar(const char* str_id, ImGuiTabBarFlags flags) {
    if (!CheckInitialized() || !str_id) return false;
    try {
        return ImGui::BeginTabBar(str_id, flags);
    } catch (...) {
        return false;
    }
}

void EndTabBar() {
    if (!CheckInitialized()) return;
    try {
        ImGui::EndTabBar();
    } catch (...) {
    }
}

bool BeginTabItem(const char* label, bool* p_open, ImGuiTabItemFlags flags) {
    if (!CheckInitialized() || !label) return false;
    try {
        return ImGui::BeginTabItem(label, p_open, flags);
    } catch (...) {
        return false;
    }
}

void EndTabItem() {
    if (!CheckInitialized()) return;
    try {
        ImGui::EndTabItem();
    } catch (...) {
    }
}

void SetTabItemClosed(const char* tab_or_docked_window_label) {
    if (!CheckInitialized() || !tab_or_docked_window_label) return;
    try {
        ImGui::SetTabItemClosed(tab_or_docked_window_label);
    } catch (...) {
    }
}

// Popups
void OpenPopup(const char* str_id, ImGuiPopupFlags popup_flags) {
    if (!CheckInitialized() || !str_id) return;
    try {
        ImGui::OpenPopup(str_id, popup_flags);
    } catch (...) {
    }
}

void OpenPopupOnItemClick(const char* str_id, ImGuiPopupFlags popup_flags) {
    if (!CheckInitialized()) return;
    try {
        ImGui::OpenPopupOnItemClick(str_id, popup_flags);
    } catch (...) {
    }
}

void CloseCurrentPopup() {
    if (!CheckInitialized()) return;
    try {
        ImGui::CloseCurrentPopup();
    } catch (...) {
    }
}

bool BeginPopup(const char* str_id, ImGuiWindowFlags flags) {
    if (!CheckInitialized() || !str_id) return false;
    try {
        return ImGui::BeginPopup(str_id, flags);
    } catch (...) {
        return false;
    }
}

bool BeginPopupModal(const char* name, bool* p_open, ImGuiWindowFlags flags) {
    if (!CheckInitialized() || !name) return false;
    try {
        return ImGui::BeginPopupModal(name, p_open, flags);
    } catch (...) {
        return false;
    }
}

bool BeginPopupContextItem(const char* str_id, ImGuiPopupFlags popup_flags) {
    if (!CheckInitialized()) return false;
    try {
        return ImGui::BeginPopupContextItem(str_id, popup_flags);
    } catch (...) {
        return false;
    }
}

bool BeginPopupContextWindow(const char* str_id, ImGuiPopupFlags popup_flags) {
    if (!CheckInitialized()) return false;
    try {
        return ImGui::BeginPopupContextWindow(str_id, popup_flags);
    } catch (...) {
        return false;
    }
}

bool BeginPopupContextVoid(const char* str_id, ImGuiPopupFlags popup_flags) {
    if (!CheckInitialized()) return false;
    try {
        return ImGui::BeginPopupContextVoid(str_id, popup_flags);
    } catch (...) {
        return false;
    }
}

void EndPopup() {
    if (!CheckInitialized()) return;
    try {
        ImGui::EndPopup();
    } catch (...) {
    }
}

bool IsPopupOpen(const char* str_id, ImGuiPopupFlags flags) {
    if (!CheckInitialized() || !str_id) return false;
    try {
        return ImGui::IsPopupOpen(str_id, flags);
    } catch (...) {
        return false;
    }
}

// Tooltips
void SetTooltip(const char* fmt, ...) {
    if (!CheckInitialized() || !fmt) return;
    try {
        va_list args;
        va_start(args, fmt);
        ImGui::SetTooltipV(fmt, args);
        va_end(args);
    } catch (...) {
    }
}

void BeginTooltip() {
    if (!CheckInitialized()) return;
    try {
        ImGui::BeginTooltip();
    } catch (...) {
    }
}

void EndTooltip() {
    if (!CheckInitialized()) return;
    try {
        ImGui::EndTooltip();
    } catch (...) {
    }
}

bool IsKeyPressed(ImGuiKey key, bool repeat) {
    if (!CheckInitialized()) return false;
    try {
        return ImGui::IsKeyPressed(key, repeat);
    } catch (...) {
        return false;
    }
}

bool IsKeyChordPressed(ImGuiKeyChord key_chord) {
    if (!CheckInitialized()) return false;
    try {
        return ImGui::IsKeyChordPressed(key_chord);
    } catch (...) {
        return false;
    }
}

bool IsMouseClicked(ImGuiMouseButton button, bool repeat) {
    if (!CheckInitialized()) return false;
    try {
        return ImGui::IsMouseClicked(button, repeat);
    } catch (...) {
        return false;
    }
}

bool IsMouseDown(ImGuiMouseButton button) {
    if (!CheckInitialized()) return false;
    try {
        return ImGui::IsMouseDown(button);
    } catch (...) {
        return false;
    }
}

bool IsMouseDragging(ImGuiMouseButton button, float lock_threshold) {
    if (!CheckInitialized()) return false;
    try {
        return ImGui::IsMouseDragging(button, lock_threshold);
    } catch (...) {
        return false;
    }
}

ImVec2 GetMousePos() {
    if (!CheckInitialized()) return ImVec2(0, 0);
    try {
        return ImGui::GetMousePos();
    } catch (...) {
        return ImVec2(0, 0);
    }
}

// Editor-specific helpers
bool InputTextMultiline(const char* label, std::string& str, const ImVec2& size, ImGuiInputTextFlags flags) {
    if (!CheckInitialized() || !label) return false;
    try {
        // Resize buffer if needed
        static thread_local std::string buffer;
        buffer = str;
        buffer.resize(buffer.size() + 1024); // Add extra space

        bool result = ImGui::InputTextMultiline(label, buffer.data(), buffer.size(), size, flags);
        if (result) {
            str = std::string(buffer.c_str());
        }
        return result;
    } catch (...) {
        return false;
    }
}

} // namespace LibUI::Widgets

