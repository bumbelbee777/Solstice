#pragma once

#include "LibUI/Core/Core.hxx"

#include <imgui.h>

namespace LibUI::Docking {

struct WorkspaceState {
    bool requestResetLayout{false};
};

LIBUI_API void BeginDockspaceHost(const char* imguiId, ImGuiWindowFlags hostFlags, ImGuiID* outDockspaceId = nullptr);
LIBUI_API void EndDockspaceHost();
LIBUI_API void DrawWorkspaceMenuItems(WorkspaceState& state);
LIBUI_API bool IsDockingEnabled();

} // namespace LibUI::Docking

