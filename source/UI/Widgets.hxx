#pragma once

#include <string>
#include <functional>
#include <vector>
#include <imgui.h>

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

} // namespace Solstice::UI::Widgets
