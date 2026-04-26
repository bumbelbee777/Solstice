#include "LibUI/Tools/ClipboardButton.hxx"

#include <imgui.h>

namespace LibUI::Tools {

bool CopyTextButton(const char* id, const char* textUtf8, const char* buttonLabel) {
    if (!id || !textUtf8 || !textUtf8[0]) {
        return false;
    }
    ImGui::PushID(id);
    const bool clicked = ImGui::SmallButton(buttonLabel ? buttonLabel : "Copy");
    ImGui::PopID();
    if (clicked) {
        ImGui::SetClipboardText(textUtf8);
    }
    return clicked;
}

} // namespace LibUI::Tools
