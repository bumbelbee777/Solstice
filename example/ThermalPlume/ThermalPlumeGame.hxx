#pragma once

#include <Game/App/GameBase.hxx>
#include <UI/Core/Window.hxx>
#include <Render/SoftwareRenderer.hxx>
#include <Render/FluidVolumeVisualizer.hxx>
#include <Render/Scene/Scene.hxx>
#include <Render/Scene/Camera.hxx>
#include <Render/Assets/Mesh.hxx>
#include <Material/Material.hxx>
#include <Physics/Integration/PhysicsSystem.hxx>
#include <Physics/Fluid/Fluid.hxx>
#include <Entity/Registry.hxx>
#include <Arzachel/Seed.hxx>
#include <Math/Vector.hxx>
#include <Math/Quaternion.hxx>
#include "ExhaustParticleSystem.hxx"
#include <memory>
#include <chrono>
#include <vector>
#include <random>

namespace Solstice::Render {
    class MeshLibrary;
    class Skybox;
}

namespace Solstice::Core {
    class MaterialLibrary;
}

namespace Solstice::Physics {
    struct LightSource;
}

namespace Solstice::ThermalPlume {

enum class SimState : uint8_t { Idle, Running, Paused };
enum class CameraMode : uint8_t { FreeFly, SideOrtho, PlumeChase };

class ThermalPlumeGame : public Game::GameBase {
public:
    ThermalPlumeGame();
    ~ThermalPlumeGame();

protected:
    void Initialize() override;
    void Shutdown() override;
    void Update(float DeltaTime) override;
    void Render() override;
    void HandleInput() override;

private:
    void InitializeWindow();
    void InitializeScene();
    void InitializeFluidSim();
    void InitializeParticles();
    void InitializeLighting();

    void HandleWindowResize(int W, int H);
    void HandleKeyInput(int Key, int Scancode, int Action, int Mods);
    void HandleMouseButton(int Button, int Action, int Mods);
    void HandleCursorPos(double Dx, double Dy);

    void InjectNozzle(float Dt);
    void UpdateCamera(float Dt);
    void UpdateEmissiveMaterial();
    void ResetSimulation();

    void RenderUI();
    void RenderSimControls();
    void RenderEngineControls();
    void RenderEnvironmentControls();
    void RenderVisualizationControls();
    void RenderStatsPanel();

    // Window
    bool m_IsFullscreen{false};
    bool m_PendingResize{false};
    int m_PendingWidth{1280};
    int m_PendingHeight{720};
    std::chrono::high_resolution_clock::time_point m_LastResizeEvent;

    // Renderer
    std::unique_ptr<Render::SoftwareRenderer> m_Renderer;

    // Scene
    Render::Scene m_Scene;
    std::unique_ptr<Render::MeshLibrary> m_MeshLibrary;
    std::unique_ptr<Core::MaterialLibrary> m_MaterialLibrary;
    ECS::Registry m_Registry;
    Arzachel::Seed m_Seed{42424};

    // Camera
    Render::Camera m_Camera;
    CameraMode m_CameraMode{CameraMode::FreeFly};
    bool m_MouseLocked{false};
    float m_CameraSpeed{4.0f};
    float m_OrbitAngle{0.0f};

    // Fluid simulation
    std::unique_ptr<Physics::FluidSimulation> m_Fluid;
    Render::FluidVolumeVisualizer m_VolumeViz;

    // Particles
    std::unique_ptr<ExhaustParticleSystem> m_ExhaustParticles;

    // Lighting
    std::vector<Physics::LightSource> m_Lights;

    // Simulation state machine
    SimState m_SimState{SimState::Idle};
    bool m_Firing{false};

    // Engine parameters (UI-driven)
    float m_Throttle{0.5f};
    float m_ChamberTemperature{2500.0f};
    float m_ExhaustVelocity{200.0f};
    float m_NozzleDiameter{0.3f};
    float m_ExpansionRatio{4.0f};
    float m_FuelFlowRate{0.5f};

    // Environment parameters
    float m_Gravity{9.81f};
    float m_AmbientTemperature{300.0f};
    float m_TurbulenceIntensity{0.5f};

    // Visualization parameters
    int m_DrawMode{0};
    float m_IsoLevel{0.45f};
    int m_VelStride{2};
    float m_VelScale{0.25f};
    bool m_ShowParticles{true};
    bool m_ShowVolume{true};
    bool m_ShowWalls{false};

    // Scene object IDs for dynamic material updates
    Render::SceneObjectID m_NozzleBellID{Render::InvalidObjectID};
    Render::SceneObjectID m_ChamberID{Render::InvalidObjectID};
    Render::SceneObjectID m_WallLeftID{Render::InvalidObjectID};
    Render::SceneObjectID m_WallRightID{Render::InvalidObjectID};
    uint32_t m_NozzleEmissiveMatID{0};

    // Grid parameters
    static constexpr int kGridNx = 32;
    static constexpr int kGridNy = 48;
    static constexpr int kGridNz = 32;
    static constexpr float kCellSize = 0.05f;

    // Nozzle geometry constants
    static constexpr float kNozzleExitY = 2.8f;
    static constexpr float kNozzleRadius = 0.15f;
    static constexpr float kStandHeight = 3.2f;

    // Physics timestep accumulator
    float m_PhysicsAccumulator{0.0f};
    static constexpr float kPhysicsStep = 1.0f / 60.0f;
    static constexpr int kMaxSubSteps = 4;

    // RNG for turbulence perturbation
    std::mt19937 m_Rng{42u};
    std::uniform_real_distribution<float> m_NoiseDist{-1.0f, 1.0f};
};

} // namespace Solstice::ThermalPlume
