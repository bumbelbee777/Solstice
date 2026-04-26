#pragma once

#include "../../Editing/SmmCurveGraphBridge.hxx"
#include "LibUI/Workspace/PanelRegistry.hxx"
#include "../WorkspaceState.hxx"

namespace Smm::UI::Panels {

void RegisterPlaceholderPanels(LibUI::Workspace::PanelRegistry& registry, WorkspaceState& state,
    Smm::Editing::AppSessionContext* session);

void SeedPlaceholderData(WorkspaceState& state);

} // namespace Smm::UI::Panels

