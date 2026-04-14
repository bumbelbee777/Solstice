#pragma once

#include <string>
#include <functional>
#include <vector>
#include <imgui.h>
#include <UI/Motion/Animation.hxx>
#include <UI/Motion/VisualEffects.hxx>

namespace Solstice::UI::Widgets {

    // Basic Layout
    void BeginWindow(const std::string& title);
    void EndWindow();
    void SameLine();
    void Separator();
    void Spacing();

    // Advanced Layout
    bool CollapsingHeader(const std::string& label, bool defaultOpen = false);
    void BeginChild(const std::string& id, float width = 0.0f, float height = 0.0f, bool border = false);
    void EndChild();
    void Tooltip(const std::string& text);

    // Text
    void Text(const std::string& content);
    void Label(const std::string& label, const std::string& text);

    // Text Styles
    void TextBold(const std::string& text);
    void TextUnderlined(const std::string& text, const ImVec4& color = ImVec4(1,1,1,1));
    void TextStrikethrough(const std::string& text, const ImVec4& color = ImVec4(1,1,1,1));

    // Text Animations
    void TypewriterText(const std::string& text, float progress); // progress 0.0 to 1.0
    void PulsingText(const std::string& text, float time, float speed = 1.0f);
    void RainbowText(const std::string& text, float time, float speed = 1.0f);

    // Controls
    bool Button(const std::string& label, const std::function<void()>& onClick = nullptr);
    bool Checkbox(const std::string& label, bool& value);
    bool ColorEdit3(const std::string& label, float* col);
    bool ColorEdit4(const std::string& label, float* col);
    bool Combo(const std::string& label, int* current_item, const std::vector<std::string>& items);
    void ProgressBar(float fraction, const std::string& overlay = "");

    // Sliders
    bool SliderFloat(const std::string& label, float& value, float min, float max);
    bool SliderInt(const std::string& label, int& value, int min, int max);
    bool SliderFloat3(const std::string& label, float* value, float min, float max);

    // Input
    bool InputText(const std::string& label, std::string& value);

    // Effects
    void PushTransparency(float alpha);
    void PopTransparency();

    // Dialogs
    bool BeginPopupModal(const std::string& name, bool* p_open = nullptr);
    void EndPopup();
    void OpenPopup(const std::string& name);
    void CloseCurrentPopup();
    void ConfirmationDialog(const std::string& id, const std::string& title, const std::string& message, const std::function<void()>& onConfirm);

    // Enhanced Dialogs
    bool BeginDialog(const std::string& title, bool* p_open = nullptr, ImGuiWindowFlags flags = 0);
    void EndDialog();

    // Color & Visual Selection
    bool ColorPicker(const std::string& label, float col[3], ImGuiColorEditFlags flags = 0);
    bool ColorPicker4(const std::string& label, float col[4], ImGuiColorEditFlags flags = 0);
    bool ColorPalette(const std::string& label, float* col, const std::vector<ImVec4>& palette);
    bool ColorButton(const std::string& desc_id, const ImVec4& col, ImGuiColorEditFlags flags = 0, ImVec2 size = ImVec2(0, 0));

    // File & Path Management
    bool FileDialog(const std::string& label, std::string& path, const std::string& filter = "");
    bool DirectoryDialog(const std::string& label, std::string& path);
    bool PathInput(const std::string& label, std::string& path, const std::string& button_label = "Browse");

    // Data Display & Input
    bool BeginTable(const std::string& str_id, int column, ImGuiTableFlags flags = 0, const ImVec2& outer_size = ImVec2(0, 0), float inner_width = 0.0f);
    void EndTable();
    void TableNextRow(ImGuiTableRowFlags row_flags = 0, float min_row_height = 0.0f);
    bool TableNextColumn();
    bool TableSetColumnIndex(int column_n);
    void TableSetupColumn(const std::string& label, ImGuiTableColumnFlags flags = 0, float init_width_or_weight = 0.0f, ImU32 user_id = 0);
    void TableSetupScrollFreeze(int cols, int rows);
    void TableHeadersRow();
    void TableHeader(const std::string& label);

    bool ListBox(const std::string& label, int* current_item, const std::vector<std::string>& items, int height_in_items = -1);
    bool ListBoxHeader(const std::string& label, const ImVec2& size = ImVec2(0, 0));
    void ListBoxFooter();
    bool Selectable(const std::string& label, bool selected = false, ImGuiSelectableFlags flags = 0, const ImVec2& size = ImVec2(0, 0));
    bool Selectable(const std::string& label, bool* p_selected, ImGuiSelectableFlags flags = 0, const ImVec2& size = ImVec2(0, 0));

