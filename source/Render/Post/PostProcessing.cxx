#include <Render/Post/PostProcessing.hxx>
#include <Render/Assets/ShaderLoader.hxx>
#include <Core/Debug.hxx>
#include <Core/ScopeTimer.hxx>
#include <array>
#include <fstream>
#include <vector>
#include <cstring>
#include <unordered_map>

namespace Solstice::Render {

PostProcessing::PostProcessing() {
    m_Layout
        .begin(bgfx::getRendererType())
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    u_ShadowParams = bgfx::createUniform("u_ShadowParams", bgfx::UniformType::Vec4);
    s_TexColor = bgfx::createUniform("s_TexColor", bgfx::UniformType::Sampler);
    s_TexShadow = bgfx::createUniform("s_TexShadow", bgfx::UniformType::Sampler);
    u_HDRExposure = bgfx::createUniform("u_HDRExposure", bgfx::UniformType::Vec4);
    u_MotionBlurParams = bgfx::createUniform("u_MotionBlurParams", bgfx::UniformType::Vec4);
    u_PrevViewProj = bgfx::createUniform("u_PrevViewProj", bgfx::UniformType::Mat4);
    s_TexDepth = bgfx::createUniform("s_TexDepth", bgfx::UniformType::Sampler);

    // Bloom uniforms
    u_BloomParams = bgfx::createUniform("u_BloomParams", bgfx::UniformType::Vec4);

    // God ray / volumetric uniforms
    u_GodRayParams = bgfx::createUniform("u_GodRayParams", bgfx::UniformType::Vec4);
    s_TexVolumetric = bgfx::createUniform("s_TexVolumetric", bgfx::UniformType::Sampler);

    u_ReflectionParams = bgfx::createUniform("u_reflectionParams", bgfx::UniformType::Vec4);
    u_ReflectionViewProj = bgfx::createUniform("u_reflectionViewProj", bgfx::UniformType::Mat4);
    u_ReflectionInvViewProj = bgfx::createUniform("u_reflectionInvViewProj", bgfx::UniformType::Mat4);
    u_CameraPos = bgfx::createUniform("u_cameraPos", bgfx::UniformType::Vec4);
    s_TexReflectionProbe = bgfx::createUniform("s_texReflectionProbe", bgfx::UniformType::Sampler);
}

PostProcessing::~PostProcessing() {
    Shutdown();
    bgfx::destroy(u_ShadowParams);
    bgfx::destroy(s_TexColor);
    bgfx::destroy(s_TexShadow);
    if (bgfx::isValid(u_HDRExposure)) {
        bgfx::destroy(u_HDRExposure);
    }
    if (bgfx::isValid(u_MotionBlurParams)) {
        bgfx::destroy(u_MotionBlurParams);
    }
    if (bgfx::isValid(u_PrevViewProj)) {
        bgfx::destroy(u_PrevViewProj);
    }
    if (bgfx::isValid(s_TexDepth)) {
        bgfx::destroy(s_TexDepth);
    }
    if (bgfx::isValid(u_BloomParams)) {
        bgfx::destroy(u_BloomParams);
    }
    if (bgfx::isValid(u_GodRayParams)) {
        bgfx::destroy(u_GodRayParams);
    }
    if (bgfx::isValid(s_TexVolumetric)) {
        bgfx::destroy(s_TexVolumetric);
    }
    if (bgfx::isValid(u_ReflectionParams)) {
        bgfx::destroy(u_ReflectionParams);
    }
    if (bgfx::isValid(u_ReflectionViewProj)) {
        bgfx::destroy(u_ReflectionViewProj);
    }
    if (bgfx::isValid(u_ReflectionInvViewProj)) {
        bgfx::destroy(u_ReflectionInvViewProj);
    }
    if (bgfx::isValid(u_CameraPos)) {
        bgfx::destroy(u_CameraPos);
    }
    if (bgfx::isValid(s_TexReflectionProbe)) {
        bgfx::destroy(s_TexReflectionProbe);
    }
}

void PostProcessing::Initialize(uint32_t width, uint32_t height) {
    m_Width = width;
    m_Height = height;

    CreateResources();

    // Load shaders
    bgfx::ShaderHandle vshPost = ShaderLoader::LoadShader("vs_post.bin");
    bgfx::ShaderHandle fshPost = ShaderLoader::LoadShader("fs_post.bin");
    m_ProgPost = bgfx::createProgram(vshPost, fshPost, true);

    bgfx::ShaderHandle vshShadow = ShaderLoader::LoadShader("vs_shadow.bin");
    bgfx::ShaderHandle fshShadow = ShaderLoader::LoadShader("fs_shadow.bin");
    m_ProgShadow = bgfx::createProgram(vshShadow, fshShadow, true);
}

void PostProcessing::Shutdown() {
    DestroyResources();

    if (bgfx::isValid(m_ProgPost)) bgfx::destroy(m_ProgPost);
    if (bgfx::isValid(m_ProgShadow)) bgfx::destroy(m_ProgShadow);

    m_ProgPost = BGFX_INVALID_HANDLE;
    m_ProgShadow = BGFX_INVALID_HANDLE;
}

void PostProcessing::Resize(uint32_t width, uint32_t height) {
    if (m_Width == width && m_Height == height) return;
    m_Width = width;
    m_Height = height;

    DestroyResources();
    CreateResources();
}

void PostProcessing::CreateResources() {
    // 1. Shadow Map
    uint64_t shadowFlags = BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
    m_ShadowMap = bgfx::createTexture2D(m_ShadowMapSize, m_ShadowMapSize, false, 1, bgfx::TextureFormat::D32, shadowFlags);
    m_ShadowFB = bgfx::createFrameBuffer(1, &m_ShadowMap, true);

    if (!bgfx::isValid(m_ShadowFB)) SIMPLE_LOG("PostProcessing: Failed to create Shadow FB");

    // 2. Scene FB (Color + Depth)
    uint64_t sceneFlags = BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
    m_SceneColor = bgfx::createTexture2D(m_Width, m_Height, false, 1, bgfx::TextureFormat::RGBA16F, sceneFlags);
    m_SceneDepth = bgfx::createTexture2D(m_Width, m_Height, false, 1, bgfx::TextureFormat::D24S8, sceneFlags);

    bgfx::TextureHandle sceneTexs[] = { m_SceneColor, m_SceneDepth };
    m_SceneFB = bgfx::createFrameBuffer(2, sceneTexs, true); // true = destroy textures when FB destroyed

    if (!bgfx::isValid(m_SceneFB)) SIMPLE_LOG("PostProcessing: Failed to create Scene FB");

    // 3. Velocity Buffer (RG16F for 2D velocity vectors)
    uint64_t velocityFlags = BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
    m_VelocityBuffer = bgfx::createTexture2D(m_Width, m_Height, false, 1, bgfx::TextureFormat::RG16F, velocityFlags);
    m_VelocityFB = bgfx::createFrameBuffer(1, &m_VelocityBuffer, true);

    if (!bgfx::isValid(m_VelocityFB)) SIMPLE_LOG("PostProcessing: Failed to create Velocity FB");
}

void PostProcessing::DestroyResources() {
    if (bgfx::isValid(m_ShadowFB)) bgfx::destroy(m_ShadowFB);
    if (bgfx::isValid(m_ShadowMap)) bgfx::destroy(m_ShadowMap);
    if (bgfx::isValid(m_SceneFB)) bgfx::destroy(m_SceneFB);
    if (bgfx::isValid(m_SceneColor)) bgfx::destroy(m_SceneColor);
    if (bgfx::isValid(m_SceneDepth)) bgfx::destroy(m_SceneDepth);
    if (bgfx::isValid(m_VelocityFB)) bgfx::destroy(m_VelocityFB);
    if (bgfx::isValid(m_VelocityBuffer)) bgfx::destroy(m_VelocityBuffer);

    m_ShadowFB = BGFX_INVALID_HANDLE;
    m_ShadowMap = BGFX_INVALID_HANDLE;
    m_SceneFB = BGFX_INVALID_HANDLE;
    m_SceneColor = BGFX_INVALID_HANDLE;
    m_SceneDepth = BGFX_INVALID_HANDLE;
    m_VelocityFB = BGFX_INVALID_HANDLE;
    m_VelocityBuffer = BGFX_INVALID_HANDLE;
}

void PostProcessing::BeginShadowPass() {
    PROFILE_SCOPE("PostProcessing::BeginShadowPass");

    // Setup shadow pass (View ID 1 = VIEW_SHADOW)
    bgfx::setViewName(VIEW_SHADOW, "Shadow Pass");
    bgfx::setViewRect(VIEW_SHADOW, 0, 0, m_ShadowMapSize, m_ShadowMapSize);
    bgfx::setViewFrameBuffer(VIEW_SHADOW, m_ShadowFB);
    bgfx::setViewClear(VIEW_SHADOW, BGFX_CLEAR_DEPTH, 0x00000000, 1.0f, 0);

    // Use light direction from SetLightDirection() (set by renderer from light sources)
    Math::Vec3 lightDir = m_LightDirection;

    // Make shadows follow camera - position light relative to camera
    // Use camera position, default to origin if not set
    Math::Vec3 camPos = m_CameraPos;
    // Check if camera position is near zero using Dot product
    if (camPos.Dot(camPos) < 0.001f) {
        camPos = Math::Vec3(0.0f, 0.0f, 0.0f); // Default to origin
    }

    // Light target is at camera position (where we're looking)
    // Project camera position onto ground plane (y=0) for better shadow coverage
    // This ensures shadows follow the camera's ground position
    Math::Vec3 lightTarget = Math::Vec3(camPos.x, 0.0f, camPos.z); // Project to ground level

    // Position light far away in the opposite direction of light, but centered on camera
    // This makes the shadow frustum follow the camera dynamically
    Math::Vec3 lightPos = lightTarget - lightDir * 400.0f; // Position light far away, centered on camera

    // Compute shadow matrices - larger area to ensure ground is covered
    // Center the orthographic projection on the camera's ground position
    // This matrix is recalculated every frame with the current camera position
    Math::Matrix4 view = Math::Matrix4::LookAt(lightPos, lightTarget, Math::Vec3(0,1,0));
    float area = 150.0f; // Increased shadow area to cover more ground around camera
    Math::Matrix4 proj = Math::Matrix4::Orthographic(-area, area, -area, area, 50.0f, 800.0f);

    // Store shadow matrix (this is updated every frame with camera position)
    m_ShadowViewProj = proj * view;

    // Transpose for BGFX
    Math::Matrix4 viewT = view.Transposed();
    Math::Matrix4 projT = proj.Transposed();

    bgfx::setViewTransform(VIEW_SHADOW, &viewT.M[0][0], &projT.M[0][0]);
    bgfx::touch(VIEW_SHADOW);
}

void PostProcessing::SetLightDirection(const Math::Vec3& direction) {
    m_LightDirection = direction.Normalized();
}

void PostProcessing::BeginScenePass() {
    PROFILE_SCOPE("PostProcessing::BeginScenePass");

    // Setup scene pass (View ID 2 = VIEW_SCENE)
    bgfx::setViewName(VIEW_SCENE, "Scene Pass");
    bgfx::setViewRect(VIEW_SCENE, 0, 0, m_Width, m_Height);
    bgfx::setViewFrameBuffer(VIEW_SCENE, m_SceneFB);
    bgfx::setViewClear(VIEW_SCENE, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x101010FF, 1.0f, 0);
    bgfx::touch(VIEW_SCENE);
}

void PostProcessing::EndScenePass() {
    // Nothing explicit needed for now
}

void PostProcessing::Apply(bgfx::ViewId viewId) {
    PROFILE_SCOPE("PostProcessing::Apply");

    if (!bgfx::isValid(m_ProgPost)) {
        static bool s_logged = false;
        if (!s_logged) { SIMPLE_LOG("PostProcessing: Cannot Apply - Invalid Program"); s_logged = true; }
        return;
    }

    // Render fullscreen quad to backbuffer (or designated view)
    bgfx::setViewName(viewId, "Post Process");
    bgfx::setViewRect(viewId, 0, 0, m_Width, m_Height);

    // Clear to BLACK now that we expect it to work (or keep Pink for one more test if you prefer, but let's assume it works)
    // Let's keep a dark gray clear to visualize "viewport" vs "texture"
    bgfx::setViewClear(viewId, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x202020FF, 1.0f, 0);

    // Bind output to default backbuffer (FrameBufferHandle invalid/null)
    bgfx::FrameBufferHandle invalid = BGFX_INVALID_HANDLE;
    bgfx::setViewFrameBuffer(viewId, invalid);

    // Use Identity Projection for NDC Quad
    float proj[16];
    std::memset(proj, 0, sizeof(proj));
    proj[0] = 1.0f;
    proj[5] = 1.0f;
    proj[10] = 1.0f;
    proj[15] = 1.0f;
    bgfx::setViewTransform(viewId, nullptr, proj);

    // Bind Scene Texture
    if (!bgfx::isValid(m_SceneColor)) {
        static bool s_logged = false;
        if (!s_logged) { SIMPLE_LOG("PostProcessing: Invalid scene color texture!"); s_logged = true; }
        return;
    }
    bgfx::setTexture(0, s_TexColor, m_SceneColor);

    // Bind depth texture (used by SSR/motion blur)
    if (bgfx::isValid(m_SceneDepth)) {
        bgfx::setTexture(1, s_TexDepth, m_SceneDepth);

        // Bind velocity buffer if available
        if (m_MotionBlurSettings.Enabled && bgfx::isValid(m_VelocityBuffer)) {
            static bgfx::UniformHandle s_texVelocity = bgfx::createUniform("s_texVelocity", bgfx::UniformType::Sampler);
            bgfx::setTexture(2, s_texVelocity, m_VelocityBuffer);
        }

    }

    // Set motion blur parameters
    if (bgfx::isValid(u_MotionBlurParams)) {
        float blurParams[4] = {
            m_MotionBlurSettings.Enabled ? m_MotionBlurSettings.Strength : 0.0f,
            m_MotionBlurSettings.Enabled ? static_cast<float>(m_MotionBlurSettings.SampleCount) : 0.0f,
            m_MotionBlurSettings.DepthScale,
            0.0f
        };
        bgfx::setUniform(u_MotionBlurParams, blurParams);
    }

    // Set previous view-proj matrix
    if (bgfx::isValid(u_PrevViewProj)) {
        Math::Matrix4 prevViewProjT = m_PreviousViewProj.Transposed();
        bgfx::setUniform(u_PrevViewProj, &prevViewProjT.M[0][0]);
    }

    // Set viewport size
    static bgfx::UniformHandle u_viewportSize = bgfx::createUniform("u_viewportSize", bgfx::UniformType::Vec4);
    if (bgfx::isValid(u_viewportSize)) {
        float viewportData[4] = { static_cast<float>(m_Width), static_cast<float>(m_Height), 0.0f, 0.0f };
        bgfx::setUniform(u_viewportSize, viewportData);
    }

    // Set HDR exposure uniform
    if (bgfx::isValid(u_HDRExposure)) {
        float exposureData[4] = { m_HDRExposure, 0.0f, 0.0f, 0.0f };
        bgfx::setUniform(u_HDRExposure, exposureData);
    }

    // Set bloom uniforms
    if (bgfx::isValid(u_BloomParams)) {
        float bloomData[4] = {
            m_BloomSettings.Threshold,
            m_BloomSettings.Intensity,
            m_BloomSettings.Radius,
            m_BloomSettings.Enabled ? 1.0f : 0.0f
        };
        bgfx::setUniform(u_BloomParams, bloomData);
    }

    // Set god ray / volumetric uniforms and texture
    if (bgfx::isValid(u_GodRayParams)) {
        float godRayData[4] = {
            m_GodRaySettings.Density,
            m_GodRaySettings.Decay,
            m_GodRaySettings.Exposure,
            m_GodRaySettings.Enabled ? 1.0f : 0.0f
        };
        bgfx::setUniform(u_GodRayParams, godRayData);
    }

    // Bind volumetric texture if available
    if (m_GodRaySettings.Enabled && bgfx::isValid(m_VolumetricTexture) && bgfx::isValid(s_TexVolumetric)) {
        bgfx::setTexture(3, s_TexVolumetric, m_VolumetricTexture);
    }

    // Reflection settings and probe
    if (bgfx::isValid(u_ReflectionParams)) {
        float params[4] = {
            m_ReflectionSettings.Intensity,
            static_cast<float>(m_ReflectionSettings.MaxSteps),
            m_ReflectionSettings.Thickness,
            m_ReflectionSettings.Stride
        };
        bgfx::setUniform(u_ReflectionParams, params);
    }
    if (bgfx::isValid(u_ReflectionViewProj) && bgfx::isValid(u_ReflectionInvViewProj)) {
        Math::Matrix4 viewProjT = m_ViewProj.Transposed();
        Math::Matrix4 invViewProjT = m_InvViewProj.Transposed();
        bgfx::setUniform(u_ReflectionViewProj, &viewProjT.M[0][0]);
        bgfx::setUniform(u_ReflectionInvViewProj, &invViewProjT.M[0][0]);
    }
    if (bgfx::isValid(u_CameraPos)) {
        float camData[4] = { m_CameraPosWorld.x, m_CameraPosWorld.y, m_CameraPosWorld.z, 0.0f };
        bgfx::setUniform(u_CameraPos, camData);
    }
    if (bgfx::isValid(s_TexReflectionProbe)) {
        bgfx::TextureHandle probeTexture = m_ReflectionProbeTexture;
        if (!bgfx::isValid(probeTexture)) {
            static bgfx::TextureHandle fallbackProbe = BGFX_INVALID_HANDLE;
            if (!bgfx::isValid(fallbackProbe)) {
                uint8_t face[4] = {102, 153, 230, 255};
                fallbackProbe = bgfx::createTextureCube(
                    1,
                    false, 1,
                    bgfx::TextureFormat::RGBA8,
                    BGFX_TEXTURE_NONE,
                    nullptr
                );
                for (uint8_t faceIdx = 0; faceIdx < 6; ++faceIdx) {
                    const bgfx::Memory* mem = bgfx::copy(face, sizeof(face));
                    bgfx::updateTextureCube(
                        fallbackProbe,
                        0,
                        faceIdx,
                        0,
                        0,
                        0,
                        1,
                        1,
                        mem
                    );
                }
            }
            probeTexture = fallbackProbe;
        }
        bgfx::setTexture(4, s_TexReflectionProbe, probeTexture);
    }

    // Fullscreen Quad Geometry in NDC [-1, 1]
    // Z = 0.0f should be safe for -1..1 or 0..1 depth if Depth Test is ALWAYS/OFF
    struct Vertex { float x,y,z; float u,v; };

    // Handle UV orientation based on backend caps if needed, but start with standard
    // Top-Left (-1, 1) -> UV (0, 0) (Vulkan/DX style top-down)
    // If GL, might need flip, but let's see.

    Vertex verts[4] = {
        { -1.0f,  1.0f, 0.0f, 0.0f, 0.0f }, // TL
        {  1.0f,  1.0f, 0.0f, 1.0f, 0.0f }, // TR
        { -1.0f, -1.0f, 0.0f, 0.0f, 1.0f }, // BL
        {  1.0f, -1.0f, 0.0f, 1.0f, 1.0f }  // BR
    };
    uint16_t indices[6] = { 0, 1, 2, 2, 1, 3 };

    bgfx::TransientVertexBuffer tvb;
    bgfx::TransientIndexBuffer tib;

    if (bgfx::getAvailTransientVertexBuffer(4, m_Layout) >= 4) {
        bgfx::allocTransientVertexBuffer(&tvb, 4, m_Layout);
        std::memcpy(tvb.data, verts, sizeof(verts));
        bgfx::allocTransientIndexBuffer(&tib, 6);
        std::memcpy(tib.data, indices, sizeof(indices));

        bgfx::setVertexBuffer(0, &tvb);
        bgfx::setIndexBuffer(&tib);

        // No cull, Write RGB/A, No Z test (always pass)
        uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_ALWAYS;
        bgfx::setState(state);

        bgfx::submit(viewId, m_ProgPost);
    } else {
        SIMPLE_LOG("PostProcessing: Failed to allocate transient buffers");
    }
}

void PostProcessing::SetCameraMatrices(const Math::Matrix4& view, const Math::Matrix4& proj, const Math::Vec3& cameraPos) {
    // Store previous view-projection before updating (for motion blur)
    // Check if this is the first frame by checking if view-proj is identity/uninitialized
    static bool firstFrame = true;
    if (firstFrame) {
        // First frame - initialize both to current
        m_ViewProj = proj * view;
        m_PreviousViewProj = m_ViewProj;
        firstFrame = false;
    } else {
        // Subsequent frames - store previous before updating
        m_PreviousViewProj = m_ViewProj;
        m_ViewProj = proj * view;
    }
    m_InvViewProj = m_ViewProj.Inverse();
    m_CameraPosWorld = cameraPos;
}

void PostProcessing::SetRaytracingTextures(bgfx::TextureHandle shadowTexture, bgfx::TextureHandle aoTexture) {
    m_RaytraceShadowTexture = shadowTexture;
    m_RaytraceAOTexture = aoTexture;
    // TODO: Use these textures in the post-processing shader
    // For now, just store them for future use
}

void PostProcessing::SetMotionBlurQuality(MotionBlurQuality quality) {
    m_MotionBlurSettings.Quality = quality;
    switch (quality) {
        case MotionBlurQuality::Low:
            m_MotionBlurSettings.SampleCount = 6;
            m_MotionBlurSettings.Strength = 0.75f;
            break;
        case MotionBlurQuality::Medium:
            m_MotionBlurSettings.SampleCount = 12;
            m_MotionBlurSettings.Strength = 1.0f;
            break;
        case MotionBlurQuality::High:
            m_MotionBlurSettings.SampleCount = 24;
            m_MotionBlurSettings.Strength = 1.25f;
            break;
    }
}

void PostProcessing::BeginVelocityPass() {
    PROFILE_SCOPE("PostProcessing::BeginVelocityPass");

    // Setup velocity pass (View ID 3 = VIEW_VELOCITY)
    bgfx::setViewName(VIEW_VELOCITY, "Velocity Pass");
    bgfx::setViewRect(VIEW_VELOCITY, 0, 0, m_Width, m_Height);
    bgfx::setViewFrameBuffer(VIEW_VELOCITY, m_VelocityFB);
    bgfx::setViewClear(VIEW_VELOCITY, BGFX_CLEAR_COLOR, 0x00000000, 1.0f, 0);
    bgfx::touch(VIEW_VELOCITY);
}

void PostProcessing::EndVelocityPass() {
    // Velocity pass complete
}

void PostProcessing::UpdateObjectVelocity(uint32_t objectId, const Math::Matrix4& currentTransform) {
    auto it = m_PreviousTransforms.find(objectId);
    if (it != m_PreviousTransforms.end()) {
        // Calculate velocity from previous transform
        Math::Vec3 prevPos = it->second.GetTranslation();
        Math::Vec3 currPos = currentTransform.GetTranslation();
        Math::Vec3 velocity = currPos - prevPos;
        m_ObjectVelocities[objectId] = velocity;
    }
    m_PreviousTransforms[objectId] = currentTransform;
}

Math::Vec3 PostProcessing::GetObjectVelocity(uint32_t objectId) const {
    auto it = m_ObjectVelocities.find(objectId);
    if (it != m_ObjectVelocities.end()) {
        return it->second;
    }
    return Math::Vec3(0.0f, 0.0f, 0.0f);
}

} // namespace Solstice::Render
