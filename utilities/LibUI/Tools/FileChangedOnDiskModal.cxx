#include "LibUI/Tools/FileChangedOnDiskModal.hxx"

#include <imgui.h>

namespace LibUI::Tools {

void DrawFileChangedOnDiskModal(const char* popupId, bool* showOpen, const char* message, const char* primaryLabel,
    const char* secondaryLabel, float buttonWidth, const std::function<void()>& onPrimary,
    const std::function<void()>& onSecondary, const std::function<void()>& onAfterClose) {
    if (!showOpen || !popupId) {
        return;
    }
    if (*showOpen) {
        ImGui::OpenPopup(popupId);
    }
    if (ImGui::BeginPopupModal(popupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (message) {
            ImGui::TextUnformatted(message);
        }
        if (ImGui::Button(primaryLabel ? primaryLabel : "OK", ImVec2(buttonWidth, 0))) {
            onPrimary();
            *showOpen = false;
            if (onAfterClose) {
                onAfterClose();
            }
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::Button(secondaryLabel ? secondaryLabel : "Cancel", ImVec2(buttonWidth, 0))) {
            onSecondary();
            *showOpen = false;
            if (onAfterClose) {
                onAfterClose();
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

} // namespace LibUI::Tools
