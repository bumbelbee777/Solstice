#pragma once

#include "LibUI/Core/Core.hxx"

#include <functional>

namespace LibUI::Tools {

/// Two-button “file changed on disk” modal. When ``*showOpen``, opens the popup on the next frame.
/// Call each frame from the main host while ``*showOpen`` may be true; pass stable ``popupId`` (##-scoped).
LIBUI_API void DrawFileChangedOnDiskModal(const char* popupId, bool* showOpen, const char* message,
    const char* primaryLabel, const char* secondaryLabel, float buttonWidth,
    const std::function<void()>& onPrimary, const std::function<void()>& onSecondary,
    const std::function<void()>& onAfterClose = {});

} // namespace LibUI::Tools
