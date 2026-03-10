#include <Render/Context/RenderPipeline.hxx>
#include <Render/PhysicsBridge.hxx>
#include <Core/Debug.hxx>
#include <chrono>

namespace Solstice::Render {
namespace Math = Solstice::Math;

RenderPipeline::RenderPipeline(uint32_t width, uint32_t height)
    : m_Width(width)
    , m_Height(height)
    , m_FrameAllocator(16 * 1024 * 1024) // 16MB per-frame allocator
{
    // Initialize post-processing
    m_PostProcessing = std::make_unique<PostProcessing>();
    m_PostProcessing->Initialize(width, height);

    // Initialize raytracing
    m_Raytracing = std::make_unique<Raytracing>();
    Math::Vec3 worldMin(-100.0f, -100.0f, -100.0f);
    Math::Vec3 worldMax(100.0f, 100.0f, 100.0f);
    m_Raytracing->Initialize(width, height, worldMin, worldMax);
}

RenderPipeline::~RenderPipeline() = default;

void RenderPipeline::BeginFrame() {
    // Reset per-frame allocator
    m_FrameAllocator.Reset();

    // Reset stats
    m_Stats = FrameStats{};
}

void RenderPipeline::EndFrame() {
    // Frame is complete, stats are already updated
}

void RenderPipeline::RenderFrame(Scene& scene, const Camera& camera, const std::vector<Physics::LightSource>& lights) {
    auto t0 = std::chrono::high_resolution_clock::now();

    BeginFrame();

    // Sync physics to scene
    if (m_PhysicsRegistry) {
        SyncPhysicsToScene(scene);
    }

    // Update raytracing (async)
    UpdateRaytracing(scene, lights);

    // Rendering passes (executed in view ID order: 1, 2, 3, 4)
    ShadowPass(scene, camera);
    ScenePass(scene, camera);
    PostProcessPass();
    // UI pass would go here if needed

    EndFrame();

    auto t1 = std::chrono::high_resolution_clock::now();
    m_Stats.TotalTimeMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
}

void RenderPipeline::ShadowPass(Scene& scene, const Camera& camera) {
    // Begin shadow pass - sets up shadow map framebuffer and view
    // The actual shadow rendering is performed by ShadowRenderer in SoftwareRenderer
    // This pass coordinates the pipeline stage by setting up PostProcessing state
    m_PostProcessing->BeginShadowPass();

    // Note: Actual shadow map rendering happens in SoftwareRenderer::RenderScene()
    // via ShadowRenderer::RenderShadowMap(), which is called after this pass
    // coordinates the pipeline stage
}

void RenderPipeline::ScenePass(Scene& scene, const Camera& camera) {
    // Begin scene pass - sets up scene framebuffer and view
    // The actual scene rendering is performed by SceneRenderer in SoftwareRenderer
    // This pass coordinates the pipeline stage by setting up PostProcessing state
    m_PostProcessing->BeginScenePass();

    // Note: Actual scene rendering happens in SoftwareRenderer::RenderScene()
    // via SceneRenderer::RenderScene(), which is called after this pass
    // coordinates the pipeline stage
}

void RenderPipeline::RaytracingPass(Scene& scene, const std::vector<Physics::LightSource>& lights) {
    // Raytracing is already async and handled in UpdateRaytracing
    // This pass is for any synchronous raytracing work if needed
}

void RenderPipeline::PostProcessPass() {
    // Apply post-processing
    if (m_Raytracing && bgfx::isValid(m_Raytracing->GetShadowTexture())) {
        m_PostProcessing->SetRaytracingTextures(
            m_Raytracing->GetShadowTexture(),
            m_Raytracing->GetAOTexture()
        );
    }

    m_PostProcessing->EndScenePass();
    m_PostProcessing->Apply(ViewID::POST);
}

void RenderPipeline::UIPass() {
    // UI rendering would go here
    // This is handled separately in SoftwareRenderer for now
}

void RenderPipeline::Resize(uint32_t width, uint32_t height) {
    m_Width = width;
    m_Height = height;

    if (m_PostProcessing) {
        m_PostProcessing->Resize(width, height);
    }
}

void RenderPipeline::SyncPhysicsToScene(Scene& scene) {
    if (m_PhysicsRegistry) {
        Render::SyncPhysicsToScene(*m_PhysicsRegistry, scene);
    }
}

void RenderPipeline::UpdateRaytracing(Scene& scene, const std::vector<Physics::LightSource>& lights) {
    if (!m_Raytracing) return;

    // Build/update voxel grid if needed (can be done less frequently)
    static int frameCount = 0;
    if (frameCount % 60 == 0) { // Rebuild every 60 frames
        m_Raytracing->BuildVoxelGrid(scene);
    }
    frameCount++;

    // Use provided lights, or fallback to default sun light
    std::vector<Physics::LightSource> lightsToUse = lights;
    if (lightsToUse.empty()) {
        // Fallback to default sun light
        Physics::LightSource sunLight(
            Math::Vec3(0.5f, 1.0f, -0.5f).Normalized(), // Direction
            Math::Vec3(1.0f, 0.95f, 0.9f), // Color
            1.5f, // Intensity
            0.0f, // Hue
            0.001f // Attenuation (very low for sun)
        );
        lightsToUse.push_back(sunLight);
    }

    // Use async raytracing (non-blocking)
    m_Raytracing->UpdateAsync(lightsToUse, scene);
    m_Raytracing->UpdateAsync(); // Clean up completed jobs
    m_Raytracing->UpdateUniforms();
}

} // namespace Solstice::Render
