#include "App/GameBase.hxx"
#include "Core/Debug/Debug.hxx"
#include "Core/Profiling/Profiler.hxx"
#include "Networking/NetworkingSystem.hxx"
#include <Physics/Integration/PhysicsSystem.hxx>
#include <Plugin/SubsystemHooks.hxx>
#include <algorithm>
#include <iostream>
#include <cstdio>

namespace Solstice::Game {

GameBase::GameBase() {
    m_LastFrameTime = std::chrono::high_resolution_clock::now();
    m_FPSLastTime = m_LastFrameTime;
}

GameBase::~GameBase() {
    Shutdown();
}

int GameBase::Run() {
    try {
        Initialize();

        std::string baseTitle = m_Window ? m_Window->GetTitle() : "Solstice";

        while (!m_ShouldClose && (!m_Window || !m_Window->ShouldClose())) {
            // Poll events
            if (m_Window) {
                m_Window->PollEvents();
            }

            if (Solstice::Networking::NetworkingSystem::Instance().IsRunning()) {
                Solstice::Networking::NetworkingSystem::Instance().Poll();
            }

            // Handle input
            HandleInput();

            Solstice::Plugin::SubsystemHooks::Instance().Invoke(
                Solstice::Plugin::SubsystemHookKind::ProfilingPreFrame, m_DeltaTime);
            Core::Profiler::Instance().BeginFrame();

            // Calculate delta time
            auto now = std::chrono::high_resolution_clock::now();
            float dt = std::chrono::duration<float>(now - m_LastFrameTime).count();

            // Cap delta time to prevent large jumps and physics explosions
            if (m_MaxFPS > 0.0f) {
                float maxDelta = 1.0f / m_MaxFPS;
                dt = std::min(dt, maxDelta);
            } else {
                // Only cap to prevent physics explosions, not to limit FPS
                dt = std::min(dt, 0.1f); // Cap at 0.1s to prevent physics issues
            }

            m_LastFrameTime = now;
            m_GameTime += dt;
            m_DeltaTime = dt;

            // Default: render uses full physics pose. Fixed-timestep games set this in Update (see SetSceneRenderBlendForSync).
            Solstice::Physics::PhysicsSystem::Instance().SetSceneRenderBlendForSync(1.0f);

            Solstice::Plugin::SubsystemHooks::Instance().Invoke(Solstice::Plugin::SubsystemHookKind::GamePreUpdate, dt);
            Update(dt);
            Solstice::Plugin::SubsystemHooks::Instance().Invoke(Solstice::Plugin::SubsystemHookKind::GamePostUpdate, dt);

            Solstice::Plugin::SubsystemHooks::Instance().Invoke(Solstice::Plugin::SubsystemHookKind::GamePreRender, dt);
            Render();
            Solstice::Plugin::SubsystemHooks::Instance().Invoke(Solstice::Plugin::SubsystemHookKind::GamePostRender, dt);

            // Render profiler overlay if debug overlay is enabled
            if (m_ShowDebugOverlay) {
                Core::Profiler::Instance().RenderOverlay();
            }

            Core::Profiler::Instance().EndFrame();
            Solstice::Plugin::SubsystemHooks::Instance().Invoke(
                Solstice::Plugin::SubsystemHookKind::ProfilingPostFrame, m_DeltaTime);

            // Update FPS counter and window title
            m_FrameCount++;
            float fpsTime = std::chrono::duration<float>(now - m_FPSLastTime).count();
            if (fpsTime >= 1.0f) {
                m_CurrentFPS = static_cast<float>(m_FrameCount) / fpsTime;
                m_FrameCount = 0;
                m_FPSLastTime = now;

                // Update window title with FPS
                if (m_Window) {
                    char titleBuffer[256];
                    snprintf(titleBuffer, sizeof(titleBuffer), "%s - %.1f FPS", baseTitle.c_str(), m_CurrentFPS);
                    m_Window->SetTitle(titleBuffer);
                }
            }
        }

        return 0;
    } catch (const std::exception& e) {
        SIMPLE_LOG("GameBase: EXCEPTION CAUGHT: " + std::string(e.what()));
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }
}

void GameBase::SetWindow(std::unique_ptr<UI::Window> Window) {
    m_Window = std::move(Window);
}

} // namespace Solstice::Game
