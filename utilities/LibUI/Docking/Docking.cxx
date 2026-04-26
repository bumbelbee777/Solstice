#include "LibUI/Docking/Docking.hxx"

#include "LibUI/Shell/MainHost.hxx"

#include <imgui.h>

namespace LibUI::Docking {

void BeginDockspaceHost(const char* imguiId, ImGuiWindowFlags hostFlags, ImGuiID* outDockspaceId) {
    LibUI::Shell::BeginMainHostWindow(imguiId, hostFlags);
#ifdef IMGUI_HAS_DOCK
    if (!IsDockingEnabled()) {
        if (outDockspaceId) {
            *outDockspaceId = 0;
        }
        return;
    }

    ImGuiID dockspaceId = ImGui::GetID("PrimaryDockspace");
    const ImGuiDockNodeFlags dockFlags = ImGuiDockNodeFlags_PassthruCentralNode;
    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), dockFlags);
    if (outDockspaceId) {
        *outDockspaceId = dockspaceId;
    }
#else
    if (outDockspaceId) {
        *outDockspaceId = 0;
    }
#endif
}

void EndDockspaceHost() {
    LibUI::Shell::EndMainHostWindow();
}

void DrawWorkspaceMenuItems(WorkspaceState& state) {
    if (!IsDockingEnabled()) {
        ImGui::BeginDisabled();
        ImGui::MenuItem("Docking unavailable (ImGui docking disabled)");
        ImGui::EndDisabled();
        return;
    }
    if (ImGui::MenuItem("Reset workspace layout")) {
        state.requestResetLayout = true;
    }
}

bool IsDockingEnabled() {
#ifdef IMGUI_HAS_DOCK
    return (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable) != 0;
#else
    return false;
#endif
}

} // namespace LibUI::Docking

