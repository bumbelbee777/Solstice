#pragma once

#include <Game/FPS/FPSGame.hxx>
#include <Game/ScriptManager.hxx>
#include <Game/ProceduralTextureManager.hxx>
#include <Game/SceneBuilder.hxx>
#include <Game/MaterialPresetManager.hxx>
#include <Game/LevelSelector.hxx>
#include <Game/PauseMenu.hxx>
#include <Game/GamePreferences.hxx>
#include "Archon.hxx"
#include <Arzachel/Seed.hxx>
#include <Arzachel/ProceduralTexture.hxx>
#include <UI/Window.hxx>
#include <Render/SoftwareRenderer.hxx>
#include <Render/Scene/Scene.hxx>
#include <Render/Assets/Mesh.hxx>
#include <Core/Material.hxx>
#include <Core/ResourceHandle.hxx>
#include <Physics/PhysicsSystem.hxx>
#include <Math/Vector.hxx>
#include <Core/AssetStream.hxx>
#include <Core/Audio.hxx>
#include <Core/Debug.hxx>
#include <Core/Async.hxx>
#include <string>
#include <memory>
#include <vector>
#include <chrono>
#include <cstdint>

// Forward declarations
namespace Solstice::Render {
    class MeshLibrary;
    class MaterialLibrary;
    class Skybox;
}

namespace Solstice::Physics {
    struct LightSource;
}

namespace Solstice::ECS {
    class Registry;
}

namespace Solstice::Hyperbourne {

class HyperbourneGame : public Game::FPSGame {
public:
    HyperbourneGame();
    ~HyperbourneGame();

protected:
    // Override GameBase/FPSGame virtual methods
    void Initialize() override;
    void Shutdown() override;
    void Update(float DeltaTime) override;
    void Render() override;
    void HandleInput() override;

    // FPSGame overrides
    void InitializeFPSSystems() override;
    void InitializePlayer() override;
    void InitializeWeapons() override;

    // Override UpdateFPSMovement
    void UpdateFPSMovement(float DeltaTime) override;

private:
    // Window management
    void InitializeWindow();
    void HandleWindowResize(int w, int h);
    void HandleKeyInput(int key, int scancode, int action, int mods);
    void HandleMouseButton(int button, int action, int mods);
    void HandleCursorPos(double dx, double dy);

    // Scene initialization
    void InitializeScene();
    void InitializeAudio();
    void InitializeSkybox();
    void InitializeProceduralTextures();
    void InitializeCurrentLevel();
    void LoadLevel(const std::string& LevelName);

    // Level initialization
    void InitializeEndlessMode();

    // Archon mesh generation helpers
    uint32_t CreateCruciformMesh();
    uint32_t CreateShardhiveMesh();
    void SpawnArchonWithVisual(ECS::EntityId Entity, const Math::Vec3& Position, Hyperbourne::ArchonType Type);

    // Weapon system helpers
    void CreateWeaponMeshes(); // Create weapon meshes when MeshLibrary is ready
    void SpawnBullet(ECS::EntityId ShooterEntity, const Math::Vec3& Direction, const Game::Weapon& Weapon);
    void UpdateBullets(float DeltaTime);

    void FinalizeProceduralTextures();
    void ProcessPendingTextures(); // Process texture creation in Render() (frame context)

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

    // Scripting & Generation
    Game::ScriptManager m_ScriptManager;
    Game::ProceduralTextureManager m_TextureManager;
    Game::SceneBuilder m_SceneBuilder;
    Arzachel::Seed m_Seed{12345};

    // Level system
    std::unique_ptr<Game::LevelSelector> m_LevelSelector;
    std::string m_CurrentLevelName;
    bool m_LevelLoaded{false};

    // Menus
    std::unique_ptr<Game::PauseMenu> m_PauseMenu;
    std::unique_ptr<Game::GamePreferences> m_GamePreferences;
    bool m_ShowSettingsMenu{false};

    // Camera (additional properties if needed, base provided by FPSGame)
    bool m_MouseLocked{false};
    const float m_CameraSpeed{5.0f};
    const float m_CameraHeight{1.75f};

    // Physics timestep
    float m_PhysicsAccumulator{0.0f};
    const float m_PhysicsStep{1.0f / 60.0f};
    const int m_MaxSubSteps{5};

    // Material handles (using ResourceHandle for type safety)
    Core::MaterialHandle m_ConcreteMaterialHandle{Core::MaterialHandle::Invalid()};
    Core::MaterialHandle m_MetalMaterialHandle{Core::MaterialHandle::Invalid()};
    Core::MaterialHandle m_PlasticMaterialHandle{Core::MaterialHandle::Invalid()};
    Core::MaterialHandle m_GlassMaterialHandle{Core::MaterialHandle::Invalid()};
    Core::MaterialHandle m_RubberMaterialHandle{Core::MaterialHandle::Invalid()};

    // Systems
    Game::MaterialPresetManager m_MaterialPresetManager;

    // Lighting
    std::vector<Physics::LightSource> m_Lights;

    // Skybox
    std::unique_ptr<Render::Skybox> m_Skybox;

    // Scene state
    bool m_SceneInitialized{false};
    bool m_AudioStarted{false};
    bool m_FirstFrameRendered{false};
    bool m_TexturesFinalized{false};

    // Asset streaming
    Core::AssetStreamer m_AssetStreamer;
    Core::AssetHandle m_MenuMusicHandle;
    Core::AssetHandle m_LevelMusicHandle;

    // Mouse position
    float m_MouseX{0.0f};
    float m_MouseY{0.0f};

    // Music state tracking
    bool m_MenuMusicPlaying{false};
    bool m_LevelMusicPlaying{false};

    // Weapon system
    uint32_t m_AK47MeshID{0};
    uint32_t m_KnifeMeshID{0};
    uint32_t m_BulletMeshID{0};
    std::vector<ECS::EntityId> m_ActiveProjectiles; // Track active bullet entities
};

} // namespace Solstice::Hyperbourne

