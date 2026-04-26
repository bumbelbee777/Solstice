#pragma once

#include "../Editing/SmmGraphEditor.hxx"
#include "../Editing/SmmParticleEditor.hxx"
#include "../Editing/SmmCurveGraphBridge.hxx"
#include "LibUI/CurveEditor/CurveModel.hxx"
#include "LibUI/Timeline/TimelineModel.hxx"
#include "LibUI/Viewport/Viewport.hxx"

namespace Smm::UI {

struct WorkspaceState {
    bool showLegacyWorkspace{true};
    bool showTimelinePanel{false}; ///< Optional dock; main layout already has a timeline strip.
    bool showCurveEditorPanel{false};
    bool showGraphEditorPanel{false};
    bool showParticleEditorPanel{false};
    bool showFluidVolumesPanel{false};
    bool showDopesheetPanel{false};
    bool showUtilitiesPanel{false};
    bool showDiagnosticsPanel{false};
    bool showCommandPalette{false};
    
    // Core SMM Panels (fixed layout panes; visibility toggles hide/show regions)
    bool showProjectSettingsPanel{true};
    /// Import/export, video, ffmpeg — separate from project file controls (was double-bound to showProjectSettingsPanel).
    bool showExportPipelinePanel{true};
    bool showSceneOutlinerPanel{true};
    bool showPropertiesPanel{true};
    bool showAssetBrowserPanel{true};
    bool show3DViewportPanel{true};
    bool show2DMGPreviewPanel{true}; ///< Legacy: unified viewport supersedes split MG tab.
    bool showUnifiedViewportPanel{true};
    float mgOverlayAlpha{1.0f};
    /// Shared camera for unified viewport (persists across frames; reset from Viewer chrome).
    LibUI::Viewport::OrbitPanZoomState unifiedViewportCamera{};
    /// Optional engine `.smat` applied to schematic preview cubes (see PreviewPanels / EditorEnginePreview).
    bool previewUseSmat{false};
    bool previewSmatActorsOnly{true};
    bool previewSmatSelectedOnly{false};
    char previewSmatPath[768]{};
    /// Optional raster maps for schematic preview (same element filter as `.smat`).
    bool previewBindMaterialMaps{false};
    char previewMaterialAlbedoPath[768]{};
    char previewMaterialNormalPath[768]{};
    char previewMaterialRoughnessPath[768]{};
    bool showSessionTimelinePanel{true};
    bool applySfmTheme{true};

    LibUI::Timeline::TimelineState timelineState{};
    LibUI::CurveEditor::CurveEditorState curveEditorState{};
    Smm::Editing::GraphEditorState graphEditorState{};
    Smm::Editing::ParticleEditorState particleEditorState{};
    /// List index into `SmmFluidVolumeElement` rows (see fluid volumes panel).
    int fluidVolumeListSelectedIndex{-1};
    /// Draw Jackhammer-style fluid AABB overlay in the unified viewport.
    bool showFluidVolumeOverlay{true};
    /// 2D overlays: title-safe, rule-of-thirds, center cross (letterboxed image region of the unified viewport).
    bool showViewportFramingGuides{false};
    char enginePreviewLastError[512]{};

    Smm::Editing::KeyframeEditUiState keyframeEditState{};
};

} // namespace Smm::UI

