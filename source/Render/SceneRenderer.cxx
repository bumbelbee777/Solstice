#include <Render/SceneRenderer.hxx>
#include <Render/PostProcessing.hxx>
#include <Render/Skybox.hxx>
#include <Render/TextureRegistry.hxx>
#include <Render/Mesh.hxx>
#include <Core/Material.hxx>
#include <Core/Debug.hxx>
#include <Core/ScopeTimer.hxx>
#include <cstring>

// File-scope uniforms (shared between RenderScene and RenderObjectOutline)
// File-scope uniforms (shared between RenderScene and RenderObjectOutline)
static bgfx::UniformHandle u_AlbedoColor = BGFX_INVALID_HANDLE;
static bgfx::UniformHandle u_MaterialParams = BGFX_INVALID_HANDLE;
static bgfx::UniformHandle u_Emission = BGFX_INVALID_HANDLE;
static bgfx::UniformHandle u_TextureBlend = BGFX_INVALID_HANDLE;
static bgfx::UniformHandle s_TexAlbedo = BGFX_INVALID_HANDLE;
static bgfx::UniformHandle s_TexAlbedo2 = BGFX_INVALID_HANDLE;
static bgfx::UniformHandle s_TexBlendMask = BGFX_INVALID_HANDLE;
static bgfx::UniformHandle s_TexRoughness = BGFX_INVALID_HANDLE;
static bgfx::UniformHandle s_TexMetallic = BGFX_INVALID_HANDLE;

// Multi-Light Uniforms
static bgfx::UniformHandle u_PointLightPos = BGFX_INVALID_HANDLE;
static bgfx::UniformHandle u_PointLightColor = BGFX_INVALID_HANDLE;
static bgfx::UniformHandle u_PointLightParams = BGFX_INVALID_HANDLE;
static bgfx::UniformHandle u_NumPointLights = BGFX_INVALID_HANDLE;

