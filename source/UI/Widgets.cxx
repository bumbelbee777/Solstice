#include <UI/Widgets.hxx>
#include <UI/Animation.hxx>
#include <UI/VisualEffects.hxx>
#include <UI/MotionGraphics.hxx>
#include <imgui.h>
#include <imgui_internal.h>
#include <cmath>
#include <cfloat>
#include <algorithm>

namespace Solstice::UI::Widgets {

    void BeginWindow(const std::string& title) {
        ImGui::Begin(title.c_str());
    }

    void EndWindow() {
        ImGui::End();
    }

    void SameLine() {
        ImGui::SameLine();
    }

    void Separator() {
        ImGui::Separator();
    }

    void Spacing() {
        ImGui::Spacing();
    }

    // Advanced Layout
    bool CollapsingHeader(const std::string& label, bool defaultOpen) {
        return ImGui::CollapsingHeader(label.c_str(), defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0);
    }

    void BeginChild(const std::string& id, float width, float height, bool border) {
        ImGui::BeginChild(id.c_str(), ImVec2(width, height), border);
    }

    void EndChild() {
        ImGui::EndChild();
    }

    void Tooltip(const std::string& text) {
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(text.c_str());
            ImGui::EndTooltip();
        }
    }

    void Text(const std::string& content) {
        ImGui::Text("%s", content.c_str());
    }

    void Label(const std::string& label, const std::string& text) {
        ImGui::LabelText(label.c_str(), "%s", text.c_str());
    }

    // Text Styles
    void TextBold(const std::string& text) {
        // Note: Real bold requires pushing a bold font.
        // For now, we just render text. If a bold font is available, we should push it here.
        ImGui::Text("%s", text.c_str());
    }

    void TextUnderlined(const std::string& text, const ImVec4& color) {
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::TextColored(color, "%s", text.c_str());
        ImVec2 size = ImGui::CalcTextSize(text.c_str());
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(pos.x, pos.y + size.y),
            ImVec2(pos.x + size.x, pos.y + size.y),
            ImGui::ColorConvertFloat4ToU32(color)
        );
    }

    void TextStrikethrough(const std::string& text, const ImVec4& color) {
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::TextColored(color, "%s", text.c_str());
        ImVec2 size = ImGui::CalcTextSize(text.c_str());
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(pos.x, pos.y + size.y * 0.5f),
            ImVec2(pos.x + size.x, pos.y + size.y * 0.5f),
            ImGui::ColorConvertFloat4ToU32(color)
        );
    }

    // Text Animations
    void TypewriterText(const std::string& text, float progress) {
        int char_count = (int)(text.length() * progress);
        if (char_count > (int)text.length()) char_count = (int)text.length();
        if (char_count < 0) char_count = 0;

        // Create a substring safely
        std::string sub = text.substr(0, char_count);
        ImGui::TextUnformatted(sub.c_str());
    }

    void PulsingText(const std::string& text, float time, float speed) {
        float alpha = (sin(time * speed) * 0.5f) + 0.5f;
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
        ImGui::Text("%s", text.c_str());
        ImGui::PopStyleVar();
    }

    void RainbowText(const std::string& text, float time, float speed) {
        ImVec2 pos = ImGui::GetCursorScreenPos();
        for (size_t i = 0; i < text.length(); ++i) {
            float hue = fmod(time * speed + (float)i * 0.1f, 1.0f);
            ImVec4 color = ImVec4(ImColor::HSV(hue, 1.0f, 1.0f));
            char buf[2] = { text[i], 0 };
            ImGui::GetWindowDrawList()->AddText(pos, ImGui::ColorConvertFloat4ToU32(color), buf);
            pos.x += ImGui::CalcTextSize(buf).x;
        }
        // Advance cursor manually
        ImGui::Dummy(ImGui::CalcTextSize(text.c_str()));
    }

    bool Button(const std::string& label, const std::function<void()>& onClick) {
        if (ImGui::Button(label.c_str())) {
            if (onClick) {
                onClick();
            }
            return true;
        }
        return false;
    }

    bool Checkbox(const std::string& label, bool& value) {
        return ImGui::Checkbox(label.c_str(), &value);
    }

    bool ColorEdit3(const std::string& label, float* col) {
        return ImGui::ColorEdit3(label.c_str(), col);
    }

    bool ColorEdit4(const std::string& label, float* col) {
        return ImGui::ColorEdit4(label.c_str(), col);
    }

    bool Combo(const std::string& label, int* current_item, const std::vector<std::string>& items) {
        std::vector<const char*> c_items;
        c_items.reserve(items.size());
        for (const auto& item : items) {
            c_items.push_back(item.c_str());
        }
        return ImGui::Combo(label.c_str(), current_item, c_items.data(), (int)c_items.size());
    }

    void ProgressBar(float fraction, const std::string& overlay) {
        ImGui::ProgressBar(fraction, ImVec2(-1.0f, 0.0f), overlay.empty() ? nullptr : overlay.c_str());
    }

    bool SliderFloat(const std::string& label, float& value, float min, float max) {
        return ImGui::SliderFloat(label.c_str(), &value, min, max);
    }

    bool SliderInt(const std::string& label, int& value, int min, int max) {
        return ImGui::SliderInt(label.c_str(), &value, min, max);
    }

    bool SliderFloat3(const std::string& label, float* value, float min, float max) {
        return ImGui::SliderFloat3(label.c_str(), value, min, max);
    }

    // Callback for InputText to resize std::string
    static int InputTextCallback(ImGuiInputTextCallbackData* data) {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
            std::string* str = (std::string*)data->UserData;
            str->resize(data->BufTextLen);
            data->Buf = (char*)str->c_str();
        }
        return 0;
    }

    bool InputText(const std::string& label, std::string& value) {
        return ImGui::InputText(label.c_str(), (char*)value.c_str(), value.capacity() + 1, ImGuiInputTextFlags_CallbackResize, InputTextCallback, &value);
    }

    // Effects
    void PushTransparency(float alpha) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
    }

    void PopTransparency() {
        ImGui::PopStyleVar();
    }

    // Dialogs
    bool BeginPopupModal(const std::string& name, bool* p_open) {
        return ImGui::BeginPopupModal(name.c_str(), p_open, ImGuiWindowFlags_AlwaysAutoResize);
    }

    void EndPopup() {
        ImGui::EndPopup();
    }

    void OpenPopup(const std::string& name) {
        ImGui::OpenPopup(name.c_str());
    }

    void CloseCurrentPopup() {
        ImGui::CloseCurrentPopup();
    }

    void ConfirmationDialog(const std::string& id, const std::string& title, const std::string& message, const std::function<void()>& onConfirm) {
        if (ImGui::BeginPopupModal(id.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("%s", title.c_str());
            ImGui::Separator();
            ImGui::Text("%s", message.c_str());
            ImGui::Spacing();

            if (ImGui::Button("OK", ImVec2(120, 0))) {
                if (onConfirm) onConfirm();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    // Enhanced Dialogs
    bool BeginDialog(const std::string& title, bool* p_open, ImGuiWindowFlags flags) {
        return ImGui::Begin(title.c_str(), p_open, flags);
    }

    void EndDialog() {
        ImGui::End();
    }

    // Color & Visual Selection
    bool ColorPicker(const std::string& label, float col[3], ImGuiColorEditFlags flags) {
        return ImGui::ColorPicker3(label.c_str(), col, flags);
    }

    bool ColorPicker4(const std::string& label, float col[4], ImGuiColorEditFlags flags) {
        return ImGui::ColorPicker4(label.c_str(), col, flags);
    }

    bool ColorPalette(const std::string& label, float* col, const std::vector<ImVec4>& palette) {
        bool changed = false;
        ImGui::Text("%s", label.c_str());
        ImGui::SameLine();
        for (size_t i = 0; i < palette.size(); ++i) {
            if (i > 0) ImGui::SameLine();
            ImVec4 p = palette[i];
            if (ImGui::ColorButton(("##palette" + std::to_string(i)).c_str(), p)) {
                col[0] = p.x; col[1] = p.y; col[2] = p.z;
                if (col[3] != -1.0f) col[3] = p.w;
                changed = true;
            }
        }
        return changed;
    }

    bool ColorButton(const std::string& desc_id, const ImVec4& col, ImGuiColorEditFlags flags, ImVec2 size) {
        return ImGui::ColorButton(desc_id.c_str(), col, flags, size);
    }

    // File & Path Management (simplified - full implementation would require file system dialogs)
    bool FileDialog(const std::string& label, std::string& path, const std::string& filter) {
        bool changed = InputText(label, path);
        ImGui::SameLine();
        if (ImGui::Button("Browse...")) {
            // TODO: Open native file dialog
            // For now, just allow text input
        }
        return changed;
    }

    bool DirectoryDialog(const std::string& label, std::string& path) {
        bool changed = InputText(label, path);
        ImGui::SameLine();
        if (ImGui::Button("Browse...")) {
            // TODO: Open native directory dialog
        }
        return changed;
    }

    bool PathInput(const std::string& label, std::string& path, const std::string& button_label) {
        bool changed = InputText(label, path);
        ImGui::SameLine();
        if (ImGui::Button(button_label.c_str())) {
            // TODO: Open file/directory picker
        }
        return changed;
    }

    // Data Display & Input - Tables
    bool BeginTable(const std::string& str_id, int column, ImGuiTableFlags flags, const ImVec2& outer_size, float inner_width) {
        return ImGui::BeginTable(str_id.c_str(), column, flags, outer_size, inner_width);
    }

    void EndTable() {
        ImGui::EndTable();
    }

    void TableNextRow(ImGuiTableRowFlags row_flags, float min_row_height) {
        ImGui::TableNextRow(row_flags, min_row_height);
    }

    bool TableNextColumn() {
        return ImGui::TableNextColumn();
    }

    bool TableSetColumnIndex(int column_n) {
        return ImGui::TableSetColumnIndex(column_n);
    }

    void TableSetupColumn(const std::string& label, ImGuiTableColumnFlags flags, float init_width_or_weight, ImU32 user_id) {
        ImGui::TableSetupColumn(label.c_str(), flags, init_width_or_weight, user_id);
    }

    void TableSetupScrollFreeze(int cols, int rows) {
        ImGui::TableSetupScrollFreeze(cols, rows);
    }

    void TableHeadersRow() {
        ImGui::TableHeadersRow();
    }

    void TableHeader(const std::string& label) {
        ImGui::TableHeader(label.c_str());
    }

    // Lists
    bool ListBox(const std::string& label, int* current_item, const std::vector<std::string>& items, int height_in_items) {
        std::vector<const char*> c_items;
        c_items.reserve(items.size());
        for (const auto& item : items) {
            c_items.push_back(item.c_str());
        }
        return ImGui::ListBox(label.c_str(), current_item, c_items.data(), (int)c_items.size(), height_in_items);
    }

    bool ListBoxHeader(const std::string& label, const ImVec2& size) {
        // ListBoxHeader/Footer were replaced with BeginListBox/EndListBox in ImGui 1.79+
        return ImGui::BeginListBox(label.c_str(), size);
    }

    void ListBoxFooter() {
        // ListBoxHeader/Footer were replaced with BeginListBox/EndListBox in ImGui 1.79+
        ImGui::EndListBox();
    }

    bool Selectable(const std::string& label, bool selected, ImGuiSelectableFlags flags, const ImVec2& size) {
        return ImGui::Selectable(label.c_str(), selected, flags, size);
    }

    bool Selectable(const std::string& label, bool* p_selected, ImGuiSelectableFlags flags, const ImVec2& size) {
        return ImGui::Selectable(label.c_str(), p_selected, flags, size);
    }

    // Enhanced Input
    namespace {
        int InputTextMultilineCallback(ImGuiInputTextCallbackData* data) {
            if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
                std::string* str = (std::string*)data->UserData;
                str->resize(data->BufTextLen);
                data->Buf = (char*)str->c_str();
            }
            return 0;
        }
    }

    bool InputTextMultiline(const std::string& label, std::string& value, const ImVec2& size, ImGuiInputTextFlags flags) {
        return ImGui::InputTextMultiline(label.c_str(), (char*)value.c_str(), value.capacity() + 1, size, flags | ImGuiInputTextFlags_CallbackResize, InputTextMultilineCallback, &value);
    }

    bool InputFloat(const std::string& label, float* v, float step, float step_fast, const std::string& format, ImGuiInputTextFlags flags) {
        return ImGui::InputFloat(label.c_str(), v, step, step_fast, format.c_str(), flags);
    }

    bool InputInt(const std::string& label, int* v, int step, int step_fast, ImGuiInputTextFlags flags) {
        return ImGui::InputInt(label.c_str(), v, step, step_fast, flags);
    }

    bool InputFloat2(const std::string& label, float v[2], const std::string& format, ImGuiInputTextFlags flags) {
        return ImGui::InputFloat2(label.c_str(), v, format.c_str(), flags);
    }

    bool InputFloat3(const std::string& label, float v[3], const std::string& format, ImGuiInputTextFlags flags) {
        return ImGui::InputFloat3(label.c_str(), v, format.c_str(), flags);
    }

    bool InputFloat4(const std::string& label, float v[4], const std::string& format, ImGuiInputTextFlags flags) {
        return ImGui::InputFloat4(label.c_str(), v, format.c_str(), flags);
    }

    bool InputInt2(const std::string& label, int v[2], ImGuiInputTextFlags flags) {
        return ImGui::InputInt2(label.c_str(), v, flags);
    }

    bool InputInt3(const std::string& label, int v[3], ImGuiInputTextFlags flags) {
        return ImGui::InputInt3(label.c_str(), v, flags);
    }

    bool InputInt4(const std::string& label, int v[4], ImGuiInputTextFlags flags) {
        return ImGui::InputInt4(label.c_str(), v, flags);
    }

    // Drag inputs
    bool DragFloat(const std::string& label, float* v, float v_speed, float v_min, float v_max, const std::string& format, ImGuiSliderFlags flags) {
        return ImGui::DragFloat(label.c_str(), v, v_speed, v_min, v_max, format.c_str(), flags);
    }

    bool DragInt(const std::string& label, int* v, float v_speed, int v_min, int v_max, const std::string& format, ImGuiSliderFlags flags) {
        return ImGui::DragInt(label.c_str(), v, v_speed, v_min, v_max, format.c_str(), flags);
    }

    bool DragFloat2(const std::string& label, float v[2], float v_speed, float v_min, float v_max, const std::string& format, ImGuiSliderFlags flags) {
        return ImGui::DragFloat2(label.c_str(), v, v_speed, v_min, v_max, format.c_str(), flags);
    }

    bool DragFloat3(const std::string& label, float v[3], float v_speed, float v_min, float v_max, const std::string& format, ImGuiSliderFlags flags) {
        return ImGui::DragFloat3(label.c_str(), v, v_speed, v_min, v_max, format.c_str(), flags);
    }

    bool DragFloat4(const std::string& label, float v[4], float v_speed, float v_min, float v_max, const std::string& format, ImGuiSliderFlags flags) {
        return ImGui::DragFloat4(label.c_str(), v, v_speed, v_min, v_max, format.c_str(), flags);
    }

    bool DragInt2(const std::string& label, int v[2], float v_speed, int v_min, int v_max, const std::string& format, ImGuiSliderFlags flags) {
        return ImGui::DragInt2(label.c_str(), v, v_speed, v_min, v_max, format.c_str(), flags);
    }

    bool DragInt3(const std::string& label, int v[3], float v_speed, int v_min, int v_max, const std::string& format, ImGuiSliderFlags flags) {
        return ImGui::DragInt3(label.c_str(), v, v_speed, v_min, v_max, format.c_str(), flags);
    }

    bool DragInt4(const std::string& label, int v[4], float v_speed, int v_min, int v_max, const std::string& format, ImGuiSliderFlags flags) {
        return ImGui::DragInt4(label.c_str(), v, v_speed, v_min, v_max, format.c_str(), flags);
    }

    // Enhanced Sliders
    bool SliderAngle(const std::string& label, float* v_rad, float v_degrees_min, float v_degrees_max, const std::string& format, ImGuiSliderFlags flags) {
        return ImGui::SliderAngle(label.c_str(), v_rad, v_degrees_min, v_degrees_max, format.c_str(), flags);
    }

    bool SliderFloat2(const std::string& label, float v[2], float v_min, float v_max, const std::string& format, ImGuiSliderFlags flags) {
        return ImGui::SliderFloat2(label.c_str(), v, v_min, v_max, format.c_str(), flags);
    }

    bool SliderFloat4(const std::string& label, float v[4], float v_min, float v_max, const std::string& format, ImGuiSliderFlags flags) {
        return ImGui::SliderFloat4(label.c_str(), v, v_min, v_max, format.c_str(), flags);
    }

    bool SliderInt2(const std::string& label, int v[2], int v_min, int v_max, const std::string& format, ImGuiSliderFlags flags) {
        return ImGui::SliderInt2(label.c_str(), v, v_min, v_max, format.c_str(), flags);
    }

    bool SliderInt3(const std::string& label, int v[3], int v_min, int v_max, const std::string& format, ImGuiSliderFlags flags) {
        return ImGui::SliderInt3(label.c_str(), v, v_min, v_max, format.c_str(), flags);
    }

    bool SliderInt4(const std::string& label, int v[4], int v_min, int v_max, const std::string& format, ImGuiSliderFlags flags) {
        return ImGui::SliderInt4(label.c_str(), v, v_min, v_max, format.c_str(), flags);
    }

    bool VSliderFloat(const std::string& label, const ImVec2& size, float* v, float v_min, float v_max, const std::string& format, ImGuiSliderFlags flags) {
        return ImGui::VSliderFloat(label.c_str(), size, v, v_min, v_max, format.c_str(), flags);
    }

    bool VSliderInt(const std::string& label, const ImVec2& size, int* v, int v_min, int v_max, const std::string& format, ImGuiSliderFlags flags) {
        return ImGui::VSliderInt(label.c_str(), size, v, v_min, v_max, format.c_str(), flags);
    }

    // Layout & Navigation - Tabs
    bool BeginTabBar(const std::string& str_id, ImGuiTabBarFlags flags) {
        return ImGui::BeginTabBar(str_id.c_str(), flags);
    }

    void EndTabBar() {
        ImGui::EndTabBar();
    }

    bool BeginTabItem(const std::string& label, bool* p_open, ImGuiTabItemFlags flags) {
        return ImGui::BeginTabItem(label.c_str(), p_open, flags);
    }

    void EndTabItem() {
        ImGui::EndTabItem();
    }

    void SetTabItemClosed(const std::string& tab_or_docked_window_label) {
        ImGui::SetTabItemClosed(tab_or_docked_window_label.c_str());
    }

    // Tree nodes
    bool TreeNode(const std::string& label) {
        return ImGui::TreeNode(label.c_str());
    }

    bool TreeNode(const std::string& str_id, const std::string& fmt) {
        return ImGui::TreeNode(str_id.c_str(), "%s", fmt.c_str());
    }

    bool TreeNodeEx(const std::string& label, ImGuiTreeNodeFlags flags) {
        return ImGui::TreeNodeEx(label.c_str(), flags);
    }

    bool TreeNodeEx(const std::string& str_id, ImGuiTreeNodeFlags flags, const std::string& fmt) {
        return ImGui::TreeNodeEx(str_id.c_str(), flags, "%s", fmt.c_str());
    }

    void TreePop() {
        ImGui::TreePop();
    }

    void SetNextItemOpen(bool is_open, ImGuiCond cond) {
        ImGui::SetNextItemOpen(is_open, cond);
    }

    // Menu bar
    bool BeginMenuBar() {
        return ImGui::BeginMenuBar();
    }

    void EndMenuBar() {
        ImGui::EndMenuBar();
    }

    bool BeginMenu(const std::string& label, bool enabled) {
        return ImGui::BeginMenu(label.c_str(), enabled);
    }

    void EndMenu() {
        ImGui::EndMenu();
    }

    bool MenuItem(const std::string& label, const std::string& shortcut, bool selected, bool enabled) {
        return ImGui::MenuItem(label.c_str(), shortcut.empty() ? nullptr : shortcut.c_str(), selected, enabled);
    }

    bool MenuItem(const std::string& label, const std::string& shortcut, bool* p_selected, bool enabled) {
        return ImGui::MenuItem(label.c_str(), shortcut.empty() ? nullptr : shortcut.c_str(), p_selected, enabled);
    }

    // Context menus
    void OpenPopupOnItemClick(const std::string& str_id, ImGuiPopupFlags popup_flags) {
        ImGui::OpenPopupOnItemClick(str_id.empty() ? nullptr : str_id.c_str(), popup_flags);
    }

    bool BeginPopupContextItem(const std::string& str_id, ImGuiPopupFlags popup_flags) {
        return ImGui::BeginPopupContextItem(str_id.empty() ? nullptr : str_id.c_str(), popup_flags);
    }

    bool BeginPopupContextWindow(const std::string& str_id, ImGuiPopupFlags popup_flags) {
        return ImGui::BeginPopupContextWindow(str_id.empty() ? nullptr : str_id.c_str(), popup_flags);
    }

    bool BeginPopupContextVoid(const std::string& str_id, ImGuiPopupFlags popup_flags) {
        return ImGui::BeginPopupContextVoid(str_id.empty() ? nullptr : str_id.c_str(), popup_flags);
    }

    void EnhancedTooltip(const std::string& text, const ImVec4& color) {
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextUnformatted(text.c_str());
            ImGui::PopStyleColor();
            ImGui::EndTooltip();
        }
    }

    // Media & Graphics
    void Image(ImTextureID user_texture_id, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& tint_col, const ImVec4& border_col) {
        ImGui::Image(user_texture_id, size, uv0, uv1, tint_col, border_col);
    }

    bool ImageButton(const std::string& str_id, ImTextureID user_texture_id, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, int frame_padding, const ImVec4& bg_col, const ImVec4& tint_col) {
        // Newer ImGui ImageButton signature: (id, texture_id, size, uv0, uv1, bg_col, tint_col)
        // frame_padding parameter was removed - ignore it
        (void)frame_padding; // Unused parameter
        return ImGui::ImageButton(str_id.c_str(), user_texture_id, size, uv0, uv1, bg_col, tint_col);
    }

    void PlotLines(const std::string& label, const float* values, int values_count, int values_offset, const std::string& overlay_text, float scale_min, float scale_max, ImVec2 graph_size, int stride) {
        ImGui::PlotLines(label.c_str(), values, values_count, values_offset, overlay_text.empty() ? nullptr : overlay_text.c_str(), scale_min, scale_max, graph_size, stride);
    }

    void PlotHistogram(const std::string& label, const float* values, int values_count, int values_offset, const std::string& overlay_text, float scale_min, float scale_max, ImVec2 graph_size, int stride) {
        ImGui::PlotHistogram(label.c_str(), values, values_count, values_offset, overlay_text.empty() ? nullptr : overlay_text.c_str(), scale_min, scale_max, graph_size, stride);
    }

    void CircularProgressBar(float fraction, float radius, const std::string& overlay) {
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        float angle = fraction * 2.0f * 3.14159f;
        ImVec2 center = ImVec2(pos.x + radius, pos.y + radius);

        // Draw background circle
        drawList->AddCircle(center, radius, ImGui::GetColorU32(ImGuiCol_FrameBg), 32, 2.0f);

        // Draw progress arc
        drawList->PathArcTo(center, radius, -3.14159f * 0.5f, -3.14159f * 0.5f + angle, 32);
        drawList->PathStroke(ImGui::GetColorU32(ImGuiCol_PlotHistogram), ImDrawFlags_None, 3.0f);

        // Draw text overlay
        if (!overlay.empty()) {
            ImVec2 textSize = ImGui::CalcTextSize(overlay.c_str());
            ImVec2 textPos = ImVec2(center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f);
            drawList->AddText(textPos, ImGui::GetColorU32(ImGuiCol_Text), overlay.c_str());
        }

        ImGui::Dummy(ImVec2(radius * 2.0f, radius * 2.0f));
    }

    void LoadingSpinner(const std::string& label, float radius, float thickness, const ImVec4& color) {
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImU32 col = ImGui::ColorConvertFloat4ToU32(color);

        float time = (float)ImGui::GetTime();
        ImVec2 center = ImVec2(pos.x + radius, pos.y + radius);

        for (int i = 0; i < 8; ++i) {
            float angle = time * 2.0f + i * 3.14159f / 4.0f;
            float x = center.x + cos(angle) * radius;
            float y = center.y + sin(angle) * radius;
            float alpha = 1.0f - (float)i / 8.0f;
            ImU32 segmentCol = ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, color.w * alpha));
            drawList->AddCircleFilled(ImVec2(x, y), thickness, segmentCol);
        }

        ImGui::Dummy(ImVec2(radius * 2.0f, radius * 2.0f));
    }

    // Advanced Controls
    bool SmallButton(const std::string& label) {
        return ImGui::SmallButton(label.c_str());
    }

    bool ArrowButton(const std::string& str_id, ImGuiDir dir) {
        return ImGui::ArrowButton(str_id.c_str(), dir);
    }

    bool InvisibleButton(const std::string& str_id, const ImVec2& size, ImGuiButtonFlags flags) {
        return ImGui::InvisibleButton(str_id.c_str(), size, flags);
    }

    bool RadioButton(const std::string& label, bool active) {
        return ImGui::RadioButton(label.c_str(), active);
    }

    bool RadioButton(const std::string& label, int* v, int v_button) {
        return ImGui::RadioButton(label.c_str(), v, v_button);
    }

    bool ComboFlags(const std::string& label, unsigned int* flags, const std::vector<std::pair<std::string, unsigned int>>& items) {
        bool changed = false;
        std::string preview = "";
        for (const auto& item : items) {
            if (*flags & item.second) {
                if (!preview.empty()) preview += ", ";
                preview += item.first;
            }
        }
        if (preview.empty()) preview = "None";

        if (ImGui::BeginCombo(label.c_str(), preview.c_str())) {
            for (const auto& item : items) {
                bool selected = (*flags & item.second) != 0;
                if (ImGui::Selectable(item.first.c_str(), selected)) {
                    if (selected) {
                        *flags &= ~item.second;
                    } else {
                        *flags |= item.second;
                    }
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }
        return changed;
    }

    // Notifications & Feedback (simplified implementations)
    void Toast(const std::string& message, float duration, const ImVec4& color) {
        // TODO: Implement toast notification system with queue
        // For now, just show as a tooltip-like popup
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 300, 50), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.8f);
        if (ImGui::Begin("##Toast", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::Text("%s", message.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::End();
    }

    void StatusBar(const std::string& text, const ImVec4& color) {
        ImVec2 pos = ImGui::GetMainViewport()->Pos;
        ImVec2 size = ImGui::GetMainViewport()->Size;
        ImDrawList* drawList = ImGui::GetBackgroundDrawList();
        if (drawList) {
            ImVec2 textPos = ImVec2(pos.x + 10, pos.y + size.y - 20);
            drawList->AddText(textPos, ImGui::ColorConvertFloat4ToU32(color), text.c_str());
        }
    }

    // Property & Data Editors
    void Value(const std::string& prefix, bool b) {
        ImGui::Value(prefix.c_str(), b);
    }

    void Value(const std::string& prefix, int v) {
        ImGui::Value(prefix.c_str(), v);
    }

    void Value(const std::string& prefix, unsigned int v) {
        ImGui::Value(prefix.c_str(), v);
    }

    void Value(const std::string& prefix, float v, const std::string& float_format) {
        ImGui::Value(prefix.c_str(), v, float_format.empty() ? nullptr : float_format.c_str());
    }

    void BulletText(const std::string& text) {
        ImGui::BulletText("%s", text.c_str());
    }

    void TextWrapped(const std::string& text) {
        ImGui::TextWrapped("%s", text.c_str());
    }

    // Console & Debug
    void Console(const std::string& label, std::vector<std::string>& log_lines, std::string& input_buffer, bool* p_open) {
        if (ImGui::Begin(label.c_str(), p_open)) {
            // Log display
            if (ImGui::BeginChild("##Log", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), false)) {
                for (const auto& line : log_lines) {
                    ImGui::TextUnformatted(line.c_str());
                }
                if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                    ImGui::SetScrollHereY(1.0f);
                }
            }
            ImGui::EndChild();

            // Input
            ImGui::Separator();
            bool reclaim_focus = false;
            if (ImGui::InputText("##Input", (char*)input_buffer.c_str(), input_buffer.capacity() + 1, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackResize, InputTextCallback, &input_buffer)) {
                if (!input_buffer.empty()) {
                    log_lines.push_back("> " + input_buffer);
                    input_buffer.clear();
                    reclaim_focus = true;
                }
            }
            ImGui::SetItemDefaultFocus();
            if (reclaim_focus) {
                ImGui::SetKeyboardFocusHere(-1);
            }
        }
        ImGui::End();
    }

    void LogWindow(const std::string& label, const std::vector<std::string>& log_lines, bool* p_open) {
        if (ImGui::Begin(label.c_str(), p_open)) {
            if (ImGui::BeginChild("##Log", ImVec2(0, 0), false)) {
                for (const auto& line : log_lines) {
                    ImGui::TextUnformatted(line.c_str());
                }
                if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                    ImGui::SetScrollHereY(1.0f);
                }
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }

    // Drag & Drop
    bool BeginDragDropSource(ImGuiDragDropFlags flags) {
        return ImGui::BeginDragDropSource(flags);
    }

    void SetDragDropPayload(const std::string& type, const void* data, size_t sz, ImGuiCond cond) {
        ImGui::SetDragDropPayload(type.c_str(), data, sz, cond);
    }

    void EndDragDropSource() {
        ImGui::EndDragDropSource();
    }

    bool BeginDragDropTarget() {
        return ImGui::BeginDragDropTarget();
    }

    const ImGuiPayload* AcceptDragDropPayload(const std::string& type, ImGuiDragDropFlags flags) {
        return ImGui::AcceptDragDropPayload(type.c_str(), flags);
    }

    void EndDragDropTarget() {
        ImGui::EndDragDropTarget();
    }

    // Splitter - custom implementation since ImGui doesn't have this
    bool Splitter(const std::string& str_id, ImGuiAxis axis, float* size1, float* size2, float min_size1, float min_size2, float hover_extend, float hover_visibility_delay) {
        ImGuiContext& g = *ImGui::GetCurrentContext();
        ImGuiWindow* window = g.CurrentWindow;
        ImGuiID id = window->GetID(str_id.c_str());

        ImRect bb;
        if (axis == ImGuiAxis::X) {
            bb = ImRect(ImVec2(*size1, 0), ImVec2(*size1 + 4.0f, window->Size.y));
        } else {
            bb = ImRect(ImVec2(0, *size1), ImVec2(window->Size.x, *size1 + 4.0f));
        }

        ImGui::ItemAdd(bb, id);
        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, ImGuiButtonFlags_None);

        if (held) {
            ImVec2 mouse_delta = ImGui::GetIO().MouseDelta;
            if (axis == ImGuiAxis::X) {
                *size1 += mouse_delta.x;
            } else {
                *size1 += mouse_delta.y;
            }
            *size1 = (std::max)(min_size1, (std::min)(*size1, (axis == ImGuiAxis::X ? window->Size.x : window->Size.y) - min_size2 - 4.0f));
            *size2 = (axis == ImGuiAxis::X ? window->Size.x : window->Size.y) - *size1 - 4.0f;
        }

        // Draw splitter
        ImU32 col = ImGui::GetColorU32(held ? ImGuiCol_SeparatorActive : (hovered ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator));
        window->DrawList->AddRectFilled(bb.Min, bb.Max, col);

        return pressed;
    }

    // Motion Graphics Widgets
    bool AnimatedButton(const std::string& Label, const Solstice::UI::Animation::AnimationClip& Clip, const std::function<void()>& OnClick) {
        return MotionGraphics::AnimatedButton(Label, Clip, OnClick);
    }

    void AnimatedText(const std::string& Text, const Solstice::UI::Animation::AnimationClip& Clip) {
        MotionGraphics::AnimatedText(Text, Clip);
    }

    bool ButtonWithShadow(const std::string& Label, const Solstice::UI::ShadowParams& Shadow, const std::function<void()>& OnClick) {
        return MotionGraphics::ButtonWithShadow(Label, Shadow, OnClick);
    }

    void PanelWithBlur(const std::string& Id, const Solstice::UI::BlurParams& Blur, const std::function<void()>& Content) {
        MotionGraphics::PanelWithBlur(Id, Blur, Content);
    }

} // namespace Solstice::UI::Widgets