    // Enhanced Input
    bool InputTextMultiline(const std::string& label, std::string& value, const ImVec2& size = ImVec2(0, 0), ImGuiInputTextFlags flags = 0);
    bool InputFloat(const std::string& label, float* v, float step = 0.0f, float step_fast = 0.0f, const std::string& format = "%.3f", ImGuiInputTextFlags flags = 0);
    bool InputInt(const std::string& label, int* v, int step = 1, int step_fast = 100, ImGuiInputTextFlags flags = 0);
    bool InputFloat2(const std::string& label, float v[2], const std::string& format = "%.3f", ImGuiInputTextFlags flags = 0);
    bool InputFloat3(const std::string& label, float v[3], const std::string& format = "%.3f", ImGuiInputTextFlags flags = 0);
    bool InputFloat4(const std::string& label, float v[4], const std::string& format = "%.3f", ImGuiInputTextFlags flags = 0);
    bool InputInt2(const std::string& label, int v[2], ImGuiInputTextFlags flags = 0);
    bool InputInt3(const std::string& label, int v[3], ImGuiInputTextFlags flags = 0);
    bool InputInt4(const std::string& label, int v[4], ImGuiInputTextFlags flags = 0);

    bool DragFloat(const std::string& label, float* v, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const std::string& format = "%.3f", ImGuiSliderFlags flags = 0);
    bool DragInt(const std::string& label, int* v, float v_speed = 1.0f, int v_min = 0, int v_max = 0, const std::string& format = "%d", ImGuiSliderFlags flags = 0);
    bool DragFloat2(const std::string& label, float v[2], float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const std::string& format = "%.3f", ImGuiSliderFlags flags = 0);
    bool DragFloat3(const std::string& label, float v[3], float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const std::string& format = "%.3f", ImGuiSliderFlags flags = 0);
    bool DragFloat4(const std::string& label, float v[4], float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const std::string& format = "%.3f", ImGuiSliderFlags flags = 0);
    bool DragInt2(const std::string& label, int v[2], float v_speed = 1.0f, int v_min = 0, int v_max = 0, const std::string& format = "%d", ImGuiSliderFlags flags = 0);
    bool DragInt3(const std::string& label, int v[3], float v_speed = 1.0f, int v_min = 0, int v_max = 0, const std::string& format = "%d", ImGuiSliderFlags flags = 0);
    bool DragInt4(const std::string& label, int v[4], float v_speed = 1.0f, int v_min = 0, int v_max = 0, const std::string& format = "%d", ImGuiSliderFlags flags = 0);

    // Enhanced Sliders
    bool SliderAngle(const std::string& label, float* v_rad, float v_degrees_min = -360.0f, float v_degrees_max = +360.0f, const std::string& format = "%.0f deg", ImGuiSliderFlags flags = 0);
    bool SliderFloat2(const std::string& label, float v[2], float v_min, float v_max, const std::string& format = "%.3f", ImGuiSliderFlags flags = 0);
    bool SliderFloat4(const std::string& label, float v[4], float v_min, float v_max, const std::string& format = "%.3f", ImGuiSliderFlags flags = 0);
    bool SliderInt2(const std::string& label, int v[2], int v_min, int v_max, const std::string& format = "%d", ImGuiSliderFlags flags = 0);
    bool SliderInt3(const std::string& label, int v[3], int v_min, int v_max, const std::string& format = "%d", ImGuiSliderFlags flags = 0);
    bool SliderInt4(const std::string& label, int v[4], int v_min, int v_max, const std::string& format = "%d", ImGuiSliderFlags flags = 0);
    bool VSliderFloat(const std::string& label, const ImVec2& size, float* v, float v_min, float v_max, const std::string& format = "%.3f", ImGuiSliderFlags flags = 0);
    bool VSliderInt(const std::string& label, const ImVec2& size, int* v, int v_min, int v_max, const std::string& format = "%d", ImGuiSliderFlags flags = 0);

    // Layout & Navigation
    bool BeginTabBar(const std::string& str_id, ImGuiTabBarFlags flags = 0);
    void EndTabBar();
    bool BeginTabItem(const std::string& label, bool* p_open = nullptr, ImGuiTabItemFlags flags = 0);
    void EndTabItem();
    void SetTabItemClosed(const std::string& tab_or_docked_window_label);

    bool TreeNode(const std::string& label);
    bool TreeNode(const std::string& str_id, const std::string& fmt);
    bool TreeNodeEx(const std::string& label, ImGuiTreeNodeFlags flags = 0);
    bool TreeNodeEx(const std::string& str_id, ImGuiTreeNodeFlags flags, const std::string& fmt);
    void TreePop();
    void SetNextItemOpen(bool is_open, ImGuiCond cond = 0);

    bool BeginMenuBar();
    void EndMenuBar();
    bool BeginMenu(const std::string& label, bool enabled = true);
    void EndMenu();
    bool MenuItem(const std::string& label, const std::string& shortcut = "", bool selected = false, bool enabled = true);
    bool MenuItem(const std::string& label, const std::string& shortcut, bool* p_selected, bool enabled = true);

