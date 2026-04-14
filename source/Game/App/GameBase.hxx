#pragma once

#include "../../Solstice.hxx"
#include "../../UI/Core/Window.hxx"
#include <memory>
#include <chrono>
#include <functional>

namespace Solstice::Game {

class SOLSTICE_API GameBase {
public:
    GameBase();
    virtual ~GameBase();

    // Main entry point
    int Run();

    // Override these in derived classes
    virtual void Initialize() {}  // Default empty implementation
    virtual void Shutdown() {}  // Default empty implementation to avoid calling pure virtual from destructor
    virtual void Update(float DeltaTime) {}  // Default empty implementation
    virtual void Render() {}  // Default empty implementation
    virtual void HandleInput() {}

    // Window management
    void SetWindow(std::unique_ptr<UI::Window> Window);
    UI::Window* GetWindow() const { return m_Window.get(); }

    // Frame rate control
    void SetMaxFPS(float MaxFPS) { m_MaxFPS = MaxFPS; }
    float GetMaxFPS() const { return m_MaxFPS; }
    float GetCurrentFPS() const { return m_CurrentFPS; }

    // Game time
    float GetGameTime() const { return m_GameTime; }
    float GetDeltaTime() const { return m_DeltaTime; }

    // Debug
    void SetShowDebugOverlay(bool Enable) { m_ShowDebugOverlay = Enable; }
    bool IsDebugOverlayVisible() const { return m_ShowDebugOverlay; }

    // Control
    void RequestClose() { m_ShouldClose = true; }
    bool ShouldClose() const { return m_ShouldClose; }

protected:
    // Window
    std::unique_ptr<UI::Window> m_Window;

    // Timing
    std::chrono::high_resolution_clock::time_point m_LastFrameTime;
    std::chrono::high_resolution_clock::time_point m_FPSLastTime;
    float m_GameTime{0.0f};
    float m_DeltaTime{0.0f};
    float m_CurrentFPS{0.0f};
    float m_MaxFPS{0.0f}; // 0 = unlimited FPS
    int m_FrameCount{0};

    // State
    bool m_ShouldClose{false};
    bool m_ShowDebugOverlay{false};
};

} // namespace Solstice::Game
