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
#include <Arzachel/MeshData.hxx>
#include <Arzachel/Generator.hxx>
#include <UI/Core/Window.hxx>
#include <Render/SoftwareRenderer.hxx>
#include <Render/Scene/Scene.hxx>
#include <Render/Assets/Mesh.hxx>
#include <Render/Particle/ParticleSystem.hxx>
#include <Material/Material.hxx>
#include <Asset/IO/ResourceHandle.hxx>
#include <Physics/Integration/PhysicsSystem.hxx>
#include <Physics/Dynamics/RigidBody.hxx>
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
    void InitializeWeapons() override;

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
    void ResetPlayerToBlizzardSpawn();
    void InitializeAudio();
    void InitializeSkybox();
    void InitializeProceduralTextures();
    void InitializeBlizzardMaterials();
    void InitializeTerrain();
    void InitializeHills(); // Deprecated - kept for compatibility
    void InitializeStructures();
    void InitializeArcticBase();
    void InitializeImportedProps();
    void InitializeCombatTargets();
    void InitializeLODMountains();
    void InitializeSwissTown();
    void InitializeVegetation();
    void InitializeGroundVegetation();


    // Footstep effects
    void UpdateFootsteps(float dt);
    void ResolvePlayerWorldCollisions(Physics::RigidBody& PlayerRB, Game::FPSMovement& Movement);
    void UpdateCombat(float DeltaTime);
    void CheckBoundsAndRecover(float DeltaTime);
    void RenderCombatHUD();
    void AddStaticBox(const Math::Vec3& Position, const Math::Vec3& Scale, Core::MaterialHandle Material,
                      bool CreateCollision = true, const Math::Vec3& CollisionHalfExtents = Math::Vec3(0.0f, 0.0f, 0.0f));
    void AddStaticCylinder(const Math::Vec3& Position, float Radius, float Height, Core::MaterialHandle Material,
                           bool CreateCollision = true);

    // Arzachel sculpting pipeline: generate a single-submesh prop and stamp it into the world.
    // The Generator is evaluated with a derived seed so each call is deterministic and unique.
    void AddArzachelProp(const Arzachel::Generator<Arzachel::MeshData>& Gen,
                        const Math::Vec3& Position,
                        const Math::Quaternion& Rotation,
                        const Math::Vec3& Scale,
                        Core::MaterialHandle Material,
                        bool CreateCollision = true,
                        const Math::Vec3& CollisionHalfExtents = Math::Vec3(0.0f, 0.0f, 0.0f),
                        Physics::ColliderType ColliderType = Physics::ColliderType::Box,
                        uint32_t SeedSalt = 0);

    // Architectural builders: each composes Arzachel-generated meshes for a single complete prop.
    void BuildIndustrialBuilding(const Math::Vec3& Center, const Math::Vec3& HalfExtents,
                                 Core::MaterialHandle WallMat, Core::MaterialHandle RoofMat,
                                 Core::MaterialHandle TrimMat, Core::MaterialHandle WindowMat,
                                 bool HasWindowBand, bool HasFloodlight, uint32_t SeedSalt);
    void BuildStorageContainer(const Math::Vec3& Center, const Math::Vec3& HalfExtents,
                              Core::MaterialHandle ShellMat, Core::MaterialHandle TrimMat,
                              uint32_t SeedSalt);
    void BuildWatchTower(const Math::Vec3& Center, float ShellHalfX, float ShellHalfZ,
                        float ShellHeight, float StiltHeight,
                        Core::MaterialHandle ShellMat, Core::MaterialHandle StiltMat,
                        Core::MaterialHandle RoofMat, Core::MaterialHandle WindowMat,
                        uint32_t SeedSalt);
    void BuildAntennaMast(const Math::Vec3& Base, float Height,
                         Core::MaterialHandle Mat, uint32_t SeedSalt);
    void BuildSnowMound(const Math::Vec3& Center, float Radius, float HeightScale,
                       Core::MaterialHandle Mat, uint32_t SeedSalt, bool CreateCollision = false);
    void BuildRockOutcrop(const Math::Vec3& Center, float Size,
                         Core::MaterialHandle Mat, uint32_t SeedSalt, bool CreateCollision = true);
    void BuildFenceLine(const Math::Vec3& Start, const Math::Vec3& End, float Height,
                       Core::MaterialHandle PostMat, int PostCount);
    // Tall, craggy snow-capped mountain. Composes multiple Arzachel meshes (rock body +
    // snow cap + side spurs) and triangles below ground plane / behind play area are
    // pre-trimmed so we don't pay for invisible geometry.
    void BuildMountainPeak(const Math::Vec3& Base, float Radius, float Height,
                          Core::MaterialHandle RockMat, Core::MaterialHandle SnowMat,
                          uint32_t SeedSalt);

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
    Core::MaterialHandle m_PaintedMetalMaterialHandle{Core::MaterialHandle::Invalid()};
    Core::MaterialHandle m_ContainerMaterialHandle{Core::MaterialHandle::Invalid()};
    Core::MaterialHandle m_ConcreteMaterialHandle{Core::MaterialHandle::Invalid()};
    Core::MaterialHandle m_WoodMaterialHandle{Core::MaterialHandle::Invalid()};
    Core::MaterialHandle m_UtilityYellowMaterialHandle{Core::MaterialHandle::Invalid()};
    Core::MaterialHandle m_DarkRoofMaterialHandle{Core::MaterialHandle::Invalid()};
    Core::MaterialHandle m_RustedMetalMaterialHandle{Core::MaterialHandle::Invalid()};
    Core::MaterialHandle m_PlasticMaterialHandle{Core::MaterialHandle::Invalid()};
    Core::MaterialHandle m_GlowingWindowMaterialHandle{Core::MaterialHandle::Invalid()};
    Core::MaterialHandle m_EnemyMaterialHandle{Core::MaterialHandle::Invalid()};

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
    Core::Audio::AudioEmitterHandle m_WindHowlEmitter{0};


    // Footstep tracking (planar body movement — avoids camera bob spam)
    float m_LastFootstepTime{0.0f};
    Math::Vec3 m_LastFootstepBodyPos{0.0f, 0.0f, 0.0f};
    const float m_FootstepMinInterval{0.62f};
    const float m_FootstepStride{0.65f};
    const float m_FootstepVolume{0.025f};
    bool m_WasOnGround{false};
    Math::Vec3 m_LastPosition{0.0f, 0.0f, 0.0f};

    // Double-tap sprint on forward (W on QWERTY, Z on AZERTY)
    float m_ForwardTapTimer{0.0f};
    bool m_DoubleTapSprintActive{false};
    const float m_ForwardDoubleTapWindow{0.28f};

    // Combat and world bounds
    std::vector<ECS::EntityId> m_CombatTargets;
    std::vector<Render::SceneObjectID> m_CombatTargetObjects;
    float m_WeaponCooldown{0.0f};
    bool m_PendingShot{false};
    float m_BoundsMessageTimer{0.0f};
    Math::Vec3 m_SafeSpawnPosition{0.0f, 1.195f, -42.0f};
    Math::Vec3 m_ActiveWindDirection{0.8f, 0.0f, 1.0f};
    float m_ActiveWindStrength{5.0f};
    const float m_WalkSpeed{5.0f};
    const float m_SprintSpeed{10.0f};
    const float m_PlayableHalfExtent{88.0f};
    const float m_FallRecoveryY{-12.0f};

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

