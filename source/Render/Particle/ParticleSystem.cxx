#include "ParticleSystem.hxx"
#include <Render/Assets/ShaderLoader.hxx>
#include <Core/Debug.hxx>
#include <Core/ScopeTimer.hxx>
#include <Core/Profiler.hxx>
#include <algorithm>
#include <cmath>

namespace Solstice::Render {

bool ParticleSystem::s_PerfScalingEnabled = true;
float ParticleSystem::s_PerfScalingTargetFps = 60.0f;
float ParticleSystem::s_PerfScalingMinScale = 0.35f;
int ParticleSystem::s_PerfScalingWindow = 60;

ParticleSystem::ParticleSystem()
    : m_MaxParticles(3000)
    , m_ActiveParticles(0)
    , m_SpawnTimer(0.0f)
    , m_SpawnRate(100.0f) // 100 particles per second
    , m_MaxDistance(60.0f)
    , m_Density(1.0f)
    , m_SpawnBudgetPerFrame(256)
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
    m_SpawnTimer = 0.0f;

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

void ParticleSystem::SetPerfScalingEnabled(bool enabled) {
    s_PerfScalingEnabled = enabled;
}

void ParticleSystem::SetPerfScalingTargetFps(float fps) {
    s_PerfScalingTargetFps = std::max(1.0f, fps);
}

void ParticleSystem::SetPerfScalingMinScale(float scale) {
    s_PerfScalingMinScale = std::clamp(scale, 0.05f, 1.0f);
}

void ParticleSystem::SetPerfScalingWindow(int frames) {
    s_PerfScalingWindow = std::max(1, frames);
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
    PROFILE_SCOPE("Particles.Update");
    if (!m_Initialized) {
        return;
    }

    float qualityScale = 1.0f;
    if (s_PerfScalingEnabled && s_PerfScalingTargetFps > 0.0f) {
        auto stats = Core::Profiler::Instance().GetAverageFrameStats(s_PerfScalingWindow);
        if (stats.FPS > 1.0f) {
            qualityScale = std::clamp(stats.FPS / s_PerfScalingTargetFps, s_PerfScalingMinScale, 1.0f);
        }
    }

    // Spawn new particles
    m_SpawnTimer += dt;
    float effectiveDensity = m_Density * qualityScale;
    float spawnRate = m_SpawnRate * effectiveDensity;
    float spawnInterval = spawnRate > 0.0f ? (1.0f / spawnRate) : 0.0f;
    if (spawnInterval > 0.0f) {
        uint32_t desiredSpawns = static_cast<uint32_t>(m_SpawnTimer / spawnInterval);
        uint32_t budget = m_SpawnBudgetPerFrame == 0 ? desiredSpawns : m_SpawnBudgetPerFrame;
        if (m_SpawnBudgetPerFrame != 0) {
            const float targetDt = 1.0f / 60.0f;
            if (dt > targetDt) {
                float scale = std::clamp(targetDt / dt, 0.25f, 1.0f);
                budget = std::max(1u, static_cast<uint32_t>(budget * scale));
            }
        }
        if (budget > 0) {
            budget = std::max(1u, static_cast<uint32_t>(budget * qualityScale));
        }
        budget = std::min(budget, desiredSpawns);
        uint32_t available = m_MaxParticles - m_ActiveParticles;
        uint32_t spawnCount = std::min(budget, available);
        for (uint32_t i = 0; i < spawnCount; ++i) {
            SpawnParticle(cameraPos);
        }
        if (spawnCount > 0) {
            m_SpawnTimer -= spawnInterval * static_cast<float>(spawnCount);
        }
        Core::Profiler::Instance().SetCounter("Particles.SpawnBudget", budget);
        Core::Profiler::Instance().SetCounter("Particles.Spawned", spawnCount);
    } else {
        Core::Profiler::Instance().SetCounter("Particles.Spawned", 0);
    }
    Core::Profiler::Instance().SetCounter("Particles.QualityScale", static_cast<int64_t>(qualityScale * 100.0f));

    // Update existing particles
    uint32_t activeCount = m_ActiveParticles;
    uint32_t processed = 0;
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
        processed++;
    }

    m_ActiveParticles = activeCount;
    Core::Profiler::Instance().SetCounter("Particles.Active", m_ActiveParticles);
    Core::Profiler::Instance().SetCounter("Particles.Updated", processed);
}

void ParticleSystem::Render(bgfx::ViewId viewId, const Math::Matrix4& viewProj, const Math::Vec3& cameraRight, const Math::Vec3& cameraUp) {
    PROFILE_SCOPE("Particles.Render");
    if (!m_Initialized || m_ActiveParticles == 0) {
        Core::Profiler::Instance().SetCounter("Particles.Rendered", 0);
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
            m_MaxParticles * 6, layout, BGFX_BUFFER_ALLOW_RESIZE);
        if (!bgfx::isValid(m_VertexBuffer)) {
            SIMPLE_LOG("ParticleSystem: ERROR - Failed to create vertex buffer");
            return;
        }
    }

    // Build vertex data
    size_t vertexSize = GetVertexSize();
    size_t totalVertices = m_ActiveParticles * 6;
    m_VertexScratch.resize(totalVertices * vertexSize);
    size_t vertexOffset = 0;

    Math::Vec3 billboardRight = cameraRight.Normalized();
    Math::Vec3 billboardUp = cameraUp.Normalized();
    Math::Vec3 billboardForward = billboardRight.Cross(billboardUp);
    if (billboardForward.Magnitude() > 0.0f) {
        billboardForward = billboardForward.Normalized();
        billboardUp = billboardForward.Cross(billboardRight).Normalized();
    }

    for (uint32_t i = 0; i < m_ActiveParticles; ++i) {
        BuildVertexData(m_Particles[i], m_VertexScratch, vertexOffset, billboardRight, billboardUp);
    }

    // Update vertex buffer
    if (!m_VertexScratch.empty()) {
        const bgfx::Memory* mem = bgfx::copy(m_VertexScratch.data(), static_cast<uint32_t>(m_VertexScratch.size()));
        bgfx::update(m_VertexBuffer, 0, mem);

        // Set render state for alpha blending
        uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A
            | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_DEPTH_TEST_LESS;

        bgfx::setVertexBuffer(0, m_VertexBuffer, 0, static_cast<uint32_t>(totalVertices));
        bgfx::setState(state);
        bgfx::submit(viewId, m_Program);
    }
    Core::Profiler::Instance().SetCounter("Particles.Rendered", m_ActiveParticles);
    Core::Profiler::Instance().SetCounter("Particles.Vertices", static_cast<int64_t>(totalVertices));
    Core::Profiler::Instance().SetCounter("Particles.BufferBytes", static_cast<int64_t>(m_VertexScratch.size()));
}

} // namespace Solstice::Render
