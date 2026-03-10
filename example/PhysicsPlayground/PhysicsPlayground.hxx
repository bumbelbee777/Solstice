#pragma once

#include <UI/Window.hxx>
#include <Render/SoftwareRenderer.hxx>
#include <Render/Scene/Scene.hxx>
#include <Render/Scene/Camera.hxx>
#include <Render/Assets/Mesh.hxx>
#include <Core/Material.hxx>
#include <Physics/PhysicsSystem.hxx>
#include <Entity/Registry.hxx>
#include <Math/Vector.hxx>
#include <Math/Quaternion.hxx>
#include <Core/Debug.hxx>
#include "ObjectSpawner.hxx"
#include <Game/GameBase.hxx>
#include <Game/InputManager.hxx>
#include <Game/ScriptManager.hxx>
#include <Render/Particle/ParticleSystem.hxx>
#include <Render/Particle/ParticlePresets.hxx>
#include <Physics/Fluid.hxx>
#include <memory>
#include <vector>
#include <chrono>
#include <map>

// Forward declarations
namespace Solstice::PhysicsPlayground {
    class SelectionSystem;
    class UIManager;
    class TerminalHub;
}

namespace Solstice::Render {
    class MeshLibrary;
    class Skybox;
    class ParticleSystem;
}

namespace Solstice::Core {
    class MaterialLibrary;
}

namespace Solstice::Physics {
    class RigidBody;
    struct LightSource;
}

namespace Solstice::ECS {
    class Registry;
}

namespace Solstice::UI {
    class UIManager;
}

namespace Solstice::PhysicsPlayground {

// Forward declarations
class ObjectSpawner;
class SelectionSystem;
class UIManager;

class PhysicsPlayground : public Game::GameBase {
public:
    PhysicsPlayground();
    ~PhysicsPlayground();

protected:
    // Override GameBase virtual methods
    void Initialize() override;
    void Shutdown() override;
    void Update(float DeltaTime) override;
    void Render() override;
    void HandleInput() override;

private:

    // Window management
    void InitializeWindow();
    void HandleWindowResize(int w, int h);
    void HandleKeyInput(int key, int scancode, int action, int mods);
    void HandleMouseButton(int button, int action, int mods);
    void HandleCursorPos(double dx, double dy);

    // Physics object management
    void CreateInitialObjects();
    void UpdateGrabbedObject(float deltaTime);
    void SyncPhysicsToScene();

    // Material initialization
    void InitializeMaterialPresets();

    // Skybox and texture initialization
    void InitializeSkybox();
    void InitializeProceduralTextures();

    // Window state
    bool m_IsFullscreen{true};
    bool m_IsMaximized{false};
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

    // Physics
    ECS::Registry m_Registry;
    std::vector<PhysicsObject> m_DynamicObjects; // PhysicsObject defined in ObjectSpawner.hxx

    // Camera
    Render::Camera m_Camera;
    bool m_MouseLocked{false};
    const float m_CameraSpeed{5.0f};

    // Interaction
    ECS::EntityId m_GrabbedEntity{0};
    float m_GrabbedDistance{3.0f};
    std::map<ECS::EntityId, Math::Vec3> m_GrabbedOffsets; // Offsets relative to grabbed center

    // Timing
    std::chrono::high_resolution_clock::time_point m_LastFrameTime;
    std::chrono::high_resolution_clock::time_point m_FPSLastTime;
    int m_FrameCount{0};
    float m_CurrentFPS{0.0f};

    // Physics timestep
    float m_PhysicsAccumulator{0.0f};
    const float m_PhysicsStep{1.0f / 60.0f};
    const int m_MaxSubSteps{5};

    // Material IDs
    uint32_t m_GrayMaterialID{0};
    uint32_t m_GroundMaterialID{0};

    // Material presets
    struct MaterialPresets {
        // Metallic materials
        uint32_t ChromeMat{0};
        uint32_t GoldMat{0};
        uint32_t SilverMat{0};
        uint32_t CopperMat{0};

        // Glass materials
        uint32_t ClearGlassMat{0};
        uint32_t TintedGlassMat{0};
        uint32_t FrostedGlassMat{0};

        // Textured materials (will be assigned procedural textures)
        uint32_t CheckerboardMat{0};
        uint32_t NoiseMat{0};
        uint32_t StripesMat{0};

        // Standard materials
        uint32_t RedMat{0};
        uint32_t GreenMat{0};
        uint32_t BlueMat{0};
    };
    MaterialPresets m_MaterialPresets;

    // Systems
    std::unique_ptr<ObjectSpawner> m_ObjectSpawner;
    std::unique_ptr<SelectionSystem> m_SelectionSystem;
    std::unique_ptr<UIManager> m_UIManager;
    std::unique_ptr<Game::InputManager> m_InputManager;
    std::unique_ptr<Game::ScriptManager> m_ScriptManager;
    std::unique_ptr<TerminalHub> m_TerminalHub;

    // Mouse position for picking
    float m_MouseX{0.0f};
    float m_MouseY{0.0f};

    // Helper to spawn object from UI
    void SpawnObjectFromUI(ObjectType type);

    // Helper to spawn fluid container
    void SpawnFluidContainer(const Math::Vec3& position);

    // Lighting
    std::vector<Physics::LightSource> m_Lights;

    // Skybox
    std::unique_ptr<Render::Skybox> m_Skybox;

    // Procedural textures
    std::vector<bgfx::TextureHandle> m_ProceduralTextures;

    // Particle systems
    std::vector<std::unique_ptr<Render::ParticleSystem>> m_ParticleSystems;

    // Fluid simulations
    std::vector<std::unique_ptr<Physics::FluidSimulation>> m_FluidSimulations;
    std::vector<ECS::EntityId> m_FluidContainers;
};

} // namespace Solstice::PhysicsPlayground
