#include <Render/FramebufferManager.hxx>
#include <Core/Debug.hxx>

namespace Solstice::Render {

FramebufferManager::FramebufferManager() = default;

FramebufferManager::~FramebufferManager() {
    Shutdown();
}

void FramebufferManager::Initialize(uint32_t width, uint32_t height, uint32_t shadowMapSize) {
    m_width = width;
    m_height = height;
    m_shadowMapSize = shadowMapSize;

    CreateShadowFramebuffer();
    CreateSceneFramebuffer();
}

void FramebufferManager::Shutdown() {
    DestroyShadowFramebuffer();
    DestroySceneFramebuffer();
}

void FramebufferManager::Resize(uint32_t width, uint32_t height) {
    if (m_width == width && m_height == height) return;

    m_width = width;
    m_height = height;

    // Recreate scene framebuffer (shadow map size doesn't change)
    DestroySceneFramebuffer();
    CreateSceneFramebuffer();
}

void FramebufferManager::CreateShadowFramebuffer() {
    uint64_t shadowFlags = BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
    m_shadowMap = bgfx::createTexture2D(
        m_shadowMapSize,
        m_shadowMapSize,
        false,
        1,
        bgfx::TextureFormat::D32,
        shadowFlags
    );
    m_shadowFB = bgfx::createFrameBuffer(1, &m_shadowMap, true);

    if (!bgfx::isValid(m_shadowFB)) {
        SIMPLE_LOG("FramebufferManager: Failed to create shadow framebuffer");
    }
}

void FramebufferManager::CreateSceneFramebuffer() {
    uint64_t sceneFlags = BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;

    m_sceneColor = bgfx::createTexture2D(
        m_width,
        m_height,
        false,
        1,
        bgfx::TextureFormat::RGBA16F,
        sceneFlags
    );

    m_sceneDepth = bgfx::createTexture2D(
        m_width,
        m_height,
        false,
        1,
        bgfx::TextureFormat::D24S8,
        sceneFlags
    );

    bgfx::TextureHandle sceneTexs[] = { m_sceneColor, m_sceneDepth };
    m_sceneFB = bgfx::createFrameBuffer(2, sceneTexs, true);

    if (!bgfx::isValid(m_sceneFB)) {
        SIMPLE_LOG("FramebufferManager: CRITICAL - Failed to create scene framebuffer");
    } else {
        SIMPLE_LOG("FramebufferManager: Created scene framebuffer " +
                   std::to_string(m_width) + "x" + std::to_string(m_height));
    }
}

void FramebufferManager::DestroyShadowFramebuffer() {
    if (bgfx::isValid(m_shadowFB)) {
        bgfx::destroy(m_shadowFB);
        m_shadowFB = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_shadowMap)) {
        bgfx::destroy(m_shadowMap);
        m_shadowMap = BGFX_INVALID_HANDLE;
    }
}

void FramebufferManager::DestroySceneFramebuffer() {
    if (bgfx::isValid(m_sceneFB)) {
        bgfx::destroy(m_sceneFB);
        m_sceneFB = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_sceneColor)) {
        bgfx::destroy(m_sceneColor);
        m_sceneColor = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_sceneDepth)) {
        bgfx::destroy(m_sceneDepth);
        m_sceneDepth = BGFX_INVALID_HANDLE;
    }
}

} // namespace Solstice::Render
