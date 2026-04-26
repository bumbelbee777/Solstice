#pragma once

#include "LibUI/Graphics/PreviewTexture.hxx"
#include "LibUI/Viewport/Viewport.hxx"

#include <Math/Vector.hxx>

#include <Parallax/DevSessionAssetResolver.hxx>
#include <Parallax/ParallaxScene.hxx>

#include <functional>

struct SDL_Window;

namespace Smm::Editing {
struct ParticleEditorState;
}

namespace Solstice::MovieMaker::UI::Panels {

/// Tunables for `DrawUnifiedViewportPanel` (camera + optional `.smat` preview).
struct UnifiedViewportSettings {
    LibUI::Viewport::OrbitPanZoomState* camera{nullptr};
    int selectedElementIndex{-1};
    const char* previewSmatUtf8{nullptr};
    bool usePreviewSmat{false};
    bool smatActorsOnly{true};
    bool smatSelectedOnly{false};
    bool bindPreviewMaterialMaps{false};
    const char* previewMaterialAlbedoUtf8{nullptr};
    const char* previewMaterialNormalUtf8{nullptr};
    const char* previewMaterialRoughnessUtf8{nullptr};
    /// If set, Shift+LMB click (no drag) ray-picks schematic cubes; `elementIndex` is Parallax `ElementIndex`, or `-1` for miss.
    std::function<void(int elementIndex)> onViewportPickElement{};
    bool showFluidVolumeOverlay{true};
    /// Title-safe, thirds, and center cross in screen space over the letterboxed preview.
    bool showFramingGuides{false};
    /// If non-null and `bytes` > 0, last `CaptureOrbitRgb` failure message (NUL-terminated).
    char* enginePreviewErrorSink{nullptr};
    size_t enginePreviewErrorSinkBytes{0};
};

void DrawScene3dSchematicPanel(SDL_Window* window, const Solstice::Parallax::ParallaxScene& scene, uint64_t timeTicks,
    LibUI::Graphics::PreviewTextureRgba& previewTexture, float preferredHeight = 200.0f);

void DrawMotionGraphicsPreviewPanel(SDL_Window* window, const Solstice::Parallax::ParallaxScene& scene,
    Solstice::Parallax::DevSessionAssetResolver& resolver, uint64_t timeTicks,
    LibUI::Graphics::PreviewTextureRgba& previewTexture, float preferredHeight = 160.0f);

/// Single viewport: schematic 3D capture + MG CPU raster composited (src-over), optional particle preview overlay.
void DrawUnifiedViewportPanel(SDL_Window* window, const Solstice::Parallax::ParallaxScene& scene,
    Solstice::Parallax::DevSessionAssetResolver& resolver, uint64_t timeTicks,
    LibUI::Graphics::PreviewTextureRgba& previewTexture, float preferredHeight, Smm::Editing::ParticleEditorState* particles,
    LibUI::Graphics::PreviewTextureRgba* particleSpriteTexture, const Solstice::Math::Vec3& emitterWorld, float mgOverlayAlpha,
    const UnifiedViewportSettings& settings);

} // namespace Solstice::MovieMaker::UI::Panels
