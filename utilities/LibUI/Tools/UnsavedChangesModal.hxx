#pragma once

#include "LibUI/Core/Core.hxx"

namespace LibUI::Tools {

/// Result of the shared Save / Discard / Cancel row used in modal unsaved prompts.
enum class UnsavedModalResult { None, Save, Discard, Cancel };

/// Which buttons to show (Jackhammer: all three; SMM: discard + cancel only).
struct UnsavedModalButtonConfig {
    bool showSave = true;
    bool showDiscard = true;
    bool showCancel = true;

    static constexpr UnsavedModalButtonConfig Triple() { return {true, true, true}; }
    static constexpr UnsavedModalButtonConfig DiscardAndCancel() { return {false, true, true}; }
};

/// Draw the button row inside an ``ImGui::BeginPopupModal`` / ``Begin`` block, after the message text.
/// Call once per frame while the modal is open; only one of Save / Discard / Cancel returns non-None per frame.
LIBUI_API UnsavedModalResult DrawUnsavedModalButtons(const UnsavedModalButtonConfig& cfg = UnsavedModalButtonConfig::Triple(),
    float buttonWidth = 120.f);

} // namespace LibUI::Tools
