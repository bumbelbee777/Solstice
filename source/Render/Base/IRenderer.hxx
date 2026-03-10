#pragma once

#include <Solstice.hxx>
#include <Render/Scene/Scene.hxx>
#include <Render/Scene/Camera.hxx>
#include <Math/Vector.hxx>
#include <vector>
#include <memory>

namespace Solstice::Render {

// Forward declarations
struct SDL_Window;
namespace Physics { struct LightSource; }

/**
 * IRenderer - Abstract renderer interface for platform-specific implementations
 * Supports desktop, VR, and web renderers through a common interface
 */
class SOLSTICE_API IRenderer {
public:
    virtual ~IRenderer() = default;

    // Initialization
    virtual bool Initialize(int width, int height, SDL_Window* window = nullptr) = 0;
    virtual void Shutdown() = 0;
    virtual bool IsInitialized() const = 0;

    // Rendering
    virtual void RenderScene(Scene& scene, const Camera& camera) = 0;
    virtual void RenderScene(Scene& scene, const Camera& camera,
                           const std::vector<Physics::LightSource>& lights) = 0;

    // VR stereo rendering
    virtual void RenderSceneVR(Scene& scene, const Camera& camera, bool leftEye) = 0;

    // Multi-viewport support (for VR, split-screen, etc.)
    virtual void SetViewportCount(uint32_t count) = 0;
    virtual uint32_t GetViewportCount() const = 0;
    virtual void SetViewport(uint32_t index, uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
    virtual void GetViewport(uint32_t index, uint32_t& x, uint32_t& y, uint32_t& width, uint32_t& height) const = 0;

    // Buffer management
    virtual void Clear(const Math::Vec4& color) = 0;
    virtual void Present() = 0;

    // Configuration
    virtual void Resize(int width, int height) = 0;
    virtual void SetVSync(bool enable) = 0;
    virtual void SetWireframe(bool enable) = 0;
    virtual void SetShowDebugOverlay(bool enable) = 0;

    // Stats
    struct RenderStats {
        uint32_t VisibleObjects = 0;
        uint32_t TrianglesSubmitted = 0;
        uint32_t TrianglesCulled = 0;
        uint32_t TrianglesRendered = 0;
        float CullTimeMs = 0.0f;
        float TransformTimeMs = 0.0f;
        float RasterTimeMs = 0.0f;
        float TotalTimeMs = 0.0f;
    };
    virtual const RenderStats& GetStats() const = 0;

    // VR support
    virtual bool IsVRSupported() const = 0;
    virtual bool IsVREnabled() const = 0;
    virtual void SetVREnabled(bool enable) = 0;
};

} // namespace Solstice::Render
