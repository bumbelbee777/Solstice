#pragma once

#include "../Editing/SmmGraphEditor.hxx"
#include "../Editing/SmmParticleEditor.hxx"
#include "../Editing/SmmCurveGraphBridge.hxx"
#include "LibUI/CurveEditor/CurveModel.hxx"
#include "LibUI/Timeline/TimelineModel.hxx"
#include "LibUI/Viewport/Viewport.hxx"

#include <Math/Vector.hxx>

#include <unordered_set>

namespace Smm::UI {

enum class UnifiedMgWorkflowMode : int {
    Pure2D = 0,
    Unified3D = 1,
};

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
    /// Global MG authoring workflow for the unified viewport.
    UnifiedMgWorkflowMode unifiedMgWorkflowMode{UnifiedMgWorkflowMode::Pure2D};
    /// In Pure2D workflow, skip the 3D preview capture and show MG-only viewport output.
    bool pure2DDisable3DBackground{true};
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
    /// Schematic 3D preview: fake baked ambient occlusion strength on default materials (unified + split schematic if used).
    float schematicBakedAO{0.42f};
    char enginePreviewLastError[512]{};
    /// When true, unified viewport skips bgfx `CaptureOrbitRgb` (MG/CPU-only) after repeated GPU failures.
    bool enginePreviewSessionDisabled{false};

    /// Unified viewport: manual particle preview origin (when `manualParticleEmitterWorld` is true).
    bool manualParticleEmitterWorld{false};
    Solstice::Math::Vec3 manualParticleEmitterWorldVec{0.f, 1.2f, 0.f};
    /// Parallax element indices selected in the unified viewport (multi-select). Primary UI uses `elementSelected` in Main.
    std::unordered_set<int> viewportSelectedElements{};

    Smm::Editing::KeyframeEditUiState keyframeEditState{};
};

} // namespace Smm::UI

