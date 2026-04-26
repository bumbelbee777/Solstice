#pragma once

#include "LibUI/Core/Core.hxx"

#include <imgui.h>

namespace LibUI::Tools {

/// Collapsible list of paths from ``LibUI::Core::RecentPathPush`` / ``RecentPathGet*`` (shared across tools).
/// No-op when the recent list is empty. ``onOpenPath`` is invoked when the user clicks **Open** for a row.
LIBUI_API void DrawRecentPathsCollapsible(const char* headerLabel, ImGuiTreeNodeFlags treeFlags, void* userData,
    void (*onOpenPath)(void* userData, const char* pathUtf8));

} // namespace LibUI::Tools
