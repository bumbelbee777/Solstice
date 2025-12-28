#include <Render/PostProcessing.hxx>
#include <Render/ShaderLoader.hxx>
#include <Core/Debug.hxx>
#include <Core/ScopeTimer.hxx>
#include <array>
#include <fstream>
#include <vector>
#include <cstring>
#include <unordered_map>

namespace Solstice::Render {

PostProcessing::PostProcessing() {
    m_layout
        .begin(bgfx::getRendererType())
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    u_shadowParams = bgfx::createUniform("u_shadowParams", bgfx::UniformType::Vec4);
    s_texColor = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    s_texShadow = bgfx::createUniform("s_texShadow", bgfx::UniformType::Sampler);
    u_hdrExposure = bgfx::createUniform("u_hdrExposure", bgfx::UniformType::Vec4);
    u_motionBlurParams = bgfx::createUniform("u_motionBlurParams", bgfx::UniformType::Vec4);
    u_prevViewProj = bgfx::createUniform("u_prevViewProj", bgfx::UniformType::Mat4);
    s_texDepth = bgfx::createUniform("s_texDepth", bgfx::UniformType::Sampler);
    
    // Bloom uniforms
    u_bloomParams = bgfx::createUniform("u_bloomParams", bgfx::UniformType::Vec4);
    
    // God ray / volumetric uniforms
    u_godRayParams = bgfx::createUniform("u_godRayParams", bgfx::UniformType::Vec4);
    s_texVolumetric = bgfx::createUniform("s_texVolumetric", bgfx::UniformType::Sampler);
}

PostProcessing::~PostProcessing() {
    Shutdown();
    bgfx::destroy(u_shadowParams);
    bgfx::destroy(s_texColor);
    bgfx::destroy(s_texShadow);
    if (bgfx::isValid(u_hdrExposure)) {
        bgfx::destroy(u_hdrExposure);
    }
    if (bgfx::isValid(u_motionBlurParams)) {
        bgfx::destroy(u_motionBlurParams);
    }
    if (bgfx::isValid(u_prevViewProj)) {
        bgfx::destroy(u_prevViewProj);
    }
    if (bgfx::isValid(s_texDepth)) {
        bgfx::destroy(s_texDepth);
    }
    if (bgfx::isValid(u_bloomParams)) {
        bgfx::destroy(u_bloomParams);
    }
    if (bgfx::isValid(u_godRayParams)) {
        bgfx::destroy(u_godRayParams);
    }
    if (bgfx::isValid(s_texVolumetric)) {
        bgfx::destroy(s_texVolumetric);
    }
}

void PostProcessing::Initialize(uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;

    CreateResources();

    // Load shaders
    bgfx::ShaderHandle vshPost = ShaderLoader::LoadShader("vs_post.bin");
    bgfx::ShaderHandle fshPost = ShaderLoader::LoadShader("fs_post.bin");
    m_progPost = bgfx::createProgram(vshPost, fshPost, true);

    bgfx::ShaderHandle vshShadow = ShaderLoader::LoadShader("vs_shadow.bin");
    bgfx::ShaderHandle fshShadow = ShaderLoader::LoadShader("fs_shadow.bin");
    m_progShadow = bgfx::createProgram(vshShadow, fshShadow, true);
}

void PostProcessing::Shutdown() {
    DestroyResources();

    if (bgfx::isValid(m_progPost)) bgfx::destroy(m_progPost);
    if (bgfx::isValid(m_progShadow)) bgfx::destroy(m_progShadow);

    m_progPost = BGFX_INVALID_HANDLE;
    m_progShadow = BGFX_INVALID_HANDLE;
}

void PostProcessing::Resize(uint32_t width, uint32_t height) {
    if (m_width == width && m_height == height) return;
    m_width = width;
    m_height = height;

    DestroyResources();
    CreateResources();
}

void PostProcessing::CreateResources() {
    // 1. Shadow Map
    uint64_t shadowFlags = BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
    m_shadowMap = bgfx::createTexture2D(m_shadowMapSize, m_shadowMapSize, false, 1, bgfx::TextureFormat::D32, shadowFlags);
    m_shadowFB = bgfx::createFrameBuffer(1, &m_shadowMap, true);

    if (!bgfx::isValid(m_shadowFB)) SIMPLE_LOG("PostProcessing: Failed to create Shadow FB");

    // 2. Scene FB (Color + Depth)
    uint64_t sceneFlags = BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
    m_sceneColor = bgfx::createTexture2D(m_width, m_height, false, 1, bgfx::TextureFormat::RGBA16F, sceneFlags);
    m_sceneDepth = bgfx::createTexture2D(m_width, m_height, false, 1, bgfx::TextureFormat::D24S8, sceneFlags);

    bgfx::TextureHandle sceneTexs[] = { m_sceneColor, m_sceneDepth };
    m_sceneFB = bgfx::createFrameBuffer(2, sceneTexs, true); // true = destroy textures when FB destroyed

    if (!bgfx::isValid(m_sceneFB)) SIMPLE_LOG("PostProcessing: Failed to create Scene FB");

    // 3. Velocity Buffer (RG16F for 2D velocity vectors)
    uint64_t velocityFlags = BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
    m_velocityBuffer = bgfx::createTexture2D(m_width, m_height, false, 1, bgfx::TextureFormat::RG16F, velocityFlags);
    m_velocityFB = bgfx::createFrameBuffer(1, &m_velocityBuffer, true);

    if (!bgfx::isValid(m_velocityFB)) SIMPLE_LOG("PostProcessing: Failed to create Velocity FB");
}

void PostProcessing::DestroyResources() {
    if (bgfx::isValid(m_shadowFB)) bgfx::destroy(m_shadowFB);
    if (bgfx::isValid(m_shadowMap)) bgfx::destroy(m_shadowMap);
    if (bgfx::isValid(m_sceneFB)) bgfx::destroy(m_sceneFB);
    if (bgfx::isValid(m_sceneColor)) bgfx::destroy(m_sceneColor);
    if (bgfx::isValid(m_sceneDepth)) bgfx::destroy(m_sceneDepth);
    if (bgfx::isValid(m_velocityFB)) bgfx::destroy(m_velocityFB);
    if (bgfx::isValid(m_velocityBuffer)) bgfx::destroy(m_velocityBuffer);

    m_shadowFB = BGFX_INVALID_HANDLE;
    m_shadowMap = BGFX_INVALID_HANDLE;
    m_sceneFB = BGFX_INVALID_HANDLE;
    m_sceneColor = BGFX_INVALID_HANDLE;
    m_sceneDepth = BGFX_INVALID_HANDLE;
    m_velocityFB = BGFX_INVALID_HANDLE;
    m_velocityBuffer = BGFX_INVALID_HANDLE;
}

void PostProcessing::BeginShadowPass() {
    PROFILE_SCOPE("PostProcessing::BeginShadowPass");

    // Setup shadow pass (View ID 1 = VIEW_SHADOW)
    bgfx::setViewName(VIEW_SHADOW, "Shadow Pass");
    bgfx::setViewRect(VIEW_SHADOW, 0, 0, m_shadowMapSize, m_shadowMapSize);
    bgfx::setViewFrameBuffer(VIEW_SHADOW, m_shadowFB);
    bgfx::setViewClear(VIEW_SHADOW, BGFX_CLEAR_DEPTH, 0x00000000, 1.0f, 0);

    // Use light direction from SetLightDirection() (set by renderer from light sources)
    Math::Vec3 lightDir = m_lightDirection;

    // Make shadows follow camera - position light relative to camera
    // Use camera position, default to origin if not set
    Math::Vec3 camPos = m_cameraPos;
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
    m_shadowViewProj = proj * view;

    // Transpose for BGFX
    Math::Matrix4 viewT = view.Transposed();
    Math::Matrix4 projT = proj.Transposed();

    bgfx::setViewTransform(VIEW_SHADOW, &viewT.M[0][0], &projT.M[0][0]);
    bgfx::touch(VIEW_SHADOW);
}

void PostProcessing::SetLightDirection(const Math::Vec3& direction) {
    m_lightDirection = direction.Normalized();
}

void PostProcessing::BeginScenePass() {
    PROFILE_SCOPE("PostProcessing::BeginScenePass");

    // Setup scene pass (View ID 2 = VIEW_SCENE)
    bgfx::setViewName(VIEW_SCENE, "Scene Pass");
    bgfx::setViewRect(VIEW_SCENE, 0, 0, m_width, m_height);
    bgfx::setViewFrameBuffer(VIEW_SCENE, m_sceneFB);
    bgfx::setViewClear(VIEW_SCENE, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x101010FF, 1.0f, 0);
    bgfx::touch(VIEW_SCENE);
}

void PostProcessing::EndScenePass() {
    // Nothing explicit needed for now
}

void PostProcessing::Apply(bgfx::ViewId viewId) {
    PROFILE_SCOPE("PostProcessing::Apply");

    if (!bgfx::isValid(m_progPost)) {
        static bool s_logged = false;
        if (!s_logged) { SIMPLE_LOG("PostProcessing: Cannot Apply - Invalid Program"); s_logged = true; }
        return;
    }

    // Render fullscreen quad to backbuffer (or designated view)
    bgfx::setViewName(viewId, "Post Process");
    bgfx::setViewRect(viewId, 0, 0, m_width, m_height);

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
    if (!bgfx::isValid(m_sceneColor)) {
        static bool s_logged = false;
        if (!s_logged) { SIMPLE_LOG("PostProcessing: Invalid scene color texture!"); s_logged = true; }
        return;
    }
    bgfx::setTexture(0, s_texColor, m_sceneColor);

    // Bind depth and velocity textures for motion blur (if enabled)
    if (m_motionBlurSettings.Enabled && bgfx::isValid(m_sceneDepth)) {
        bgfx::setTexture(1, s_texDepth, m_sceneDepth);

        // Bind velocity buffer if available
        if (bgfx::isValid(m_velocityBuffer)) {
            static bgfx::UniformHandle s_texVelocity = bgfx::createUniform("s_texVelocity", bgfx::UniformType::Sampler);
            bgfx::setTexture(2, s_texVelocity, m_velocityBuffer);
        }

        // Set motion blur parameters
        if (bgfx::isValid(u_motionBlurParams)) {
            float blurParams[4] = {
                m_motionBlurSettings.Strength,
                static_cast<float>(m_motionBlurSettings.SampleCount),
                m_motionBlurSettings.DepthScale,
                0.0f
            };
            bgfx::setUniform(u_motionBlurParams, blurParams);
        }

        // Set previous view-proj matrix
        if (bgfx::isValid(u_prevViewProj)) {
            Math::Matrix4 prevViewProjT = m_previousViewProj.Transposed();
            bgfx::setUniform(u_prevViewProj, &prevViewProjT.M[0][0]);
        }

        // Set viewport size
        static bgfx::UniformHandle u_viewportSize = bgfx::createUniform("u_viewportSize", bgfx::UniformType::Vec4);
        if (bgfx::isValid(u_viewportSize)) {
            float viewportData[4] = { static_cast<float>(m_width), static_cast<float>(m_height), 0.0f, 0.0f };
            bgfx::setUniform(u_viewportSize, viewportData);
        }
    }

    // Set HDR exposure uniform
    if (bgfx::isValid(u_hdrExposure)) {
        float exposureData[4] = { m_hdrExposure, 0.0f, 0.0f, 0.0f };
        bgfx::setUniform(u_hdrExposure, exposureData);
    }

    // Set bloom uniforms
    if (bgfx::isValid(u_bloomParams)) {
        float bloomData[4] = {
            m_BloomSettings.Threshold,
            m_BloomSettings.Intensity,
            m_BloomSettings.Radius,
            m_BloomSettings.Enabled ? 1.0f : 0.0f
        };
        bgfx::setUniform(u_bloomParams, bloomData);
    }

    // Set god ray / volumetric uniforms and texture
    if (bgfx::isValid(u_godRayParams)) {
        float godRayData[4] = {
            m_GodRaySettings.Density,
            m_GodRaySettings.Decay,
            m_GodRaySettings.Exposure,
            m_GodRaySettings.Enabled ? 1.0f : 0.0f
        };
        bgfx::setUniform(u_godRayParams, godRayData);
    }
    
    // Bind volumetric texture if available
    if (m_GodRaySettings.Enabled && bgfx::isValid(m_VolumetricTexture) && bgfx::isValid(s_texVolumetric)) {
        bgfx::setTexture(3, s_texVolumetric, m_VolumetricTexture);
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

    if (bgfx::getAvailTransientVertexBuffer(4, m_layout) >= 4) {
        bgfx::allocTransientVertexBuffer(&tvb, 4, m_layout);
        std::memcpy(tvb.data, verts, sizeof(verts));
        bgfx::allocTransientIndexBuffer(&tib, 6);
        std::memcpy(tib.data, indices, sizeof(indices));

        bgfx::setVertexBuffer(0, &tvb);
        bgfx::setIndexBuffer(&tib);

        // No cull, Write RGB/A, No Z test (always pass)
        uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_ALWAYS;
        bgfx::setState(state);

        bgfx::submit(viewId, m_progPost);
    } else {
        SIMPLE_LOG("PostProcessing: Failed to allocate transient buffers");
    }
}

void PostProcessing::SetRaytracingTextures(bgfx::TextureHandle shadowTexture, bgfx::TextureHandle aoTexture) {
    m_raytraceShadowTexture = shadowTexture;
    m_raytraceAOTexture = aoTexture;
    // TODO: Use these textures in the post-processing shader
    // For now, just store them for future use
}

void PostProcessing::SetMotionBlurQuality(MotionBlurQuality quality) {
    m_motionBlurSettings.Quality = quality;
    switch (quality) {
        case MotionBlurQuality::Low:
            m_motionBlurSettings.SampleCount = 6;
            m_motionBlurSettings.Strength = 0.75f;
            break;
        case MotionBlurQuality::Medium:
            m_motionBlurSettings.SampleCount = 12;
            m_motionBlurSettings.Strength = 1.0f;
            break;
        case MotionBlurQuality::High:
            m_motionBlurSettings.SampleCount = 24;
            m_motionBlurSettings.Strength = 1.25f;
            break;
    }
}

void PostProcessing::BeginVelocityPass() {
    PROFILE_SCOPE("PostProcessing::BeginVelocityPass");

    // Setup velocity pass (View ID 3 = VIEW_VELOCITY)
    bgfx::setViewName(VIEW_VELOCITY, "Velocity Pass");
    bgfx::setViewRect(VIEW_VELOCITY, 0, 0, m_width, m_height);
    bgfx::setViewFrameBuffer(VIEW_VELOCITY, m_velocityFB);
    bgfx::setViewClear(VIEW_VELOCITY, BGFX_CLEAR_COLOR, 0x00000000, 1.0f, 0);
    bgfx::touch(VIEW_VELOCITY);
}

void PostProcessing::EndVelocityPass() {
    // Velocity pass complete
}

void PostProcessing::UpdateObjectVelocity(uint32_t objectId, const Math::Matrix4& currentTransform) {
    auto it = m_previousTransforms.find(objectId);
    if (it != m_previousTransforms.end()) {
        // Calculate velocity from previous transform
        Math::Vec3 prevPos = it->second.GetTranslation();
        Math::Vec3 currPos = currentTransform.GetTranslation();
        Math::Vec3 velocity = currPos - prevPos;
        m_objectVelocities[objectId] = velocity;
    }
    m_previousTransforms[objectId] = currentTransform;
}

Math::Vec3 PostProcessing::GetObjectVelocity(uint32_t objectId) const {
    auto it = m_objectVelocities.find(objectId);
    if (it != m_objectVelocities.end()) {
        return it->second;
    }
    return Math::Vec3(0.0f, 0.0f, 0.0f);
}

} // namespace Solstice::Render
