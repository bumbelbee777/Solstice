#include <Render/Lighting/ShadowRenderer.hxx>
#include <Render/Post/PostProcessing.hxx>
#include <Render/Assets/Mesh.hxx>
#include <Core/Debug.hxx>
#include <algorithm>
#include <cstring>

namespace Solstice::Render {
namespace Math = Solstice::Math;

ShadowRenderer::ShadowRenderer() = default;
ShadowRenderer::~ShadowRenderer() = default;

void ShadowRenderer::Initialize(bgfx::ProgramHandle shadowProgram, bgfx::VertexLayout vertexLayout,
                                PostProcessing* postProcessing, uint32_t shadowMapSize) {
    m_ShadowProgram = shadowProgram;
    m_VertexLayout = vertexLayout;
    m_PostProcessing = postProcessing;
    m_ShadowMapSize = shadowMapSize;
}

void ShadowRenderer::GetAllObjectsForShadow(Scene& scene, const Camera& camera, std::vector<SceneObjectID>& objects) {
    // Start with frustum-culled objects
    scene.FrustumCull(camera, objects);

    // Also include ALL objects for shadow pass to ensure ground is always included
    // The ground plane should always be in the shadow map
    size_t objectCount = scene.GetObjectCount();
    for (size_t i = 0; i < objectCount; ++i) {
        SceneObjectID objID = static_cast<SceneObjectID>(i);
        // Check if already in list
        bool found = false;
        for (SceneObjectID existingID : objects) {
            if (existingID == objID) {
                found = true;
                break;
            }
        }
        if (!found) {
            objects.push_back(objID);
        }
    }
}

const Math::Matrix4& ShadowRenderer::GetShadowViewProj() const {
    if (m_PostProcessing) {
        return m_PostProcessing->GetShadowViewProj();
    }
    static Math::Matrix4 identity;
    return identity;
}

void ShadowRenderer::RenderShadowMap(Scene& scene, const Camera& camera, MeshLibrary* meshLib,
                                    bool optimizeStaticBuffers, uint32_t& visibleObjectsCount) {
    if (!meshLib || !bgfx::isValid(m_ShadowProgram) || !m_PostProcessing) {
        return;
    }

    // Update camera position for shadow following BEFORE BeginShadowPass
    // This ensures the shadow matrix is calculated with the current camera position
    Math::Vec3 camPos = camera.GetPosition();
    m_PostProcessing->SetCameraPosition(camPos);
    m_PostProcessing->BeginShadowPass(); // This recalculates shadow matrix with camera position

    // Get all objects for shadow pass (including ground)
    std::vector<SceneObjectID> visibleObjects;
    GetAllObjectsForShadow(scene, camera, visibleObjects);
    visibleObjectsCount = static_cast<uint32_t>(visibleObjects.size());

    // Render each object to shadow map
    for (SceneObjectID ObjID : visibleObjects) {
        uint32_t MeshID = scene.GetMeshID(ObjID);
        Mesh* MeshPtr = meshLib->GetMesh(MeshID);
        if (!MeshPtr || MeshPtr->Vertices.empty()) continue;

        const Math::Matrix4& WorldMat = scene.GetWorldMatrix(ObjID);
        float model[16];
        // Convert row-major World to column-major for BGFX u_model
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c)
                model[c*4 + r] = WorldMat.M[r][c];
        bgfx::setTransform(model);

        // Setup Buffers (Static or Transient)
        bool buffersSet = false;
        bgfx::VertexBufferHandle vbh = { MeshPtr->VertexBufferHandle.Handle };
        bgfx::IndexBufferHandle ibh = { MeshPtr->IndexBufferHandle.Handle };

        if (m_OptimizeStaticBuffers && optimizeStaticBuffers && MeshPtr->IsStatic && bgfx::isValid(vbh)) {
            bgfx::setVertexBuffer(0, vbh);
            bgfx::setIndexBuffer(ibh);
            buffersSet = true;
        }

        if (!buffersSet) {
            // Transient fallback for non-static meshes
            uint32_t numVerts = static_cast<uint32_t>(MeshPtr->Vertices.size());
            uint32_t numIndices = static_cast<uint32_t>(MeshPtr->Indices.size());

            if (bgfx::getAvailTransientVertexBuffer(numVerts, m_VertexLayout) >= numVerts &&
                bgfx::getAvailTransientIndexBuffer(numIndices, true) >= numIndices) {

                bgfx::TransientVertexBuffer tvb;
                bgfx::allocTransientVertexBuffer(&tvb, numVerts, m_VertexLayout);
                if (tvb.data != nullptr) {
                    std::memcpy(tvb.data, MeshPtr->Vertices.data(), numVerts * sizeof(QuantizedVertex));
                }

                bgfx::TransientIndexBuffer tib;
                bgfx::allocTransientIndexBuffer(&tib, numIndices, true);
                if (tib.data != nullptr) {
                    std::memcpy(tib.data, MeshPtr->Indices.data(), numIndices * sizeof(uint32_t));
                }

                bgfx::setVertexBuffer(0, &tvb);
                bgfx::setIndexBuffer(&tib);
                buffersSet = true;
            }
        }

        if (!buffersSet) {
            continue; // Skip if no buffer space available
        }

        // Shadow pass render state
        uint64_t state = BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_CULL_CCW;
        bgfx::setState(state);
        bgfx::submit(PostProcessing::VIEW_SHADOW, m_ShadowProgram);
    }
}

} // namespace Solstice::Render
