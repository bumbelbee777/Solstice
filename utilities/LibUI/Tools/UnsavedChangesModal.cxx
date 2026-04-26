#include "LibUI/Tools/UnsavedChangesModal.hxx"

#include <imgui.h>

namespace LibUI::Tools {

UnsavedModalResult DrawUnsavedModalButtons(const UnsavedModalButtonConfig& cfg, float buttonWidth) {
    UnsavedModalResult result = UnsavedModalResult::None;
    bool first = true;
    if (cfg.showSave) {
        if (ImGui::Button("Save", ImVec2(buttonWidth, 0))) {
            result = UnsavedModalResult::Save;
        }
        first = false;
    }
    if (cfg.showDiscard) {
        if (!first) {
            ImGui::SameLine();
        }
        if (ImGui::Button("Discard", ImVec2(buttonWidth, 0))) {
            result = UnsavedModalResult::Discard;
        }
        first = false;
    }
    if (cfg.showCancel) {
        if (!first) {
            ImGui::SameLine();
        }
        if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0))) {
            result = UnsavedModalResult::Cancel;
        }
    }
    return result;
}

} // namespace LibUI::Tools
