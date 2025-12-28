#include "ParticleSystem.hxx"
#include "ShaderLoader.hxx"
#include <Core/Debug.hxx>
#include <algorithm>
#include <cmath>

namespace Solstice::Render {

ParticleSystem::ParticleSystem()
    : m_MaxParticles(3000)
    , m_ActiveParticles(0)
    , m_SpawnTimer(0.0f)
    , m_SpawnRate(100.0f) // 100 particles per second
    , m_MaxDistance(60.0f)
    , m_Density(1.0f)
    , m_Program(BGFX_INVALID_HANDLE)
    , m_VertexBuffer(BGFX_INVALID_HANDLE)
    , m_Initialized(false)
{
}

ParticleSystem::~ParticleSystem() {
    Shutdown();
}

void ParticleSystem::Initialize(uint32_t maxParticles, const std::string& vertexShader, const std::string& fragmentShader) {
    if (m_Initialized) {
        Shutdown();
    }

    m_MaxParticles = maxParticles;
    m_Particles.clear();
    m_Particles.reserve(m_MaxParticles);
    m_ActiveParticles = 0;

    // Load shaders
    SIMPLE_LOG("ParticleSystem: Loading shaders...");
    bgfx::ShaderHandle vsh = ShaderLoader::LoadShader(vertexShader);
    if (!bgfx::isValid(vsh)) {
        SIMPLE_LOG("ParticleSystem: ERROR - Failed to load vertex shader " + vertexShader);
        m_Program = BGFX_INVALID_HANDLE;
        return;
    }

    bgfx::ShaderHandle fsh = ShaderLoader::LoadShader(fragmentShader);
    if (!bgfx::isValid(fsh)) {
        SIMPLE_LOG("ParticleSystem: ERROR - Failed to load fragment shader " + fragmentShader);
        bgfx::destroy(vsh);
        m_Program = BGFX_INVALID_HANDLE;
        return;
    }

    m_Program = bgfx::createProgram(vsh, fsh, true);
    if (!bgfx::isValid(m_Program)) {
        SIMPLE_LOG("ParticleSystem: ERROR - Failed to create shader program");
        bgfx::destroy(vsh);
        bgfx::destroy(fsh);
        m_Program = BGFX_INVALID_HANDLE;
        return;
    }

    // Vertex buffer will be created lazily on first render
    m_VertexBuffer = BGFX_INVALID_HANDLE;

    m_Initialized = true;
    SIMPLE_LOG("ParticleSystem: Initialized with " + std::to_string(maxParticles) + " max particles");
}

void ParticleSystem::Shutdown() {
    if (bgfx::isValid(m_VertexBuffer)) {
        bgfx::destroy(m_VertexBuffer);
        m_VertexBuffer = BGFX_INVALID_HANDLE;
    }

    if (bgfx::isValid(m_Program)) {
        bgfx::destroy(m_Program);
        m_Program = BGFX_INVALID_HANDLE;
    }

    m_Particles.clear();
    m_ActiveParticles = 0;
    m_Initialized = false;
}

void ParticleSystem::Update(float dt, const Math::Vec3& cameraPos) {
    if (!m_Initialized) {
        return;
    }

    // Spawn new particles
    m_SpawnTimer += dt;
    float spawnInterval = 1.0f / (m_SpawnRate * m_Density);
    while (m_SpawnTimer >= spawnInterval && m_ActiveParticles < m_MaxParticles) {
        SpawnParticle(cameraPos);
        m_SpawnTimer -= spawnInterval;
    }

    // Update existing particles
    uint32_t activeCount = m_ActiveParticles;
    for (uint32_t i = 0; i < activeCount; ) {
        UpdateParticle(m_Particles[i], dt);

        // Remove dead particles or particles too far from camera
        Math::Vec3 toParticle = m_Particles[i].Position - cameraPos;
        float distance = toParticle.Magnitude();

        if (m_Particles[i].Life <= 0.0f || distance > m_MaxDistance) {
            // Swap with last active particle
            if (i < activeCount - 1) {
                m_Particles[i] = m_Particles[activeCount - 1];
            }
            activeCount--;
        } else {
            i++;
        }
    }

    m_ActiveParticles = activeCount;
}

void ParticleSystem::Render(bgfx::ViewId viewId, const Math::Matrix4& viewProj) {
    if (!m_Initialized || m_ActiveParticles == 0) {
        return;
    }

    if (!bgfx::isValid(m_Program)) {
        SIMPLE_LOG("ParticleSystem: ERROR - Shader program not initialized");
        return;
    }

    // Lazy initialization of vertex buffer
    if (!bgfx::isValid(m_VertexBuffer)) {
        bgfx::VertexLayout layout = GetVertexLayout();
        m_VertexBuffer = bgfx::createDynamicVertexBuffer(
            m_MaxParticles * 4, layout, BGFX_BUFFER_ALLOW_RESIZE);
        if (!bgfx::isValid(m_VertexBuffer)) {
            SIMPLE_LOG("ParticleSystem: ERROR - Failed to create vertex buffer");
            return;
        }
    }

    // Build vertex data
    size_t vertexSize = GetVertexSize();
    size_t totalVertices = m_ActiveParticles * 4;
    std::vector<uint8_t> vertexData(totalVertices * vertexSize);
    size_t vertexOffset = 0;

    for (uint32_t i = 0; i < m_ActiveParticles; ++i) {
        BuildVertexData(m_Particles[i], vertexData, vertexOffset);
    }

    // Update vertex buffer
    if (vertexData.size() > 0) {
        const bgfx::Memory* mem = bgfx::copy(vertexData.data(), static_cast<uint32_t>(vertexData.size()));
        bgfx::update(m_VertexBuffer, 0, mem);

        // Set render state for alpha blending
        uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A
            | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_DEPTH_TEST_LESS;

        bgfx::setVertexBuffer(0, m_VertexBuffer, 0, static_cast<uint32_t>(totalVertices));
        bgfx::setState(state);
        bgfx::submit(viewId, m_Program);
    }
}

} // namespace Solstice::Render
