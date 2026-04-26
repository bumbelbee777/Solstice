#include "ComingSoonPanels.hxx"

#include "../../Editing/SmmCurveEditor.hxx"
#include "../../Editing/SmmGraphEditor.hxx"
#include "../../Editing/SmmParticleEditor.hxx"
#include "../../Editing/SmmFluidPanel.hxx"
#include "LibUI/Timeline/TimelineWidget.hxx"
#include "LibUI/Tools/ToolScaffolds/ComingSoonPanel.hxx"

namespace Smm::UI::Panels {

void RegisterPlaceholderPanels(LibUI::Workspace::PanelRegistry& registry, WorkspaceState& state,
    Smm::Editing::AppSessionContext* session) {
    registry.RegisterPanel({
        "timeline.panel",
        "Timeline",
        &state.showTimelinePanel,
        [&state]() { LibUI::Timeline::DrawTimelineWidget("Timeline##SMMDock", state.timelineState, &state.showTimelinePanel); },
    });
    registry.RegisterPanel({
        "curve.panel",
        "Curve Editor",
        &state.showCurveEditorPanel,
        [session, &state]() {
            if (!session) {
                return;
            }
            Smm::Editing::DrawCurveEditorSession("Curve Editor##SMMDock", &state.showCurveEditorPanel, *session,
                state.curveEditorState, state.timelineState);
        },
    });
    registry.RegisterPanel({
        "graph.panel",
        "Graph Editor",
        &state.showGraphEditorPanel,
        [session, &state]() {
            if (!session || !session->bindings) {
                return;
            }
            Smm::Editing::DrawGraphEditorSession("Graph Editor##SMMDock", &state.showGraphEditorPanel, *session,
                state.graphEditorState, state.timelineState, *session->bindings);
        },
    });
    registry.RegisterPanel({
        "particle.panel",
        "Particles",
        &state.showParticleEditorPanel,
        [session, &state]() {
            Smm::Editing::DrawParticleEditorPanel("Particles##SMMDock", &state.showParticleEditorPanel,
                state.particleEditorState, session ? session->mainWindow : nullptr, session);
        },
    });
    registry.RegisterPanel({
        "fluid.panel",
        "Fluid volumes",
        &state.showFluidVolumesPanel,
        [session, &state]() {
            if (!session || !session->scene || !session->timeTicks || !session->sceneDirty) {
                return;
            }
            Smm::Editing::DrawFluidVolumesPanel("Fluid volumes##SMMDock", &state.showFluidVolumesPanel, *session->scene,
                *session->timeTicks, state.fluidVolumeListSelectedIndex, session->compressPrlx, *session->sceneDirty);
        },
    });
    registry.RegisterPanel({
        "dopesheet.panel",
        "Dopesheet",
        &state.showDopesheetPanel,
        [&state]() {
            LibUI::Tools::ToolScaffolds::DrawComingSoonPanel("Dopesheet##SMMDock", &state.showDopesheetPanel,
                "Dopesheet scaffolded.", "Planned: lane operations, ripple edits, and marker-aware retiming.");
        },
    });
    registry.RegisterPanel({
        "utilities.panel",
        "Utilities",
        &state.showUtilitiesPanel,
        [&state]() {
            LibUI::Tools::ToolScaffolds::DrawComingSoonPanel("Utilities##SMMDock", &state.showUtilitiesPanel,
                "Utility shelf scaffolded.", "Planned: batch renaming, diagnostics exporters, and import assistants.");
        },
    });
    registry.RegisterPanel({
        "diagnostics.panel",
        "Diagnostics",
        &state.showDiagnosticsPanel,
        [&state]() {
            LibUI::Tools::ToolScaffolds::DrawComingSoonPanel("Diagnostics##SMMDock", &state.showDiagnosticsPanel,
                "Diagnostics scaffolded.", "Planned: perf capture, validation traces, and event timeline probes.");
        },
    });
}

void SeedPlaceholderData(WorkspaceState& state) {
    (void)state;
    // Timeline/curve data come from Parallax via BridgeSyncFromScene in Main.
}

} // namespace Smm::UI::Panels
