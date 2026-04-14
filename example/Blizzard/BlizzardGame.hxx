#pragma once

#include <Game/FPS/FPSGame.hxx>
#include <Game/Integration/ScriptManager.hxx>
#include <Game/World/ProceduralTextureManager.hxx>
#include <Game/World/SceneBuilder.hxx>
#include <Game/World/MaterialPresetManager.hxx>
#include <Game/UI/HUDPresets.hxx>
#include <Game/FPS/SnowSystem.hxx>
#include <Render/Particle/ParticlePresets.hxx>
#include <Scripting/VM/BytecodeVM.hxx>
#include <Arzachel/Seed.hxx>
#include <Arzachel/ProceduralTexture.hxx>
#include <UI/Core/Window.hxx>
#include <Render/SoftwareRenderer.hxx>
#include <Render/Scene/Scene.hxx>
#include <Render/Assets/Mesh.hxx>
#include <Render/Particle/ParticleSystem.hxx>
#include <Material/Material.hxx>
#include <Asset/IO/ResourceHandle.hxx>
#include <Physics/Integration/PhysicsSystem.hxx>
#include <Math/Vector.hxx>
#include <Asset/Streaming/AssetStream.hxx>
#include <Core/Audio/Audio.hxx>
#include <Core/Debug/Debug.hxx>
#include <Core/System/Async.hxx>
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

namespace Solstice::Blizzard {

class BlizzardGame : public Game::FPSGame {
public:
    BlizzardGame();
    ~BlizzardGame();

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

    // Override UpdateFPSMovement to pass snow system
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
    void InitializeTerrain();
    void InitializeHills(); // Deprecated - kept for compatibility
    void InitializeStructures();
    void InitializeLODMountains();
    void InitializeSwissTown();
    void InitializeVegetation();
    void InitializeGroundVegetation();


    // Footstep effects
    void UpdateFootsteps(float dt);

    // Minimal custom HUD elements
    void RenderMinimap();
    void RenderInstructions();

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

    // Camera (additional properties if needed, base provided by FPSGame)
    bool m_MouseLocked{false};
    const float m_CameraSpeed{5.0f}; // Increased for clearer, more responsive movement
    const float m_CameraHeight{1.75f}; // Head-mounted bodycam height

    // Physics timestep
    float m_PhysicsAccumulator{0.0f};
    const float m_PhysicsStep{1.0f / 60.0f};
    const int m_MaxSubSteps{5};

    // Material handles (using ResourceHandle for type safety)
    Core::MaterialHandle m_SnowMaterialHandle{Core::MaterialHandle::Invalid()};
    Core::MaterialHandle m_RockMaterialHandle{Core::MaterialHandle::Invalid()};
    Core::MaterialHandle m_TerrainMaterialHandle{Core::MaterialHandle::Invalid()};
    Core::MaterialHandle m_IceMaterialHandle{Core::MaterialHandle::Invalid()};

    // Systems
    std::unique_ptr<Render::ParticleSystem> m_SnowParticles;
    Game::MaterialPresetManager m_MaterialPresetManager;
    Game::BodycamHUD m_BodycamHUD;
    Game::SnowSystem m_SnowSystem;

    // Lighting
    std::vector<Physics::LightSource> m_Lights;

    // Skybox
    std::unique_ptr<Render::Skybox> m_Skybox;

    // Scene state
    bool m_SceneInitialized{false};
    bool m_AudioStarted{false};
    bool m_FirstFrameRendered{false};

    // Asset streaming
    Core::AssetStreamer m_AssetStreamer;
    Core::AssetHandle m_BlizzardAudioHandle;
    Core::AssetHandle m_MusicAudioHandle;
    Core::AssetHandle m_FootstepsAudioHandle;

    // Audio sources
    Core::Audio::AudioSource m_BlizzardAudioSource;
    Core::Audio::AudioSource m_FootstepAudioSource;


    // Footstep tracking
    float m_LastFootstepTime{0.0f};
    const float m_FootstepInterval{0.5f};
    bool m_WasOnGround{false};
    Math::Vec3 m_LastPosition{0.0f, 0.0f, 0.0f};

    // Player physics state
    bool m_IsGrounded{false};
    bool m_IsSliding{false};
    float m_JumpCooldown{0.0f};
    const float m_JumpForce{6.0f}; // m/s upward velocity
    const float m_NormalCapsuleHeight{1.75f};
    const float m_SlidingCapsuleHeight{0.8f};
    const float m_CapsuleRadius{0.3f};
    float m_CurrentCapsuleHeight{1.75f};

    // Mouse position
    float m_MouseX{0.0f};
    float m_MouseY{0.0f};
};

} // namespace Solstice::Blizzard

