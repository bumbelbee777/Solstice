#pragma once

#include "LibUI/Graphics/PreviewTexture.hxx"
#include "LibUI/Viewport/Viewport.hxx"

#include <Math/Vector.hxx>

#include <unordered_set>

#include <Parallax/DevSessionAssetResolver.hxx>
#include <Parallax/ParallaxScene.hxx>

#include <functional>

struct SDL_Window;

namespace Solstice::EditorEnginePreview {
struct CinematicViewStatePod;
}

namespace Smm::Editing {
struct ParticleEditorState;
}

namespace Solstice::MovieMaker::UI::Panels {

/// Tunables for `DrawUnifiedViewportPanel` (camera + optional `.smat` preview).
struct UnifiedViewportSettings {
    LibUI::Viewport::OrbitPanZoomState* camera{nullptr};
    /// Primary Parallax element index for property panels / focus (written by viewport interaction when non-null).
    int* primaryElementIndex{nullptr};
    /// When non-null, multi-select highlights + pick/drag use this set (Parallax element indices).
    std::unordered_set<int>* viewportSelectedElements{nullptr};
    /// Selected MG row index (for 2D sprite viewport manipulator overlays).
    int* selectedMgElementIndex{nullptr};
    bool compressPrlxForUndo{false};
    /// Written when viewport edits mutate the Parallax scene (drag, delete from menu, etc.).
    bool* sceneDirty{nullptr};
    const char* previewSmatUtf8{nullptr};
    bool usePreviewSmat{false};
    bool smatActorsOnly{true};
    bool smatSelectedOnly{false};
    bool bindPreviewMaterialMaps{false};
    const char* previewMaterialAlbedoUtf8{nullptr};
    const char* previewMaterialNormalUtf8{nullptr};
    const char* previewMaterialRoughnessUtf8{nullptr};
    /// Legacy single-element pick (optional). Prefer updating selection via `viewportSelectedElements` + `selectedElementIndex` in host.
    std::function<void(int elementIndex)> onViewportPickElement{};
    bool showFluidVolumeOverlay{true};
    /// Title-safe, thirds, and center cross in screen space over the letterboxed preview.
    bool showFramingGuides{false};
    /// If non-null and `bytes` > 0, last `CaptureOrbitRgb` failure message (NUL-terminated).
    char* enginePreviewErrorSink{nullptr};
    size_t enginePreviewErrorSinkBytes{0};
    /// Multiplier for schematic 3D “baked AO” darkening (0=off, ~0.3–0.5 typical). Drives `PreviewEntity::BakedAOPreview`.
    float lowPolyAOPreview{0.42f};
    /// When non-null, dragging the particle emitter hit updates manual world placement (preview-only until particles sync).
    bool* manualParticleEmitterWorld{nullptr};
    Solstice::Math::Vec3* manualParticleEmitterWorldVec{nullptr};
    /// If non-null, applied each frame to `EditorEnginePreview` 3D capture (`fs_post` chroma / smear / fog dither).
    const Solstice::EditorEnginePreview::CinematicViewStatePod* cinematicView3D{nullptr};
    /// If non-null and `*enginePreviewSessionDisabled` is true, skip GPU capture for the session (CPU MG fallback only).
    bool* enginePreviewSessionDisabled{nullptr};
    /// 0 = pure 2D MG workflow (legacy), 1 = unified world MG workflow.
    int mgWorkflowMode{0};
    /// In pure 2D workflow, force MG-only capture path (no 3D background).
    bool disable3DInPure2D{true};
};

/// Call after creating or replacing the Parallax scene so the first frames avoid immediate bgfx capture (startup/new-scene stability).
void ResetUnifiedViewportEnginePreviewWarmup();

void DrawScene3dSchematicPanel(SDL_Window* window, const Solstice::Parallax::ParallaxScene& scene, uint64_t timeTicks,
    LibUI::Graphics::PreviewTextureRgba& previewTexture, float preferredHeight = 200.0f);

void DrawMotionGraphicsPreviewPanel(SDL_Window* window, const Solstice::Parallax::ParallaxScene& scene,
    Solstice::Parallax::DevSessionAssetResolver& resolver, uint64_t timeTicks,
    LibUI::Graphics::PreviewTextureRgba& previewTexture, float preferredHeight = 160.0f);

/// Single viewport: schematic 3D capture + MG CPU raster composited (src-over), optional particle preview overlay.
void DrawUnifiedViewportPanel(SDL_Window* window, Solstice::Parallax::ParallaxScene& scene,
    Solstice::Parallax::DevSessionAssetResolver& resolver, uint64_t timeTicks,
    LibUI::Graphics::PreviewTextureRgba& previewTexture, float preferredHeight, Smm::Editing::ParticleEditorState* particles,
    LibUI::Graphics::PreviewTextureRgba* particleSpriteTexture, const Solstice::Math::Vec3& emitterWorld, float mgOverlayAlpha,
    const UnifiedViewportSettings& settings);

} // namespace Solstice::MovieMaker::UI::Panels