namespace Solstice::Render {
namespace Math = Solstice::Math;

SceneRenderer::SceneRenderer() = default;
SceneRenderer::~SceneRenderer() = default;

void SceneRenderer::Initialize(bgfx::ProgramHandle sceneProgram, bgfx::VertexLayout vertexLayout,
                               PostProcessing* postProcessing, TextureRegistry* textureRegistry,
                               Skybox* skybox, bgfx::ProgramHandle skyboxProgram,
                               uint32_t width, uint32_t height) {
    if (!postProcessing) {
        SIMPLE_LOG("ERROR: SceneRenderer::Initialize called with null PostProcessing");
        return;
    }
    m_sceneProgram = sceneProgram;
    m_vertexLayout = vertexLayout;
    m_postProcessing = postProcessing;
    m_textureRegistry = textureRegistry;
    m_skybox = skybox; // Can be null, will be set later via SetSkybox
    m_skyboxProgram = skyboxProgram; // Can be invalid, will be checked before use
    m_width = width;
    m_height = height;
}

void SceneRenderer::CullObjects(Scene& scene, const Camera& camera, std::vector<SceneObjectID>& visibleObjects) {
    // Phase 7: Pass aspect ratio for proper frustum culling
    float aspectRatio = static_cast<float>(m_width) / static_cast<float>(m_height);
    scene.FrustumCull(camera, visibleObjects, aspectRatio);
}

void SceneRenderer::SetLightSources(const std::vector<Physics::LightSource>& lights) {
    m_lights = lights;
}

bool SceneRenderer::SetupBuffers(Mesh* meshPtr, bool optimizeStatic) {
    if (!meshPtr) return false;

    bgfx::VertexBufferHandle vbh = { meshPtr->VertexBufferHandle.Handle };
    bgfx::IndexBufferHandle ibh = { meshPtr->IndexBufferHandle.Handle };

    if (m_optimizeStaticBuffers && optimizeStatic && meshPtr->IsStatic) {
        // If handles are invalid, create them
        if (meshPtr->VertexBufferHandle.Handle == 0xFFFF) {
            const bgfx::Memory* mem = bgfx::copy(meshPtr->Vertices.data(),
                static_cast<uint32_t>(meshPtr->Vertices.size() * sizeof(QuantizedVertex)));
            vbh = bgfx::createVertexBuffer(mem, m_vertexLayout);
            meshPtr->VertexBufferHandle.Handle = vbh.idx;
        }

        if (meshPtr->IndexBufferHandle.Handle == 0xFFFF) {
            const bgfx::Memory* mem = bgfx::copy(meshPtr->Indices.data(),
                static_cast<uint32_t>(meshPtr->Indices.size() * sizeof(uint32_t)));
            ibh = bgfx::createIndexBuffer(mem, BGFX_BUFFER_INDEX32);
            meshPtr->IndexBufferHandle.Handle = ibh.idx;
        }

        // Set static buffers
        vbh = { meshPtr->VertexBufferHandle.Handle };
        ibh = { meshPtr->IndexBufferHandle.Handle };

        if (bgfx::isValid(vbh) && bgfx::isValid(ibh)) {
            bgfx::setVertexBuffer(0, vbh);
            bgfx::setIndexBuffer(ibh);
            return true;
        }
    }

    // Fallback to transient buffers
    uint32_t numVerts = static_cast<uint32_t>(meshPtr->Vertices.size());
    uint32_t numIndices = static_cast<uint32_t>(meshPtr->Indices.size());

    if (bgfx::getAvailTransientVertexBuffer(numVerts, m_vertexLayout) >= numVerts &&
        bgfx::getAvailTransientIndexBuffer(numIndices, true) >= numIndices) {

        bgfx::TransientVertexBuffer tvb;
        bgfx::allocTransientVertexBuffer(&tvb, numVerts, m_vertexLayout);
        if (tvb.data != nullptr) {
            std::memcpy(tvb.data, meshPtr->Vertices.data(), numVerts * sizeof(QuantizedVertex));
        }

        bgfx::TransientIndexBuffer tib;
        bgfx::allocTransientIndexBuffer(&tib, numIndices, true);
        if (tib.data != nullptr) {
            std::memcpy(tib.data, meshPtr->Indices.data(), numIndices * sizeof(uint32_t));
        }

        bgfx::setVertexBuffer(0, &tvb);
        bgfx::setIndexBuffer(&tib);
        return true;
    }

    return false;
}

void SceneRenderer::RenderScene(Scene& scene, const Camera& camera,
                                MeshLibrary* meshLib, Core::MaterialLibrary* materialLib,
                                uint32_t& trianglesSubmitted) {
    PROFILE_SCOPE("SceneRenderer::RenderScene");

    if (!meshLib || !bgfx::isValid(m_sceneProgram) || !m_postProcessing) {
        if (!meshLib) SIMPLE_LOG("SceneRenderer::RenderScene: meshLib is null");
        if (!bgfx::isValid(m_sceneProgram)) SIMPLE_LOG("SceneRenderer::RenderScene: sceneProgram is invalid");
        if (!m_postProcessing) SIMPLE_LOG("SceneRenderer::RenderScene: postProcessing is null");
        return;
    }

    // Get camera position and sync with PostProcessing before scene pass
    // This ensures shadow matrix calculations use the correct camera position
    Math::Vec3 camPos = camera.GetPosition();
    m_postProcessing->SetCameraPosition(camPos);

    // Begin scene pass
    m_postProcessing->BeginScenePass();

    // Update debug flags
    uint32_t debugFlags = m_showDebugOverlay ? BGFX_DEBUG_TEXT : 0;
    if (m_wireframeEnabled) debugFlags |= BGFX_DEBUG_WIREFRAME;
    bgfx::setDebug(debugFlags);

    // Camera Matrices
    Math::Matrix4 View = camera.GetViewMatrix();
    auto isApproxIdentity = [](const Math::Matrix4& M) {
        const float eps = 1e-6f;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                if (std::fabs(M.M[i][j] - ((i==j)?1.0f:0.0f)) > eps) return false;
        return true;
    };
    if (isApproxIdentity(View)) {
        View = Math::Matrix4::LookAt(Math::Vec3(0.0f, 0.0f, 2.0f), Math::Vec3(0.0f, 0.0f, 0.0f), Math::Vec3(0.0f, 1.0f, 0.0f));
    }
    // Phase 8: Extended far plane to 2000.0f for distant landmark visibility
    float aspectRatio = static_cast<float>(m_width) / static_cast<float>(m_height);
    Math::Matrix4 Proj = Math::Matrix4::Perspective(camera.GetZoom() * 0.0174533f,
                                                    aspectRatio,
                                                    0.1f, 2000.0f);

    // Set View Transform for scene (skybox will override this temporarily)
    Math::Matrix4 ViewT = View.Transposed();
    Math::Matrix4 ProjT = Proj.Transposed();
    bgfx::setViewTransform(PostProcessing::VIEW_SCENE, &ViewT.M[0][0], &ProjT.M[0][0]);

    // Render skybox first (before scene objects)
    if (m_skybox && m_skybox->IsInitialized() && bgfx::isValid(m_skyboxProgram) && bgfx::isValid(m_skybox->GetCubemap())) {
        bgfx::setViewFrameBuffer(PostProcessing::VIEW_SCENE, m_postProcessing->GetSceneFramebuffer());
        m_skybox->Render(camera, m_skyboxProgram, m_width, m_height, PostProcessing::VIEW_SCENE);
        // Restore view setup for scene objects
        bgfx::setViewRect(PostProcessing::VIEW_SCENE, 0, 0, static_cast<uint16_t>(m_width), static_cast<uint16_t>(m_height));
        bgfx::setViewFrameBuffer(PostProcessing::VIEW_SCENE, m_postProcessing->GetSceneFramebuffer());
        bgfx::setViewClear(PostProcessing::VIEW_SCENE, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x101010FF, 1.0f, 0);
        bgfx::setViewTransform(PostProcessing::VIEW_SCENE, &ViewT.M[0][0], &ProjT.M[0][0]);
    }

    // Uniforms - create once, set every frame
    static bgfx::UniformHandle u_shadowMtx = bgfx::createUniform("u_shadowMtx", bgfx::UniformType::Mat4);
    static bgfx::UniformHandle s_TexShadow = bgfx::createUniform("s_TexShadow", bgfx::UniformType::Sampler);

    // Initialize file-scope uniforms if not already done
    if (!bgfx::isValid(u_AlbedoColor)) {
        u_AlbedoColor = bgfx::createUniform("u_AlbedoColor", bgfx::UniformType::Vec4);
        u_MaterialParams = bgfx::createUniform("u_MaterialParams", bgfx::UniformType::Vec4);
        u_Emission = bgfx::createUniform("u_Emission", bgfx::UniformType::Vec4);
        u_TextureBlend = bgfx::createUniform("u_TextureBlend", bgfx::UniformType::Vec4);
        s_TexAlbedo = bgfx::createUniform("s_TexAlbedo", bgfx::UniformType::Sampler);
        s_TexAlbedo2 = bgfx::createUniform("s_TexAlbedo2", bgfx::UniformType::Sampler);
        s_TexBlendMask = bgfx::createUniform("s_TexBlendMask", bgfx::UniformType::Sampler);
        s_TexRoughness = bgfx::createUniform("s_TexRoughness", bgfx::UniformType::Sampler);
        s_TexMetallic = bgfx::createUniform("s_TexMetallic", bgfx::UniformType::Sampler);

        // Point lights support (max 32)
        u_PointLightPos = bgfx::createUniform("u_PointLightPos", bgfx::UniformType::Vec4, 32);
        u_PointLightColor = bgfx::createUniform("u_PointLightColor", bgfx::UniformType::Vec4, 32);
        u_PointLightParams = bgfx::createUniform("u_PointLightParams", bgfx::UniformType::Vec4, 32);
        u_NumPointLights = bgfx::createUniform("u_NumPointLights", bgfx::UniformType::Vec4);
    }
    static bgfx::UniformHandle u_LightDir = bgfx::createUniform("u_LightDir", bgfx::UniformType::Vec4);
    static bgfx::UniformHandle u_LightColor = bgfx::createUniform("u_LightColor", bgfx::UniformType::Vec4);
    static bgfx::UniformHandle u_CameraPos = bgfx::createUniform("u_CameraPos", bgfx::UniformType::Vec4);
    static bgfx::UniformHandle s_TexEnvironment = bgfx::createUniform("s_TexEnvironment", bgfx::UniformType::Sampler);
    static bgfx::UniformHandle u_NormalMapParams = bgfx::createUniform("u_NormalMapParams", bgfx::UniformType::Vec4);
    static bgfx::UniformHandle s_TexNormal = bgfx::createUniform("s_TexNormal", bgfx::UniformType::Sampler);
    static bgfx::UniformHandle u_LightmapParams = bgfx::createUniform("u_LightmapParams", bgfx::UniformType::Vec4);
    static bgfx::UniformHandle s_TexLightmap = bgfx::createUniform("s_TexLightmap", bgfx::UniformType::Sampler);

    // Get primary light source (first directional light, or fallback)
    Math::Vec3 lightDir = Math::Vec3(0.5f, 1.0f, -0.5f).Normalized();
    Math::Vec3 lightColor = Math::Vec3(1.0f, 0.95f, 0.9f);
    float lightIntensity = 1.5f;

    // Point Light accumulation buffers
    static float pltPos[32 * 4];
    static float pltCol[32 * 4];
    static float pltPrm[32 * 4];
    int pltCount = 0;

    // Process lights: First directional becomes sun, others become point lights
    for (const auto& light : m_lights) {
        if (light.Type == Physics::LightSource::LightType::Directional) {
            // First directional is sun
            lightDir = light.Position.Normalized();
            lightColor = light.Color;
            lightIntensity = light.Intensity;
        } else if (pltCount < 32) {
            // Add to point lights
            pltPos[pltCount * 4 + 0] = light.Position.x;
            pltPos[pltCount * 4 + 1] = light.Position.y;
            pltPos[pltCount * 4 + 2] = light.Position.z;
            pltPos[pltCount * 4 + 3] = light.Range; // Store range in w

            pltCol[pltCount * 4 + 0] = light.Color.x;
            pltCol[pltCount * 4 + 1] = light.Color.y;
            pltCol[pltCount * 4 + 2] = light.Color.z;
            pltCol[pltCount * 4 + 3] = light.Intensity; // Store intensity in w

            pltPrm[pltCount * 4 + 0] = light.Attenuation;
            pltPrm[pltCount * 4 + 1] = 0.0f; // Padding
            pltPrm[pltCount * 4 + 2] = 0.0f; // Padding
            pltPrm[pltCount * 4 + 3] = 0.0f; // Padding

            pltCount++;
        }
    }

    // Set Sun Light Uniforms
    float lightDirData[4] = { lightDir.x, lightDir.y, lightDir.z, 0.0f };
    float lightColorData[4] = { lightColor.x, lightColor.y, lightColor.z, lightIntensity };
    bgfx::setUniform(u_LightDir, lightDirData);
    bgfx::setUniform(u_LightColor, lightColorData);

    // Set Point Light Uniforms
    if (pltCount > 0) {
        bgfx::setUniform(u_PointLightPos, pltPos, static_cast<uint16_t>(pltCount));
        bgfx::setUniform(u_PointLightColor, pltCol, static_cast<uint16_t>(pltCount));
        bgfx::setUniform(u_PointLightParams, pltPrm, static_cast<uint16_t>(pltCount));
    }
    float numLightsData[4] = { static_cast<float>(pltCount), 0.0f, 0.0f, 0.0f };
    bgfx::setUniform(u_NumPointLights, numLightsData);

    // Set camera position uniform EVERY FRAME - required for correct view direction calculation
    float camPosData[4] = { camPos.x, camPos.y, camPos.z, 0.0f };
    bgfx::setUniform(u_CameraPos, camPosData);

    // Prepare Shadow Matrix - get it AFTER BeginShadowPass() has been called
    // (which happens in ShadowRenderer before this function is called)
    Math::Matrix4 shadowViewProj = m_postProcessing->GetShadowViewProj();
    Math::Matrix4 shadowMatT = shadowViewProj.Transposed();
    bgfx::setUniform(u_shadowMtx, &shadowMatT.M[0][0]);
    bgfx::setTexture(1, s_TexShadow, m_postProcessing->GetShadowMap());

    // Set environment map (skybox cubemap) for reflections
    if (m_skybox && m_skybox->IsInitialized() && bgfx::isValid(m_skybox->GetCubemap())) {
        bgfx::setTexture(3, s_TexEnvironment, m_skybox->GetCubemap());
    } else {
        // Fallback: use a default cubemap or skip reflection
        // For now, we'll just not set it and let the shader handle it
    }

    // Default textures for materials without textures
    static bgfx::TextureHandle defaultWhiteTexture = BGFX_INVALID_HANDLE;
    static bgfx::TextureHandle defaultRoughnessTexture = BGFX_INVALID_HANDLE;
    static bgfx::TextureHandle defaultMetallicTexture = BGFX_INVALID_HANDLE;
    static bgfx::TextureHandle defaultNormalTexture = BGFX_INVALID_HANDLE;
    static bgfx::TextureHandle defaultLightmapTexture = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(defaultWhiteTexture)) {
        uint8_t whitePixel[4] = {255, 255, 255, 255};
        const bgfx::Memory* mem = bgfx::copy(whitePixel, 4);
        defaultWhiteTexture = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_NONE, mem);
    }
    if (!bgfx::isValid(defaultRoughnessTexture)) {
        // Default roughness: mid-gray (0.5 = 128) - indicates medium roughness
        uint8_t roughnessPixel[4] = {255, 255, 255, 255}; // White = use uniform value
        const bgfx::Memory* mem = bgfx::copy(roughnessPixel, 4);
        defaultRoughnessTexture = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_NONE, mem);
    }
    if (!bgfx::isValid(defaultMetallicTexture)) {
        // Default metallic: black (0.0 = 0) - indicates non-metallic
        uint8_t metallicPixel[4] = {0, 0, 0, 255}; // Black = use uniform value
        const bgfx::Memory* mem = bgfx::copy(metallicPixel, 4);
        defaultMetallicTexture = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_NONE, mem);
    }
    if (!bgfx::isValid(defaultNormalTexture)) {
        // Default normal map: flat normal pointing up (0.5, 0.5, 1.0) in [0,1] range
        // Encoded as RGB: (128, 128, 255) = (0.5, 0.5, 1.0)
        uint8_t normalPixel[4] = {128, 128, 255, 255};
        const bgfx::Memory* mem = bgfx::copy(normalPixel, 4);
        defaultNormalTexture = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_NONE, mem);
    }
    if (!bgfx::isValid(defaultLightmapTexture)) {
        // Default lightmap: black (no radiosity)
        uint8_t lightmapPixel[4] = {0, 0, 0, 0};
        const bgfx::Memory* mem = bgfx::copy(lightmapPixel, 4);
        defaultLightmapTexture = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_NONE, mem);
    }

    // Cull objects
    std::vector<SceneObjectID> visibleObjects;
    {
        PROFILE_SCOPE("SceneRenderer::CullObjects");
        CullObjects(scene, camera, visibleObjects);
    }

    // Render visible objects
    trianglesSubmitted = 0;
    {
        PROFILE_SCOPE("SceneRenderer::RenderObjects");
        for (SceneObjectID ObjID : visibleObjects) {
        uint32_t MeshID = scene.GetMeshID(ObjID);
        Mesh* MeshPtr = meshLib->GetMesh(MeshID);
        if (!MeshPtr) {
            SIMPLE_LOG("SceneRenderer: Mesh not found for object " + std::to_string(ObjID));
            continue;
        }
        if (MeshPtr->Vertices.empty()) {
            continue;
        }

        const Math::Matrix4& WorldMat = scene.GetWorldMatrix(ObjID);
        float model[16];
        // Convert row-major World to column-major for BGFX u_model
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c)
                model[c*4 + r] = WorldMat.M[r][c];
        bgfx::setTransform(model);

        // Setup buffers
        if (!SetupBuffers(MeshPtr, m_optimizeStaticBuffers)) {
            continue; // Skip if no buffer space
        }

        // Check if mesh uses static buffers (allows index offsets)
        bool UsesStaticBuffers = (m_optimizeStaticBuffers && MeshPtr->IsStatic &&
                                  MeshPtr->IndexBufferHandle.Handle != 0xFFFF);

        // Render each submesh with its own material
        if (MeshPtr->SubMeshes.empty() || !UsesStaticBuffers) {
            // No submeshes or transient buffers - render entire mesh with first submesh's material
        Math::Vec4 albedoColor(0.7f, 0.7f, 0.7f, 0.5f);
        float metallic = 0.0f;
        bgfx::TextureHandle albedoTexture = BGFX_INVALID_HANDLE;
        bgfx::TextureHandle detailTexture = BGFX_INVALID_HANDLE;
        bgfx::TextureHandle blendMaskTexture = BGFX_INVALID_HANDLE;
        bgfx::TextureHandle roughnessTexture = BGFX_INVALID_HANDLE;
        bgfx::TextureHandle normalTexture = BGFX_INVALID_HANDLE;
        float blendMode = 0.0f;
        float blendFactor = 0.0f;
        bool isTransparent = false;
        bool hasNormalMap = false;
        const Core::Material* currentMat = nullptr;

        if (materialLib && !MeshPtr->SubMeshes.empty()) {
            uint32_t matID = MeshPtr->SubMeshes[0].MaterialID;
            const auto& materials = materialLib->GetMaterials();
                if (matID != UINT32_MAX && matID < materials.size()) {
                const Core::Material& mat = materials[matID];
                currentMat = &mat;
                Math::Vec3 albedo = mat.GetAlbedoColor();
                float roughness = mat.GetRoughness();
                albedoColor = Math::Vec4(albedo.x, albedo.y, albedo.z, mat.Opacity / 255.0f);
                metallic = mat.GetMetallicFactor();
                isTransparent = (mat.Flags & Core::MaterialFlag_Transparent) != 0;

                if (mat.AlbedoTexIndex != 0xFFFF && m_textureRegistry) {
                    albedoTexture = m_textureRegistry->Get(mat.AlbedoTexIndex);
                }
                if (mat.AlbedoTexIndex2 != 0xFFFF && m_textureRegistry) {
                    detailTexture = m_textureRegistry->Get(mat.AlbedoTexIndex2);
                }
                if (mat.AlbedoTexIndex3 != 0xFFFF && m_textureRegistry) {
                    blendMaskTexture = m_textureRegistry->Get(mat.AlbedoTexIndex3);
                }
                if (mat.RoughnessTexIndex != 0xFFFF && m_textureRegistry) {
                    roughnessTexture = m_textureRegistry->Get(mat.RoughnessTexIndex);
                }
                if (mat.NormalMapIndex != 0xFFFF && m_textureRegistry) {
                    normalTexture = m_textureRegistry->Get(mat.NormalMapIndex);
                    hasNormalMap = true;
                }
                blendMode = static_cast<float>(mat.TextureBlendMode);
                blendFactor = mat.GetTextureBlendFactor();

                bgfx::setUniform(u_AlbedoColor, &albedoColor);
                Math::Vec4 materialParams(metallic, roughness, 0.0f, 0.0f);
                bgfx::setUniform(u_MaterialParams, &materialParams);
                Math::Vec3 emissionColor = mat.GetEmissionColor();
                float emissionStrength = mat.GetEmissionStrength();
                Math::Vec4 emission(emissionColor.x, emissionColor.y, emissionColor.z, emissionStrength);
                bgfx::setUniform(u_Emission, &emission);
                Math::Vec4 textureBlendParams(blendMode, blendFactor, 0.0f, 0.0f);
                bgfx::setUniform(u_TextureBlend, &textureBlendParams);
                } else {
                    // Invalid material ID - use default gray material instead of black
                    albedoColor = Math::Vec4(0.7f, 0.7f, 0.7f, 1.0f);
                    bgfx::setUniform(u_AlbedoColor, &albedoColor);
                    Math::Vec4 materialParams(0.0f, 0.5f, 0.0f, 0.0f); // Default 0.5 roughness
                    bgfx::setUniform(u_MaterialParams, &materialParams);
                    Math::Vec4 emission(0.0f, 0.0f, 0.0f, 0.0f); // No emission
                    bgfx::setUniform(u_Emission, &emission);
                    Math::Vec4 textureBlendParams(0.0f, 0.0f, 0.0f, 0.0f);
                    bgfx::setUniform(u_TextureBlend, &textureBlendParams);
                }
            }

            if (bgfx::isValid(albedoTexture)) {
                bgfx::setTexture(2, s_TexAlbedo, albedoTexture);
            } else {
                bgfx::setTexture(2, s_TexAlbedo, defaultWhiteTexture);
            }
            if (bgfx::isValid(detailTexture)) {
                bgfx::setTexture(4, s_TexAlbedo2, detailTexture);
            } else {
                bgfx::setTexture(4, s_TexAlbedo2, defaultWhiteTexture);
            }
            if (bgfx::isValid(blendMaskTexture)) {
                bgfx::setTexture(5, s_TexBlendMask, blendMaskTexture);
            } else {
                bgfx::setTexture(5, s_TexBlendMask, defaultWhiteTexture);
            }
            if (bgfx::isValid(roughnessTexture)) {
                bgfx::setTexture(6, s_TexRoughness, roughnessTexture);
            } else {
                bgfx::setTexture(6, s_TexRoughness, defaultRoughnessTexture);
            }
            // Metallic texture support: Material struct doesn't have MetallicTexIndex yet,
            // but shader supports it for future use. Always bind default for now.
            bgfx::setTexture(7, s_TexMetallic, defaultMetallicTexture);

            // Normal map texture
            if (bgfx::isValid(normalTexture) && hasNormalMap) {
                bgfx::setTexture(8, s_TexNormal, normalTexture);
            } else {
                bgfx::setTexture(8, s_TexNormal, defaultNormalTexture);
            }

            // Set normal map parameters: strength (x), has normal map (y)
            Math::Vec4 normalMapParams(1.0f, hasNormalMap ? 1.0f : 0.0f, 0.0f, 0.0f);
            bgfx::setUniform(u_NormalMapParams, &normalMapParams);

            // Set lightmap parameters and texture
            // Get lightmap scale/offset from material if available
            Math::Vec2 lightmapScale(1.0f, 1.0f);
            Math::Vec2 lightmapOffset(0.0f, 0.0f);
            if (currentMat) {
                lightmapScale = currentMat->GetLightmapScale();
                lightmapOffset = currentMat->GetLightmapOffset();
            }
            Math::Vec4 lightmapParams(lightmapScale.x, lightmapScale.y, lightmapOffset.x, lightmapOffset.y);
            bgfx::setUniform(u_LightmapParams, &lightmapParams);
            bgfx::setTexture(9, s_TexLightmap, defaultLightmapTexture); // TODO: Bind actual lightmap texture when available

            uint64_t state = BGFX_STATE_WRITE_R | BGFX_STATE_WRITE_G | BGFX_STATE_WRITE_B | BGFX_STATE_WRITE_A
                            | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_MSAA;
            if (isTransparent) {
                state |= BGFX_STATE_BLEND_ALPHA;
                state &= ~BGFX_STATE_WRITE_Z;
            }
            bgfx::setState(state);
            bgfx::submit(PostProcessing::VIEW_SCENE, m_sceneProgram);
            trianglesSubmitted += static_cast<uint32_t>(MeshPtr->Indices.size() / 3);
        } else {
            // Static buffers - render each submesh separately with index offsets
            for (const auto& SubMesh : MeshPtr->SubMeshes) {
                // Set material properties for this submesh
                Math::Vec4 albedoColor(0.7f, 0.7f, 0.7f, 0.5f);
                float metallic = 0.0f;
                bgfx::TextureHandle albedoTexture = BGFX_INVALID_HANDLE;
                bgfx::TextureHandle detailTexture = BGFX_INVALID_HANDLE;
                bgfx::TextureHandle blendMaskTexture = BGFX_INVALID_HANDLE;
                bgfx::TextureHandle roughnessTexture = BGFX_INVALID_HANDLE;
                bgfx::TextureHandle normalTexture = BGFX_INVALID_HANDLE;
                float blendMode = 0.0f;
                float blendFactor = 0.0f;
                bool isTransparent = false;
                bool hasNormalMap = false;
                const Core::Material* currentMatSub = nullptr;

                if (materialLib) {
                    uint32_t matID = SubMesh.MaterialID;
                    const auto& materials = materialLib->GetMaterials();
                    if (matID != UINT32_MAX && matID < materials.size()) {
                        const Core::Material& mat = materials[matID];
                        currentMatSub = &mat;
                        Math::Vec3 albedo = mat.GetAlbedoColor();
                        float roughness = mat.GetRoughness();
                        albedoColor = Math::Vec4(albedo.x, albedo.y, albedo.z, mat.Opacity / 255.0f);
                        metallic = mat.GetMetallicFactor();
                        isTransparent = (mat.Flags & Core::MaterialFlag_Transparent) != 0;

                        if (mat.AlbedoTexIndex != 0xFFFF && m_textureRegistry) {
                            albedoTexture = m_textureRegistry->Get(mat.AlbedoTexIndex);
                        }
                        if (mat.AlbedoTexIndex2 != 0xFFFF && m_textureRegistry) {
                            detailTexture = m_textureRegistry->Get(mat.AlbedoTexIndex2);
                        }
                        if (mat.AlbedoTexIndex3 != 0xFFFF && m_textureRegistry) {
                            blendMaskTexture = m_textureRegistry->Get(mat.AlbedoTexIndex3);
                        }
                        if (mat.RoughnessTexIndex != 0xFFFF && m_textureRegistry) {
                            roughnessTexture = m_textureRegistry->Get(mat.RoughnessTexIndex);
                        }
                        if (mat.NormalMapIndex != 0xFFFF && m_textureRegistry) {
                            normalTexture = m_textureRegistry->Get(mat.NormalMapIndex);
                            hasNormalMap = true;
                        }
                        blendMode = static_cast<float>(mat.TextureBlendMode);
                        blendFactor = mat.GetTextureBlendFactor();

                        bgfx::setUniform(u_AlbedoColor, &albedoColor);
                        Math::Vec4 materialParams(metallic, roughness, 0.0f, 0.0f);
                        bgfx::setUniform(u_MaterialParams, &materialParams);
                        Math::Vec3 emissionColor = mat.GetEmissionColor();
                        float emissionStrength = mat.GetEmissionStrength();
                        Math::Vec4 emission(emissionColor.x, emissionColor.y, emissionColor.z, emissionStrength);
                        bgfx::setUniform(u_Emission, &emission);
                        Math::Vec4 textureBlendParams(blendMode, blendFactor, 0.0f, 0.0f);
                        bgfx::setUniform(u_TextureBlend, &textureBlendParams);
                    } else {
                        // Invalid material ID - use default gray material instead of black
                        albedoColor = Math::Vec4(0.7f, 0.7f, 0.7f, 1.0f);
                        bgfx::setUniform(u_AlbedoColor, &albedoColor);
                        Math::Vec4 materialParams(0.0f, 0.5f, 0.0f, 0.0f);
                        bgfx::setUniform(u_MaterialParams, &materialParams);
                        Math::Vec4 emission(0.0f, 0.0f, 0.0f, 0.0f); // No emission
                        bgfx::setUniform(u_Emission, &emission);
                        Math::Vec4 textureBlendParams(0.0f, 0.0f, 0.0f, 0.0f);
                        bgfx::setUniform(u_TextureBlend, &textureBlendParams);
                    }
                }

        if (bgfx::isValid(albedoTexture)) {
            bgfx::setTexture(2, s_TexAlbedo, albedoTexture);
        } else {
            bgfx::setTexture(2, s_TexAlbedo, defaultWhiteTexture);
        }
        if (bgfx::isValid(detailTexture)) {
            bgfx::setTexture(4, s_TexAlbedo2, detailTexture);
        } else {
            bgfx::setTexture(4, s_TexAlbedo2, defaultWhiteTexture);
        }
        if (bgfx::isValid(blendMaskTexture)) {
            bgfx::setTexture(5, s_TexBlendMask, blendMaskTexture);
        } else {
            bgfx::setTexture(5, s_TexBlendMask, defaultWhiteTexture);
        }
        if (bgfx::isValid(roughnessTexture)) {
            bgfx::setTexture(6, s_TexRoughness, roughnessTexture);
        } else {
            bgfx::setTexture(6, s_TexRoughness, defaultRoughnessTexture);
        }
        // Metallic texture support: Material struct doesn't have MetallicTexIndex yet,
        // but shader supports it for future use. Always bind default for now.
        bgfx::setTexture(7, s_TexMetallic, defaultMetallicTexture);

        // Normal map texture
        if (bgfx::isValid(normalTexture) && hasNormalMap) {
            bgfx::setTexture(8, s_TexNormal, normalTexture);
        } else {
            bgfx::setTexture(8, s_TexNormal, defaultNormalTexture);
        }

        // Set normal map parameters: strength (x), has normal map (y)
        Math::Vec4 normalMapParams(1.0f, hasNormalMap ? 1.0f : 0.0f, 0.0f, 0.0f);
        bgfx::setUniform(u_NormalMapParams, &normalMapParams);

        // Set lightmap parameters and texture
        // Get lightmap scale/offset from material if available
        Math::Vec2 lightmapScale(1.0f, 1.0f);
        Math::Vec2 lightmapOffset(0.0f, 0.0f);
        if (currentMatSub) {
            lightmapScale = currentMatSub->GetLightmapScale();
            lightmapOffset = currentMatSub->GetLightmapOffset();
        }
        Math::Vec4 lightmapParams(lightmapScale.x, lightmapScale.y, lightmapOffset.x, lightmapOffset.y);
        bgfx::setUniform(u_LightmapParams, &lightmapParams);
        bgfx::setTexture(9, s_TexLightmap, defaultLightmapTexture); // TODO: Bind actual lightmap texture when available

        uint64_t state = BGFX_STATE_WRITE_R | BGFX_STATE_WRITE_G | BGFX_STATE_WRITE_B | BGFX_STATE_WRITE_A
                        | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_MSAA;
        if (isTransparent) {
            state |= BGFX_STATE_BLEND_ALPHA;
                    state &= ~BGFX_STATE_WRITE_Z;
        }
        bgfx::setState(state);

                // Render this submesh's indices with offset
                bgfx::setIndexBuffer(
                    bgfx::IndexBufferHandle{ MeshPtr->IndexBufferHandle.Handle },
                    SubMesh.IndexStart,
                    SubMesh.IndexCount
                );
        bgfx::submit(PostProcessing::VIEW_SCENE, m_sceneProgram);

                trianglesSubmitted += static_cast<uint32_t>(SubMesh.IndexCount / 3);
            }
        }

        // Render outline for selected/hovered objects
        bool isSelected = m_selectedObjects.find(ObjID) != m_selectedObjects.end();
        bool isHovered = (m_hoveredObject == ObjID);
        if (isSelected || isHovered) {
            Math::Vec4 outlineColor;
            if (isSelected) {
                outlineColor = Math::Vec4(1.0f, 0.8f, 0.0f, 1.0f); // Yellow for selected
            } else {
                outlineColor = Math::Vec4(0.2f, 0.6f, 1.0f, 1.0f); // Blue for hovered
            }
            RenderObjectOutline(ObjID, scene, meshLib, camera, outlineColor);
        }
        }
    }

    Core::Profiler::Instance().SetCounter("TrianglesSubmitted", trianglesSubmitted);
    Core::Profiler::Instance().SetCounter("VisibleObjects", static_cast<int64_t>(visibleObjects.size()));
}

