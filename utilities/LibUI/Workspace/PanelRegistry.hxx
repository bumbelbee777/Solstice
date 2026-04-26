#pragma once

#include "LibUI/Core/Core.hxx"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace LibUI::Workspace {

struct PanelEntry {
    std::string id;
    std::string title;
    bool* visible{nullptr};
    std::function<void()> draw;
};

class LIBUI_API PanelRegistry {
public:
    void RegisterPanel(PanelEntry entry);
    void DrawPanels() const;
    void DrawPanelVisibilityMenuItems() const;
    bool HasPanel(std::string_view id) const;

private:
    std::vector<PanelEntry> m_panels;
};

} // namespace LibUI::Workspace

