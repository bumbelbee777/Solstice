#pragma once

#include "../Solstice.hxx"
#include <Render/Scene.hxx>
#include <Render/Camera.hxx>
#include <Render/PostProcessing.hxx>
#include <Render/Raytracing.hxx>
#include <Core/Allocator.hxx>
#include <Core/Async.hxx>
#include <Math/Matrix.hxx>
#include <Math/Vector.hxx>
#include <memory>
#include <vector>
#include <bgfx/bgfx.h>

// Forward declarations
namespace Solstice { namespace ECS { class Registry; } }
namespace Solstice::Render { class Skybox; }
namespace Solstice::Physics { struct LightSource; }

namespace Solstice::Render {

// View IDs - BGFX executes views in ascending order
namespace ViewID {
    static constexpr bgfx::ViewId RESERVED = 0;  // Reserved/Unused
    static constexpr bgfx::ViewId SHADOW = 1;    // Shadow Pass
    static constexpr bgfx::ViewId SCENE = 2;      // Scene Pass
    static constexpr bgfx::ViewId UI = 3;         // UI Overlay
    static constexpr bgfx::ViewId POST = 4;       // Post-Processing (final output)
}

class SOLSTICE_API RenderPipeline {
public:
    RenderPipeline(uint32_t width, uint32_t height);
    ~RenderPipeline();

    // Delete copy constructor and assignment operator
    RenderPipeline(const RenderPipeline&) = delete;
    RenderPipeline& operator=(const RenderPipeline&) = delete;

    // Main rendering entry point
    void RenderFrame(Scene& scene, const Camera& camera, const std::vector<Physics::LightSource>& lights);

    // Frame lifecycle
    void BeginFrame();
    void EndFrame();

    // Rendering passes
    void ShadowPass(Scene& scene, const Camera& camera);
    void ScenePass(Scene& scene, const Camera& camera);
    void RaytracingPass(Scene& scene, const std::vector<Physics::LightSource>& lights);
    void PostProcessPass();
    void UIPass();

    // Configuration
    void Resize(uint32_t width, uint32_t height);
    void SetPhysicsRegistry(ECS::Registry* registry) { m_PhysicsRegistry = registry; }
    void SetSkybox(Skybox* skybox) { m_Skybox = skybox; }

    // Access to subsystems
    PostProcessing& GetPostProcessing() { return *m_PostProcessing; }
    const PostProcessing& GetPostProcessing() const { return *m_PostProcessing; }
    Raytracing* GetRaytracing() { return m_Raytracing.get(); }
    const Raytracing* GetRaytracing() const { return m_Raytracing.get(); }

    // Stats
    struct FrameStats {
        uint32_t VisibleObjects = 0;
        uint32_t TrianglesSubmitted = 0;
        float TotalTimeMs = 0.0f;
    };
    const FrameStats& GetStats() const { return m_Stats; }

private:
    // Frame state
    uint32_t m_Width;
    uint32_t m_Height;
    FrameStats m_Stats;

    // Subsystems
    std::unique_ptr<PostProcessing> m_PostProcessing;
    std::unique_ptr<Raytracing> m_Raytracing;

    // External references (not owned)
    ECS::Registry* m_PhysicsRegistry{nullptr};
    Skybox* m_Skybox{nullptr};

    // Per-frame allocator
    Core::ArenaAllocator m_FrameAllocator;

    // Helper functions
    void SyncPhysicsToScene(Scene& scene);
    void UpdateRaytracing(Scene& scene, const std::vector<Physics::LightSource>& lights);
};

} // namespace Solstice::Render
