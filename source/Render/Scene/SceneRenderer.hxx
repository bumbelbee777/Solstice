#pragma once

#include <Solstice.hxx>
#include <Render/Scene/Scene.hxx>
#include <Render/Scene/Camera.hxx>
#include <Render/Assets/Mesh.hxx>
#include <Core/Material.hxx>
#include <Render/Post/PostProcessing.hxx>
#include <Render/Scene/Skybox.hxx>
#include <Render/Assets/TextureRegistry.hxx>
#include <Physics/LightSource.hxx>
#include <bgfx/bgfx.h>
#include <vector>
#include <set>
#include <cstdint>

namespace Solstice::Render {

// Forward declarations
class PostProcessing;

// Handles scene object rendering (culling, submission)
class SOLSTICE_API SceneRenderer {
public:
    SceneRenderer();
    ~SceneRenderer();

    // Initialize with required resources
    void Initialize(bgfx::ProgramHandle sceneProgram, bgfx::VertexLayout vertexLayout,
                   PostProcessing* postProcessing, TextureRegistry* textureRegistry,
                   Skybox* skybox, bgfx::ProgramHandle skyboxProgram,
                   uint32_t width, uint32_t height);

    // Render scene objects to the specified view
    void RenderScene(Scene& scene, const Camera& camera,
                    MeshLibrary* meshLib, Core::MaterialLibrary* materialLib,
                    uint32_t& trianglesSubmitted);

    // Culling
    void CullObjects(Scene& scene, const Camera& camera, std::vector<SceneObjectID>& visibleObjects);

    // Set light sources for rendering
    void SetLightSources(const std::vector<Physics::LightSource>& lights);

    // Configuration
    void SetOptimizeStaticBuffers(bool enable) { m_OptimizeStaticBuffers = enable; }
    void SetWireframe(bool enable) { m_WireframeEnabled = enable; }
    void SetShowDebugOverlay(bool enable) { m_ShowDebugOverlay = enable; }

    // Object selection and hover state
    void SetSelectedObjects(const std::set<SceneObjectID>& objects) { m_SelectedObjects = objects; }
    void SetHoveredObject(SceneObjectID objectID) { m_HoveredObject = objectID; }

    // Get scene program for world-space UI rendering
    bgfx::ProgramHandle GetSceneProgram() const { return m_SceneProgram; }

private:
    bgfx::ProgramHandle m_SceneProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_SkyboxProgram = BGFX_INVALID_HANDLE;
    bgfx::VertexLayout m_VertexLayout;
    PostProcessing* m_PostProcessing = nullptr;
    TextureRegistry* m_TextureRegistry = nullptr;
    Skybox* m_Skybox = nullptr;
    uint32_t m_Width = 0;
    uint32_t m_Height = 0;
    bool m_OptimizeStaticBuffers = true;
    bool m_WireframeEnabled = false;
    bool m_ShowDebugOverlay = false;
    std::vector<Physics::LightSource> m_Lights;

    // Selection and hover state
    std::set<SceneObjectID> m_SelectedObjects;
    SceneObjectID m_HoveredObject{InvalidObjectID};

    // Velocity tracking for motion blur
    Math::Matrix4 m_PreviousViewProj;
    Math::Vec3 m_PreviousCameraPos;
    bool m_HasPreviousFrame = false;

    // Helper: Setup buffers for an object
    bool SetupBuffers(Mesh* meshPtr, bool optimizeStatic);

    // Helper: Render outline for an object (two-pass technique)
    void RenderObjectOutline(SceneObjectID objID, Scene& scene, MeshLibrary* meshLib, const Camera& camera, const Math::Vec4& outlineColor);
};

} // namespace Solstice::Render
