#include "LibUI/Workspace/PanelRegistry.hxx"

#include <imgui.h>

namespace LibUI::Workspace {

void PanelRegistry::RegisterPanel(PanelEntry entry) {
    if (entry.id.empty() || entry.title.empty() || !entry.visible || !entry.draw) {
        return;
    }
    if (HasPanel(entry.id)) {
        return;
    }
    m_panels.push_back(std::move(entry));
}

void PanelRegistry::DrawPanels() const {
    for (const PanelEntry& panel : m_panels) {
        if (!panel.visible || !(*panel.visible)) {
            continue;
        }
        panel.draw();
    }
}

void PanelRegistry::DrawPanelVisibilityMenuItems() const {
    for (const PanelEntry& panel : m_panels) {
        if (!panel.visible) {
            continue;
        }
        ImGui::MenuItem(panel.title.c_str(), nullptr, panel.visible);
    }
}

bool PanelRegistry::HasPanel(std::string_view id) const {
    for (const PanelEntry& panel : m_panels) {
        if (panel.id == id) {
            return true;
        }
    }
    return false;
}

} // namespace LibUI::Workspace

