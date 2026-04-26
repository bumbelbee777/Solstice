#pragma once

#include <cstddef>
#include <vector>

#include <Math/Vector.hxx>
#include <Physics/Lighting/LightSource.hxx>

namespace LibUI::Viewport {
struct OrbitPanZoomState;
}

namespace Solstice::EditorEnginePreview {

/// Half-extent for schematic SMM/LevelEditor preview cubes and ImGui hit-test/selection (must match `PreviewEntity` passed to `CaptureOrbitRgb`).
inline constexpr float kSchematicPreviewHalfExtent = 0.28f;

/// World-space cube instance for crude editor previews.
struct PreviewEntity {
    Math::Vec3 Position{0.f, 0.f, 0.f};
    Math::Vec3 Albedo{0.65f, 0.68f, 0.8f};
    float HalfExtent{kSchematicPreviewHalfExtent};
    /// Per-axis multiplier for preview cube size (1,1,1 = uniform from HalfExtent).
    Math::Vec3 Scale{1.f, 1.f, 1.f};
    /// Euler degrees for preview cube (Y-up): pitch (X), yaw (Y), roll (Z).
    float PitchDeg{0.f};
    float YawDeg{0.f};
    float RollDeg{0.f};
    /// UTF-8 path to engine `.smat` (Material v1). Empty = derive appearance from `Albedo` only.
    char PreviewSmatPath[768]{};
    /// Optional maps for bgfx preview (loaded into `TextureRegistry` for this frame only).
    char PreviewAlbedoTexturePath[768]{};
    char PreviewNormalTexturePath[768]{};
    char PreviewRoughnessTexturePath[768]{};
};

/// Creates hidden SDL window + `SoftwareRenderer` once (idempotent).
bool EnsureInitialized();
void Shutdown();

/// Renders lit meshes (ground plane + cubes) to an offscreen target and returns RGBA8 top-down.
/// `viewportAspect` is width/height. `lights` may be empty (default sun is used).
bool CaptureOrbitRgb(const LibUI::Viewport::OrbitPanZoomState& orbit, float targetX, float targetY, float targetZ,
    float fovYDeg, float viewportAspect, int framebufferWidth, int framebufferHeight,
    const PreviewEntity* entities, size_t entityCount, const Physics::LightSource* lights, size_t lightCount,
    std::vector<std::byte>& outRgba, int& outW, int& outH);

} // namespace Solstice::EditorEnginePreview
