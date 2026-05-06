#pragma once

#include <EditorEnginePreview/EditorEnginePreview.hxx>
#include <Math/Vector.hxx>
#include <Parallax/ParallaxScene.hxx>
#include <Parallax/ParallaxTypes.hxx>
#include <Physics/Lighting/LightSource.hxx>

#include <vector>

namespace LibUI::Viewport {
struct OrbitPanZoomState;
}

namespace Solstice::MovieMaker::Export {

/// Output bundle from `BuildPreviewEntitiesAndLights`. Lifetimes are tied to the caller-owned
/// vectors, so this is a thin "view assembler" without per-frame allocations beyond `vector`
/// growth which is amortized when the vectors are reused across frames.
struct PreviewMappingResult {
    std::vector<Solstice::EditorEnginePreview::PreviewEntity> Entities;
    std::vector<Solstice::Physics::LightSource> Lights;
};

/// Camera derivation result for the unified viewport / export. `Found` is false when the scene
/// has no `CameraElement`, which signals callers to keep the user-driven orbit nav.
struct PreviewCameraPose {
    bool Found{false};
    Math::Vec3 Target{0.f, 0.f, 0.f};
    float FovYDeg{55.f};
    /// Built up to nav fields the unified viewport already understands.
    float NavYaw{0.f};
    float NavPitch{0.f};
    float NavDistance{8.f};
    float NavPanX{0.f};
    float NavPanY{0.f};
};

/// Builds the unified preview/export entity + light list from an evaluated scene and authored
/// scene attributes. Mirrors the previous inline conversion in `VideoExport.cxx` so preview and
/// export render the same proxy geometry.
///
/// The output `result.Entities` and `result.Lights` are cleared first; vectors are reused for
/// efficiency on long export runs.
void BuildPreviewEntitiesAndLights(const Solstice::Parallax::ParallaxScene& scene,
    const Solstice::Parallax::SceneEvaluationResult& eval, PreviewMappingResult& result);

/// Resolves the active scene camera for preview/export. Returns `Found = false` when no camera
/// is present (caller keeps user-driven orbit nav). Reads `Target` and `FovDegrees` attributes
/// from the camera element when present.
PreviewCameraPose ResolvePreviewCameraPose(const Solstice::Parallax::ParallaxScene& scene,
    const Solstice::Parallax::SceneEvaluationResult& eval);

} // namespace Solstice::MovieMaker::Export
