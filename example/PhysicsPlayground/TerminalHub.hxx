#pragma once

#include <UI/Viewport/ViewportUI.hxx>
#include <Render/Scene/Camera.hxx>
#include <Render/Scene/Skybox.hxx>
#include <Render/Particle/ParticleSystem.hxx>
#include <Render/Particle/ParticlePresets.hxx>
#include <Game/App/InputManager.hxx>
#include <Physics/Integration/PhysicsSystem.hxx>
#include <Entity/Registry.hxx>
#include <Math/Vector.hxx>
#include <bgfx/bgfx.h>
#include <memory>
#include <vector>
#include <string>
#include <functional>

namespace Solstice::Render {
    class Scene;
    class MeshLibrary;
}

namespace Solstice::Core {
    class MaterialLibrary;
}

namespace Solstice::Physics {
    class FluidSimulation;
}

namespace Solstice::Game {
    class InputManager;
}

namespace Solstice::PhysicsPlayground {

class ObjectSpawner;

class TerminalHub {
public:
    TerminalHub(
        Render::Scene& scene,
        Render::MeshLibrary& meshLibrary,
        Core::MaterialLibrary& materialLibrary,
        ECS::Registry& registry,
        Render::Skybox* skybox,
        Render::Camera& camera
    );
    ~TerminalHub();

    // Update terminal hub (check interactions, etc.)
    void Update(float deltaTime, const Render::Camera& camera, Solstice::Game::InputManager& inputManager);

    // Render all terminals
    // sceneProgram, viewId, and sceneFramebuffer are optional - if provided, renders as 3D billboards in world space
    void Render(const Render::Camera& camera, int screenWidth, int screenHeight,
                bgfx::ProgramHandle sceneProgram = BGFX_INVALID_HANDLE,
                bgfx::ViewId viewId = 2,
                bgfx::FrameBufferHandle sceneFramebuffer = BGFX_INVALID_HANDLE);

    // Callbacks for terminal actions
    using ParticleSpawnCallback = std::function<void(Render::ParticlePresetType)>;
    using FluidSpawnCallback = std::function<void(const Math::Vec3&)>;
    using ScriptExecuteCallback = std::function<bool(const std::string&, std::string&)>;

    void SetParticleSpawnCallback(ParticleSpawnCallback callback) { m_ParticleSpawnCallback = callback; }
    void SetFluidSpawnCallback(FluidSpawnCallback callback) { m_FluidSpawnCallback = callback; }
    void SetScriptExecuteCallback(ScriptExecuteCallback callback) { m_ScriptExecuteCallback = callback; }

    // Get terminal positions for interaction checking
    std::vector<Math::Vec3> GetTerminalPositions() const;

private:
    // Terminal types
    enum class TerminalType {
        TimeOfDay,
        Weather,
        Fluid,
        Script,
        Environment
    };

    // Terminal data
    struct Terminal {
        TerminalType Type;
        Math::Vec3 Position;
        std::unique_ptr<UI::ViewportUI::WorldSpaceDialog> Dialog;
        bool IsActive{false};

        Terminal(TerminalType type, const Math::Vec3& pos, float width, float height)
            : Type(type), Position(pos), Dialog(std::make_unique<UI::ViewportUI::WorldSpaceDialog>(pos, width, height)), IsActive(false) {}

        // Delete default constructor since Dialog can't be default-constructed
        Terminal() = delete;
        Terminal(const Terminal&) = delete;
        Terminal& operator=(const Terminal&) = delete;
        Terminal(Terminal&&) = default;
        Terminal& operator=(Terminal&&) = default;
    };

    // Render individual terminals
    void RenderTimeOfDayTerminal(Terminal& terminal, const Render::Camera& camera, int screenWidth, int screenHeight);
    void RenderWeatherTerminal(Terminal& terminal, const Render::Camera& camera, int screenWidth, int screenHeight);
    void RenderFluidTerminal(Terminal& terminal, const Render::Camera& camera, int screenWidth, int screenHeight);
    void RenderScriptTerminal(Terminal& terminal, const Render::Camera& camera, int screenWidth, int screenHeight);
    void RenderEnvironmentTerminal(Terminal& terminal, const Render::Camera& camera, int screenWidth, int screenHeight);

    // Check if player is near a terminal
    bool IsNearTerminal(const Math::Vec3& playerPos, const Math::Vec3& terminalPos, float distance = 3.0f) const;

    Render::Scene& m_Scene;
    Render::MeshLibrary& m_MeshLibrary;
    Core::MaterialLibrary& m_MaterialLibrary;
    ECS::Registry& m_Registry;
    Render::Skybox* m_Skybox;
    Render::Camera& m_Camera;

    std::vector<Terminal> m_Terminals;
    int m_ActiveTerminal{-1};
    bool m_Interacting{false};

    // Terminal state
    float m_TimeOfDay{12.0f};
    std::string m_ScriptInput;
    std::string m_ScriptError;
    int m_CurrentSkyPreset{1}; // 0=Dawn, 1=Noon, 2=Dusk, 3=Night, 4=Overcast, 5=Clear

    // Callbacks
    ParticleSpawnCallback m_ParticleSpawnCallback;
    FluidSpawnCallback m_FluidSpawnCallback;
    ScriptExecuteCallback m_ScriptExecuteCallback;
};

} // namespace Solstice::PhysicsPlayground

