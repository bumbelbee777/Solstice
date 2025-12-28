#pragma once

#include "../Solstice.hxx"
#include <Render/Scene.hxx>
#include <Render/Camera.hxx>
#include <Render/Mesh.hxx>
#include <Core/Material.hxx>
#include <Render/PostProcessing.hxx>
#include <Render/Skybox.hxx>
#include <Render/TextureRegistry.hxx>
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
    void SetOptimizeStaticBuffers(bool enable) { m_optimizeStaticBuffers = enable; }
    void SetWireframe(bool enable) { m_wireframeEnabled = enable; }
    void SetShowDebugOverlay(bool enable) { m_showDebugOverlay = enable; }
    
    // Object selection and hover state
    void SetSelectedObjects(const std::set<SceneObjectID>& objects) { m_selectedObjects = objects; }
    void SetHoveredObject(SceneObjectID objectID) { m_hoveredObject = objectID; }

private:
    bgfx::ProgramHandle m_sceneProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_skyboxProgram = BGFX_INVALID_HANDLE;
    bgfx::VertexLayout m_vertexLayout;
    PostProcessing* m_postProcessing = nullptr;
    TextureRegistry* m_textureRegistry = nullptr;
    Skybox* m_skybox = nullptr;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    bool m_optimizeStaticBuffers = true;
    bool m_wireframeEnabled = false;
    bool m_showDebugOverlay = false;
    std::vector<Physics::LightSource> m_lights;
    
    // Selection and hover state
    std::set<SceneObjectID> m_selectedObjects;
    SceneObjectID m_hoveredObject{InvalidObjectID};

    // Velocity tracking for motion blur
    Math::Matrix4 m_previousViewProj;
    Math::Vec3 m_previousCameraPos;
    bool m_hasPreviousFrame = false;

    // Helper: Setup buffers for an object
    bool SetupBuffers(Mesh* meshPtr, bool optimizeStatic);
    
    // Helper: Render outline for an object (two-pass technique)
    void RenderObjectOutline(SceneObjectID objID, Scene& scene, MeshLibrary* meshLib, const Camera& camera, const Math::Vec4& outlineColor);
};

} // namespace Solstice::Render