    void OpenPopupOnItemClick(const std::string& str_id = "", ImGuiPopupFlags popup_flags = 1);
    bool BeginPopupContextItem(const std::string& str_id = "", ImGuiPopupFlags popup_flags = 1);
    bool BeginPopupContextWindow(const std::string& str_id = "", ImGuiPopupFlags popup_flags = 1);
    bool BeginPopupContextVoid(const std::string& str_id = "", ImGuiPopupFlags popup_flags = 1);

    void EnhancedTooltip(const std::string& text, const ImVec4& color = ImVec4(1, 1, 1, 1));

    // Media & Graphics
    void Image(ImTextureID user_texture_id, const ImVec2& size, const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1), const ImVec4& tint_col = ImVec4(1, 1, 1, 1), const ImVec4& border_col = ImVec4(0, 0, 0, 0));
    bool ImageButton(const std::string& str_id, ImTextureID user_texture_id, const ImVec2& size, const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1), int frame_padding = -1, const ImVec4& bg_col = ImVec4(0, 0, 0, 0), const ImVec4& tint_col = ImVec4(1, 1, 1, 1));

    void PlotLines(const std::string& label, const float* values, int values_count, int values_offset = 0, const std::string& overlay_text = "", float scale_min = FLT_MAX, float scale_max = FLT_MAX, ImVec2 graph_size = ImVec2(0, 0), int stride = sizeof(float));
    void PlotHistogram(const std::string& label, const float* values, int values_count, int values_offset = 0, const std::string& overlay_text = "", float scale_min = FLT_MAX, float scale_max = FLT_MAX, ImVec2 graph_size = ImVec2(0, 0), int stride = sizeof(float));

    void CircularProgressBar(float fraction, float radius = 20.0f, const std::string& overlay = "");
    void LoadingSpinner(const std::string& label, float radius = 10.0f, float thickness = 2.0f, const ImVec4& color = ImVec4(1, 1, 1, 1));

    // Advanced Controls
    bool SmallButton(const std::string& label);
    bool ArrowButton(const std::string& str_id, ImGuiDir dir);
    bool InvisibleButton(const std::string& str_id, const ImVec2& size, ImGuiButtonFlags flags = 0);
    bool RadioButton(const std::string& label, bool active);
    bool RadioButton(const std::string& label, int* v, int v_button);

    bool ComboFlags(const std::string& label, unsigned int* flags, const std::vector<std::pair<std::string, unsigned int>>& items);

    // Notifications & Feedback
    void Toast(const std::string& message, float duration = 3.0f, const ImVec4& color = ImVec4(1, 1, 1, 1));
    void StatusBar(const std::string& text, const ImVec4& color = ImVec4(1, 1, 1, 1));

    // Property & Data Editors
    void Value(const std::string& prefix, bool b);
    void Value(const std::string& prefix, int v);
    void Value(const std::string& prefix, unsigned int v);
    void Value(const std::string& prefix, float v, const std::string& float_format = "");
    void BulletText(const std::string& text);
    void TextWrapped(const std::string& text);

    // Console & Debug
    void Console(const std::string& label, std::vector<std::string>& log_lines, std::string& input_buffer, bool* p_open = nullptr);
    void LogWindow(const std::string& label, const std::vector<std::string>& log_lines, bool* p_open = nullptr);

    // Drag & Drop
    bool BeginDragDropSource(ImGuiDragDropFlags flags = 0);
    void SetDragDropPayload(const std::string& type, const void* data, size_t sz, ImGuiCond cond = 0);
    void EndDragDropSource();
    bool BeginDragDropTarget();
    const ImGuiPayload* AcceptDragDropPayload(const std::string& type, ImGuiDragDropFlags flags = 0);
    void EndDragDropTarget();

    // Splitter
    enum class ImGuiAxis {
        X = 0,
        Y = 1
    };
    bool Splitter(const std::string& str_id, ImGuiAxis axis, float* size1, float* size2, float min_size1, float min_size2, float hover_extend = 0.0f, float hover_visibility_delay = 0.0f);

    // Motion Graphics Widgets
    bool AnimatedButton(const std::string& Label, const Solstice::UI::Animation::AnimationClip& Clip, const std::function<void()>& OnClick = nullptr);
    void AnimatedText(const std::string& Text, const Solstice::UI::Animation::AnimationClip& Clip);
    bool ButtonWithShadow(const std::string& Label, const Solstice::UI::ShadowParams& Shadow, const std::function<void()>& OnClick = nullptr);
    void PanelWithBlur(const std::string& Id, const Solstice::UI::BlurParams& Blur, const std::function<void()>& Content);
} // namespace Solstice::UI::Widgets
