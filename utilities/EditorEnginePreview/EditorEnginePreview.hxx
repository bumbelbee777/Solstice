#pragma once

#include <cstddef>
#include <vector>

#include <Math/Vector.hxx>
#include <Physics/Lighting/LightSource.hxx>

namespace LibUI::Viewport {
struct OrbitPanZoomState;
}

namespace Solstice::EditorEnginePreview {

/// World-space cube instance for crude editor previews.
struct PreviewEntity {
    Math::Vec3 Position{0.f, 0.f, 0.f};
    Math::Vec3 Albedo{0.65f, 0.68f, 0.8f};
    float HalfExtent{0.35f};
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
