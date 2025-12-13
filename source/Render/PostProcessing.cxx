#include <Render/PostProcessing.hxx>
#include <Core/Debug.hxx>
#include <array>
#include <fstream>
#include <vector>
#include <cstring>

namespace Solstice::Render {

// Helper to load shader (duplicated from SoftwareRenderer - should refactor later)
static bgfx::ShaderHandle LoadShader(const std::string& Name) {
    const std::vector<std::string> searchPaths = {
        "source/Shaders/bin/",
        "../source/Shaders/bin/",
        "../../source/Shaders/bin/",
        "../../../source/Shaders/bin/",
        "../../../../source/Shaders/bin/"
    };
    
    std::string finalPath;
    std::ifstream file;
    
    for (const auto& prefix : searchPaths) {
        std::string path = prefix + Name;
        file.open(path, std::ios::binary | std::ios::ate);
        if (file.is_open()) {
            finalPath = path;
            SIMPLE_LOG("PostProcessing: Found shader " + Name + " at " + path);
            break;
        }
    }
    
    if (!file.is_open()) {
        SIMPLE_LOG("PostProcessing: CRITICAL - Failed to find shader " + Name);
        return BGFX_INVALID_HANDLE;
    }
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    const bgfx::Memory* mem = bgfx::alloc(static_cast<uint32_t>(size) + 1);
    file.read(reinterpret_cast<char*>(mem->data), size);
    mem->data[mem->size - 1] = '\0';
    
    return bgfx::createShader(mem);
}

PostProcessing::PostProcessing() {
    m_layout
        .begin(bgfx::getRendererType())
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    u_shadowParams = bgfx::createUniform("u_shadowParams", bgfx::UniformType::Vec4);
    s_texColor = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    s_texShadow = bgfx::createUniform("s_texShadow", bgfx::UniformType::Sampler);
}

PostProcessing::~PostProcessing() {
    Shutdown();
    bgfx::destroy(u_shadowParams);
    bgfx::destroy(s_texColor);
    bgfx::destroy(s_texShadow);
}

void PostProcessing::Initialize(uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;
    
    CreateResources();
    
    // Load shaders
    bgfx::ShaderHandle vshPost = LoadShader("vs_post.bin");
    bgfx::ShaderHandle fshPost = LoadShader("fs_post.bin");
    m_progPost = bgfx::createProgram(vshPost, fshPost, true);
    
    bgfx::ShaderHandle vshShadow = LoadShader("vs_shadow.bin");
    bgfx::ShaderHandle fshShadow = LoadShader("fs_shadow.bin");
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
}

void PostProcessing::DestroyResources() {
    if (bgfx::isValid(m_shadowFB)) bgfx::destroy(m_shadowFB);
    if (bgfx::isValid(m_shadowMap)) bgfx::destroy(m_shadowMap);
    if (bgfx::isValid(m_sceneFB)) bgfx::destroy(m_sceneFB);
    if (bgfx::isValid(m_sceneColor)) bgfx::destroy(m_sceneColor);
    if (bgfx::isValid(m_sceneDepth)) bgfx::destroy(m_sceneDepth);
    
    m_shadowFB = BGFX_INVALID_HANDLE;
    m_shadowMap = BGFX_INVALID_HANDLE;
    m_sceneFB = BGFX_INVALID_HANDLE;
    m_sceneColor = BGFX_INVALID_HANDLE;
    m_sceneDepth = BGFX_INVALID_HANDLE;
}

void PostProcessing::BeginShadowPass() {
    // Setup shadow pass (View ID 1 = VIEW_SHADOW)
    bgfx::setViewName(VIEW_SHADOW, "Shadow Pass");
    bgfx::setViewRect(VIEW_SHADOW, 0, 0, m_shadowMapSize, m_shadowMapSize);
    bgfx::setViewFrameBuffer(VIEW_SHADOW, m_shadowFB);
    bgfx::setViewClear(VIEW_SHADOW, BGFX_CLEAR_DEPTH, 0x00000000, 1.0f, 0);
    
    // Define light position (directional)
    // For specific levels we might want to pass this in
    Math::Vec3 lightPos(100.0f, 150.0f, 100.0f);
    Math::Vec3 lightTarget(0.0f, 0.0f, 0.0f);
    
    // Compute shadow matrices
    Math::Matrix4 view = Math::Matrix4::LookAt(lightPos, lightTarget, Math::Vec3(0,1,0));
    float area = 60.0f; // Shadow area size (can be tuned or cascaded)
    Math::Matrix4 proj = Math::Matrix4::Orthographic(-area, area, -area, area, 1.0f, 400.0f);
    
    m_shadowViewProj = proj * view;
    
    // Transpose for BGFX
    Math::Matrix4 viewT = view.Transposed();
    Math::Matrix4 projT = proj.Transposed();
    
    bgfx::setViewTransform(VIEW_SHADOW, &viewT.M[0][0], &projT.M[0][0]);
    bgfx::touch(VIEW_SHADOW);
}

void PostProcessing::BeginScenePass() {
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
    bgfx::setTexture(0, s_texColor, m_sceneColor);
    
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

} // namespace Solstice::Render
