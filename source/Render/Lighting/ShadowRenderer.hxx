#pragma once

#include <Solstice.hxx>
#include <Render/Scene/Scene.hxx>
#include <Render/Scene/Camera.hxx>
#include <Render/Post/PostProcessing.hxx>
#include <bgfx/bgfx.h>
#include <Math/Matrix.hxx>
#include <vector>

namespace Solstice::Render {

// Forward declarations
class PostProcessing;

// Handles shadow map generation
class SOLSTICE_API ShadowRenderer {
public:
    ShadowRenderer();
    ~ShadowRenderer();

    // Initialize with required resources
    void Initialize(bgfx::ProgramHandle shadowProgram, bgfx::VertexLayout vertexLayout,
                    PostProcessing* postProcessing, uint32_t shadowMapSize = 1024);

    // Render shadow map - handles full shadow pass
    void RenderShadowMap(Scene& scene, const Camera& camera, MeshLibrary* meshLib,
                         bool optimizeStaticBuffers, uint32_t& visibleObjectsCount);

    // Get shadow view-projection matrix (from PostProcessing)
    const Math::Matrix4& GetShadowViewProj() const;

    // Configuration
    void SetOptimizeStaticBuffers(bool enable) { m_OptimizeStaticBuffers = enable; }

private:
    bgfx::ProgramHandle m_ShadowProgram = BGFX_INVALID_HANDLE;
    bgfx::VertexLayout m_VertexLayout;
    PostProcessing* m_PostProcessing = nullptr;
    uint32_t m_ShadowMapSize = 1024;
    bool m_OptimizeStaticBuffers = true;

    // Helper: Get all objects for shadow pass (including ground)
    void GetAllObjectsForShadow(Scene& scene, const Camera& camera, std::vector<SceneObjectID>& objects);
};

} // namespace Solstice::Render
