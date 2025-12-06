#include <UI/Widgets.hxx>
#include <imgui.h>
#include <imgui_internal.h>
#include <cmath>

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

} // namespace Solstice::UI::Widgets