void SceneRenderer::RenderObjectOutline(SceneObjectID objID, Scene& scene, MeshLibrary* meshLib, const Camera& camera, const Math::Vec4& outlineColor) {
    uint32_t MeshID = scene.GetMeshID(objID);
    Mesh* MeshPtr = meshLib->GetMesh(MeshID);
    if (!MeshPtr || MeshPtr->Vertices.empty()) return;

    const Math::Matrix4& WorldMat = scene.GetWorldMatrix(objID);

    // Create scaled matrix for outline (scale up by ~1.02)
    Math::Matrix4 scaledWorld = WorldMat;
    scaledWorld = Math::Matrix4::Scale(Math::Vec3(1.02f, 1.02f, 1.02f)) * scaledWorld;

    float model[16];
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            model[c*4 + r] = scaledWorld.M[r][c];
    bgfx::setTransform(model);

    // Setup buffers
    if (!SetupBuffers(MeshPtr, m_optimizeStaticBuffers)) {
        return;
    }

    // Use outline color instead of material color
    bgfx::setUniform(u_AlbedoColor, &outlineColor);
    Math::Vec4 materialParams(0.0f, 0.0f, 0.0f, 0.0f);
    bgfx::setUniform(u_MaterialParams, &materialParams);
    Math::Vec4 textureBlendParams(0.0f, 0.0f, 0.0f, 0.0f);
    bgfx::setUniform(u_TextureBlend, &textureBlendParams);

    // Use default white texture
    static bgfx::TextureHandle defaultWhiteTexture = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(defaultWhiteTexture)) {
        uint8_t whitePixel[4] = {255, 255, 255, 255};
        const bgfx::Memory* mem = bgfx::copy(whitePixel, 4);
        defaultWhiteTexture = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_NONE, mem);
    }
    bgfx::setTexture(2, s_TexAlbedo, defaultWhiteTexture);
    bgfx::setTexture(4, s_TexAlbedo2, defaultWhiteTexture);
    bgfx::setTexture(5, s_TexBlendMask, defaultWhiteTexture);

    // Render outline with depth test but write only to color (no depth write)
    // Cull front faces so we only see the back-facing scaled version
    uint64_t state = BGFX_STATE_WRITE_R | BGFX_STATE_WRITE_G | BGFX_STATE_WRITE_B | BGFX_STATE_WRITE_A
                    | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_CULL_CW | BGFX_STATE_MSAA;
    bgfx::setState(state);

    bool UsesStaticBuffers = (m_optimizeStaticBuffers && MeshPtr->IsStatic &&
                              MeshPtr->IndexBufferHandle.Handle != 0xFFFF);

    if (MeshPtr->SubMeshes.empty() || !UsesStaticBuffers) {
        bgfx::submit(PostProcessing::VIEW_SCENE, m_sceneProgram);
    } else {
        for (const auto& SubMesh : MeshPtr->SubMeshes) {
            bgfx::setIndexBuffer(
                bgfx::IndexBufferHandle{ MeshPtr->IndexBufferHandle.Handle },
                SubMesh.IndexStart,
                SubMesh.IndexCount
            );
            bgfx::submit(PostProcessing::VIEW_SCENE, m_sceneProgram);
        }
    }
}

} // namespace Solstice::Render
