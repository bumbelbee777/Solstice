#include "BlizzardGame.hxx"
#include <Game/FPS/FPSMovement.hxx>
#include <Game/Gameplay/Health.hxx>
#include <UI/Core/UISystem.hxx>
#include <Render/Assets/Mesh.hxx>
#include <Material/Material.hxx>
#include <Arzachel/ProceduralTexture.hxx>
#include <Arzachel/MaterialPresets.hxx>
#include <Arzachel/GeometryOps.hxx>
#include <Arzachel/AssetPresets.hxx>
#include <Arzachel/AssetBuilder.hxx>
#include <Arzachel/TerrainGenerator.hxx>
#include <Arzachel/Seed.hxx>
#include <Render/Scene/Skybox.hxx>
#include <Render/PhysicsBridge.hxx>
#include <Physics/Integration/PhysicsSystem.hxx>
#include <Physics/Dynamics/RigidBody.hxx>
#include <Physics/Lighting/LightSource.hxx>
#include <Arzachel/MaterialSerializer.hxx>
#include <Core/System/Async.hxx>
#include <Core/Audio/Audio.hxx>
#include <Asset/Streaming/AssetStream.hxx>
#include <Core/Debug/Debug.hxx>
#include <Solstice.hxx>
#include <imgui.h>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <random>
#include <cmath>
#include <thread>
#include <chrono>
#include <cstdint>
#include <limits>
#include <filesystem>

// Key constants
static constexpr int KEY_ESCAPE = 27;
static constexpr int MOD_CTRL = 0x0040;
static constexpr int MOD_ALT  = 0x0100;

using namespace Solstice;
using namespace Solstice::Render;
using namespace Solstice::Core;
using namespace Solstice::UI;
using namespace Solstice::Math;
using namespace Solstice::Game;
using namespace Solstice::Blizzard;
using namespace Solstice::Scripting;

namespace Solstice::Blizzard {

BlizzardGame::BlizzardGame() : FPSGame() {
}

BlizzardGame::~BlizzardGame() {
    Shutdown();
}

// Run() method removed - using inherited GameBase::Run()

void BlizzardGame::Initialize() {
    // CRITICAL: Initialize unified engine systems (JobSystem, Audio, etc.)
    // This MUST be called before any JobSystem or AudioManager usage.
    Solstice::Initialize();

    // Initialize window first
    InitializeWindow();

    // Initialize FPSGame base systems
    FPSGame::Initialize();

    // Blizzard demo starts directly in Playing mode (no main menu)
    if (m_GameState) {
        m_GameState->SetState(Solstice::Game::GameStateType::Playing);
    }

    std::cout << "=== Solstice Engine - Blizzard Demo ===" << std::endl;
    std::cout << "An immersive first-person blizzard experience refactored with Moonwalk & Arzachel" << std::endl;

    // Start physics system
    Physics::PhysicsSystem::Instance().Start(m_Registry);

    // Initialize script manager
    if (!m_ScriptManager.Initialize("scripts", &m_Registry, &m_Scene, &Physics::PhysicsSystem::Instance(), &m_Camera)) {
        SIMPLE_LOG("WARNING: ScriptManager initialization failed - scripts may not work");
    }

    // Register Blizzard-specific native functions
    m_ScriptManager.RegisterNative("Blizzard.SetSnowIntensity", [this](const std::vector<Scripting::Value>& Args) -> Scripting::Value {
        if (Args.size() >= 1 && m_SnowParticles) {
            m_SnowParticles->SetDensity(GetFloat(Args[0]));
        }
        return (int64_t)0;
    });

    m_ScriptManager.RegisterNative("Blizzard.SetWind", [this](const std::vector<Scripting::Value>& Args) -> Scripting::Value {
        if (Args.size() >= 3 && m_SnowParticles) {
            m_SnowParticles->SetWindDirection(Vec3(GetFloat(Args[0]), GetFloat(Args[1]), GetFloat(Args[2])));
        }
        if (Args.size() >= 4 && m_SnowParticles) {
            m_SnowParticles->SetWindStrength(GetFloat(Args[3]));
        }
        return (int64_t)0;
    });

    // Schedule script execution after textures and scene are ready
    std::function<bool()> Condition = [this]() {
        return m_TextureManager.IsReady() && m_SceneInitialized;
    };
    m_ScriptManager.ExecuteModuleWhen(Condition, "Blizzard", 5000);

    // Create renderer (retaining software renderer for the demo's aesthetic)
    auto fbSize = m_Window->GetFramebufferSize();
    m_Renderer = std::make_unique<SoftwareRenderer>(fbSize.first, fbSize.second, 16, m_Window->NativeWindow());
    m_Renderer->SetWireframe(false);
    m_Renderer->SetShowDebugOverlay(m_ShowDebugOverlay);
    m_Renderer->SetShowCrosshair(false); // Disable built-in crosshair (HUD has its own)
    m_Renderer->SetPhysicsRegistry(&m_Registry);

    // Initialize UI system AFTER BGFX is initialized (by SoftwareRenderer)
    if (!UI::UISystem::Instance().IsInitialized()) {
        UI::UISystem::Instance().Initialize(m_Window->NativeWindow());
    }

    // Create scene and libraries
    m_MeshLibrary = std::make_unique<MeshLibrary>();
    m_MaterialLibrary = std::make_unique<Core::MaterialLibrary>();
    m_Scene.SetMeshLibrary(m_MeshLibrary.get());
    m_Scene.SetMaterialLibrary(m_MaterialLibrary.get());

    // Initialize material preset manager
    m_MaterialPresetManager.SetMaterialLibrary(m_MaterialLibrary.get());
    m_MaterialPresetManager.SetTextureRegistry(&m_Renderer->GetTextureRegistry());

    // Create default materials early (will be updated with textures later in FinalizeProceduralTextures)
    // This ensures material handles are valid when InitializeTerrain() is called
    Game::MaterialConfig DefaultConfig;
    m_SnowMaterialHandle = m_MaterialPresetManager.CreateMaterial(DefaultConfig);
    m_RockMaterialHandle = m_MaterialPresetManager.CreateMaterial(DefaultConfig);
    m_TerrainMaterialHandle = m_MaterialPresetManager.CreateMaterial(DefaultConfig);
    m_IceMaterialHandle = m_MaterialPresetManager.CreateMaterial(DefaultConfig);

    // Initialize procedural textures (async - will show loading screen)
    InitializeProceduralTextures();

    // Initialize skybox (doesn't depend on procedural textures)
    InitializeSkybox();

    // Initialize audio
    InitializeAudio();

    // Initialize snow particle system using preset
    try {
        Render::SnowParticleConfig SnowConfig;
        SnowConfig.WindDirection = Math::Vec3(0.5f, 0.0f, 0.8f);
        SnowConfig.WindStrength = 2.0f;
        SnowConfig.Density = 1.0f;
        m_SnowParticles = Render::ParticlePresets::CreateSnowParticleSystem(SnowConfig);
        m_SnowParticles->SetWindDirection(SnowConfig.WindDirection);
        m_SnowParticles->SetWindStrength(SnowConfig.WindStrength);
        m_SnowParticles->SetDensity(SnowConfig.Density);
    } catch (...) {
        SIMPLE_LOG("ERROR: Failed to initialize snow particle system");
    }

    // Initialize snow system for movement physics
    m_SnowSystem.SetBaseDepth(0.3f);        // 30cm base depth
    m_SnowSystem.SetDepthVariation(0.4f);  // 40cm variation for drifts
    m_SnowSystem.SetMaxDepth(1.5f);         // Maximum 1.5m depth
    m_SnowSystem.SetSeed(40000);            // Seed for noise generation

    // Initialize bodycam HUD
    Game::BodycamHUDConfig BodycamConfig;
    m_BodycamHUD = Game::BodycamHUD(BodycamConfig);

    // Initialize mouse position
    if (m_Window) {
        m_MouseX = m_Window->GetFramebufferSize().first * 0.5f;
        m_MouseY = m_Window->GetFramebufferSize().second * 0.5f;
    }
}

void BlizzardGame::Shutdown() {
    // Shutdown managers (they will wait for async tasks to complete)
    m_TextureManager.Shutdown();
    m_ScriptManager.Shutdown();

    Solstice::Shutdown();

    if (m_SnowParticles) {
        m_SnowParticles->Shutdown();
    }

    // BodycamHUD cleanup is automatic (no manual shutdown needed)
}

void BlizzardGame::InitializeWindow() {
    // Create window in fullscreen mode
    auto WindowPtr = std::make_unique<Window>(1280, 720, "Solstice Engine - Blizzard");
    WindowPtr->SetFullscreen(true);
    m_IsFullscreen = true;

    // Set up callbacks
    WindowPtr->SetResizeCallback([this](int W, int H) {
        HandleWindowResize(W, H);
    });

    WindowPtr->SetKeyCallback([this](int Key, int Scancode, int Action, int Mods) {
        HandleKeyInput(Key, Scancode, Action, Mods);
    });

    WindowPtr->SetMouseButtonCallback([this](int Button, int Action, int Mods) {
        HandleMouseButton(Button, Action, Mods);
    });

    WindowPtr->SetCursorPosCallback([this](double DX, double DY) {
        HandleCursorPos(DX, DY);
    });

    // Set window via GameBase
    SetWindow(std::move(WindowPtr));
}

void BlizzardGame::InitializeFPSSystems() {
    FPSGame::InitializeFPSSystems();
    // Specialized FPS settings for Blizzard demo
    m_ShowCrosshair = true;
    m_ShowWeaponHUD = false;
}

void BlizzardGame::InitializePlayer() {
    FPSGame::InitializePlayer();

    // Add Health component for HUD rendering
    if (m_PlayerEntity != 0) {
        Game::Health PlayerHealth;
        PlayerHealth.CurrentHealth = 100.0f;
        PlayerHealth.MaxHealth = 100.0f;
        m_Registry.Add<Game::Health>(m_PlayerEntity, PlayerHealth);
    }

    // Customize player for Blizzard demo
    // SIMPLIFIED: Spawn at center of flat terrain
    float SpawnHeight = 2.0f; // Spawn 2 units above flat ground (Y=0)
    Math::Vec3 SpawnPosition = Math::Vec3(0.0f, SpawnHeight, 0.0f); // Center of flat terrain

    // Customize FPS movement - SIMPLIFIED for debugging
    if (m_Registry.Has<Game::FPSMovement>(m_PlayerEntity)) {
        auto& Movement = m_Registry.Get<Game::FPSMovement>(m_PlayerEntity);
        Movement.MaxGroundSpeed = 7.0f; // Standard speed for debugging
        Movement.GroundAcceleration = 20.0f; // Standard acceleration
        Movement.GroundFriction = 6.0f; // Standard friction
        Movement.AirAcceleration = 10.0f; // Better air control for debugging
    }

    if (m_Registry.Has<Physics::RigidBody>(m_PlayerEntity)) {
        auto& RB = m_Registry.Get<Physics::RigidBody>(m_PlayerEntity);
        RB.Position = SpawnPosition;
        RB.Friction = 0.6f; // Walking on snow
        RB.LinearDamping = 0.05f;

        // CRITICAL: Force sync player body to ReactPhysics3D immediately
        // This ensures the player exists in the physics world with correct position
        Physics::ReactPhysics3DBridge& Bridge = Physics::PhysicsSystem::Instance().GetBridge();
        Bridge.SyncToReactPhysics3D();
    }

    m_Camera.Position = SpawnPosition;
    m_Camera.Zoom = 60.0f; // Bodycam FOV
    m_Camera.ProcessMouseMovement(0, 0, true);

    m_LastPosition = m_Camera.Position;
}

void BlizzardGame::HandleInput() {
    // Input is primarily handled through callbacks
}

void BlizzardGame::UpdateFPSMovement(float DeltaTime) {
    if (m_PlayerEntity == 0) return;

    // Get input
    Math::Vec3 moveDir(0, 0, 0);
    bool jump = false;
    bool crouch = false;
    bool sprint = false;

    if (m_InputManager) {
        // WASD (QWERTY) and ZQSD (AZERTY) movement support
        // Forward: W (26) or Z (25)
        if (m_InputManager->IsKeyPressed(26) || m_InputManager->IsKeyPressed(25)) moveDir.z += 1.0f;
        // Backward: S (22) - same for both layouts
        if (m_InputManager->IsKeyPressed(22)) moveDir.z -= 1.0f;
        // Left: A (4) or Q (20)
        if (m_InputManager->IsKeyPressed(4) || m_InputManager->IsKeyPressed(20)) moveDir.x -= 1.0f;
        // Right: D (7) - same for both layouts
        if (m_InputManager->IsKeyPressed(7)) moveDir.x += 1.0f;
        // Jump: Space (44)
        if (m_InputManager->IsKeyPressed(44)) jump = true;
        // Crouch: Ctrl (29)
        if (m_InputManager->IsKeyPressed(29)) crouch = true;
        // Sprint: Shift (225)
        if (m_InputManager->IsKeyPressed(225)) sprint = true;
    }

    // Pass snow system to movement system
    Game::FPSMovementSystem::ProcessInput(m_Registry, m_PlayerEntity, moveDir, jump, crouch, sprint);
    Game::FPSMovementSystem::Update(m_Registry, DeltaTime, m_Camera, &m_SnowSystem);
}

void BlizzardGame::HandleWindowResize(int W, int H) {
    m_PendingResize = true;
    m_PendingWidth = W;
    m_PendingHeight = H;
    m_LastResizeEvent = std::chrono::high_resolution_clock::now();
}

void BlizzardGame::HandleKeyInput(int Key, int Scancode, int Action, int Mods) {
    auto* WindowPtr = GetWindow();
    if (!WindowPtr) return;

    if (Action == 1) { // Key press
        // F - Toggle fullscreen/maximized
        if (Key == 'f' || Key == 'F') {
            if (m_IsFullscreen) {
                WindowPtr->SetFullscreen(false);
                WindowPtr->SetMaximized(true);
                WindowPtr->SetResizable(false);
                m_IsFullscreen = false;
                m_IsMaximized = true;
                auto FB = WindowPtr->GetFramebufferSize();
                m_PendingResize = true;
                m_PendingWidth = FB.first;
                m_PendingHeight = FB.second;
                m_LastResizeEvent = std::chrono::high_resolution_clock::now();
            } else {
                WindowPtr->SetFullscreen(true);
                WindowPtr->SetResizable(true);
                m_IsFullscreen = true;
                m_IsMaximized = false;
                auto FB = WindowPtr->GetFramebufferSize();
                m_PendingResize = true;
                m_PendingWidth = FB.first;
                m_PendingHeight = FB.second;
                m_LastResizeEvent = std::chrono::high_resolution_clock::now();
            }
        }

        // ESC - Release mouse lock
        if (Key == KEY_ESCAPE) {
            if (m_MouseLocked) {
                m_MouseLocked = false;
                WindowPtr->SetRelativeMouse(false);
                WindowPtr->SetCursorGrab(false);
                WindowPtr->ShowCursor(true);
                m_Renderer->SetShowCrosshair(false);
            }
        }

        // F3 - Toggle debug overlay
        if (Scancode == 60) { // F3 (SDL3 scancode)
            m_ShowDebugOverlay = !m_ShowDebugOverlay;
            if (m_Renderer) {
                m_Renderer->SetShowDebugOverlay(m_ShowDebugOverlay);
            }
            SIMPLE_LOG("Debug overlay: " + std::string(m_ShowDebugOverlay ? "ENABLED" : "DISABLED"));
        }

        // Ctrl+Alt - Lock mouse (removed, using right mouse button only)
    }
}

void BlizzardGame::HandleMouseButton(int Button, int Action, int Mods) {
    (void)Mods;
    auto* WindowPtr = GetWindow();
    if (!WindowPtr) return;

    if (Button == 2 && Action == 1) { // Right mouse button
        m_MouseLocked = !m_MouseLocked;
        WindowPtr->SetRelativeMouse(m_MouseLocked);
        WindowPtr->SetCursorGrab(m_MouseLocked);
        WindowPtr->ShowCursor(!m_MouseLocked);
        m_Renderer->SetShowCrosshair(m_MouseLocked);
    }
}

void BlizzardGame::HandleCursorPos(double DX, double DY) {
    auto* WindowPtr = GetWindow();
    if (!WindowPtr) return;

    if (m_MouseLocked) {
        m_Camera.ProcessMouseMovement(static_cast<float>(DX), static_cast<float>(-DY));
    } else {
        // Update mouse position
        auto FBSize = WindowPtr->GetFramebufferSize();
        m_MouseX = static_cast<float>(DX);
        m_MouseY = static_cast<float>(DY);
    }
}

void BlizzardGame::InitializeProceduralTextures() {
    using namespace Solstice::Arzachel;


    // Prepare texture configurations with custom generators
    std::vector<Game::TextureConfig> TextureConfigs;

    // Main textures (512x512)
    TextureConfigs.push_back({"Snow", 512, 7, 6.0f, 10000, []() { return MaterialPresets::SnowData(Arzachel::Seed(10000)); }});
    TextureConfigs.push_back({"Rock", 512, 7, 8.0f, 15000, []() { return MaterialPresets::RockData(Arzachel::Seed(15000)); }});
    TextureConfigs.push_back({"Terrain", 512, 7, 4.0f, 20000, []() { return ProceduralTexture::GenerateNoise(512, 7, 4.0f, Arzachel::Seed(20000)); }});
    TextureConfigs.push_back({"Ice", 512, 8, 12.0f, 20000, []() { return MaterialPresets::IceData(Arzachel::Seed(20000)); }});

    // Detail textures (128x128)
    TextureConfigs.push_back({"Detail Snow", 128, 6, 16.0f, 25000, []() { return MaterialPresets::FrostData(Arzachel::Seed(25000)); }});
    TextureConfigs.push_back({"Detail Ice", 128, 7, 20.0f, 26000, []() { return ProceduralTexture::GenerateNoise(256, 10, 15.0f, Arzachel::Seed(26000)); }});
    TextureConfigs.push_back({"Detail Rock", 128, 6, 18.0f, 27000, []() { return ProceduralTexture::GenerateNoise(256, 6, 12.0f, Arzachel::Seed(27000)); }});

    // Blend masks (512x512)
    TextureConfigs.push_back({"Blend Snow", 512, 5, 6.0f, 28000, []() { return ProceduralTexture::GenerateNoise(512, 4, 2.0f, Arzachel::Seed(28000)); }});
    TextureConfigs.push_back({"Blend Ice", 512, 5, 6.0f, 29000, []() { return ProceduralTexture::GenerateNoise(512, 4, 2.0f, Arzachel::Seed(29000)); }});
    TextureConfigs.push_back({"Blend Rock", 512, 5, 6.0f, 30000, []() { return ProceduralTexture::GenerateNoise(512, 4, 2.0f, Arzachel::Seed(30000)); }});
    TextureConfigs.push_back({"Blend Terrain", 512, 5, 6.0f, 31000, []() { return ProceduralTexture::GenerateNoise(512, 4, 2.0f, Arzachel::Seed(31000)); }});

    // Initialize texture manager
    m_TextureManager.Initialize(TextureConfigs, "cache");
}

void BlizzardGame::InitializeSkybox() {
    m_Skybox = std::make_unique<Skybox>();
    m_Skybox->Initialize(512); // 512x512 cubemap faces

    // Set overcast preset for blizzard atmosphere
    m_Skybox->SetPreset(Render::SkyPreset::Overcast);

    // Set skybox in renderer
    if (m_Renderer) {
        m_Renderer->SetSkybox(m_Skybox.get());
    }
}

void BlizzardGame::InitializeScene() {
    // SIMPLIFIED: Just flat terrain for debugging movement
    InitializeTerrain();

    // DISABLED FOR DEBUGGING - Re-enable once movement works:
    // InitializeLODMountains();
    // InitializeSwissTown();
    // InitializeVegetation();
    // InitializeGroundVegetation();

    // Update scene transforms after adding all objects
    m_Scene.UpdateTransforms();

    SIMPLE_LOG("Blizzard: Scene initialized");
}

void BlizzardGame::InitializeTerrain() {
    Game::TerrainConfig Config;
    Config.Resolution = 64; // Smaller resolution for simpler terrain
    Config.TerrainSize = 100.0f; // Smaller terrain
    Config.HeightScale = 0.0f; // Completely flat terrain
    Config.Seed = 12345;
    Config.LakeMaterialID = m_IceMaterialHandle.IsValid() ? m_IceMaterialHandle.GetValue() : 0xFFFFFFFF;
    Config.TerrainMaterialID = m_TerrainMaterialHandle.IsValid() ? m_TerrainMaterialHandle.GetValue() : 0xFFFFFFFF;
    Config.CreatePhysicsBody = true;
    Config.EnableCaching = true;

    m_SceneBuilder.AddTerrain(&m_Scene, m_MeshLibrary.get(), m_MaterialLibrary.get(), &m_Registry, Config);
}

void BlizzardGame::InitializeHills() {
    Game::HillConfig Config;
    Config.Count = 5;
    Config.TerrainSize = 200.0f; // Use default terrain size
    Config.Seed = 12345;
    Config.MaterialID = m_RockMaterialHandle.IsValid() ? m_RockMaterialHandle.GetValue() : 0xFFFFFFFF;
    Config.CreatePhysicsBody = true;

    m_SceneBuilder.AddHills(&m_Scene, m_MeshLibrary.get(), m_MaterialLibrary.get(), &m_Registry, Config);
}

void BlizzardGame::InitializeStructures() {
    // Create wood material using MaterialPresetManager
    static Core::MaterialHandle WoodMaterialHandle = Core::MaterialHandle::Invalid();
    if (!WoodMaterialHandle.IsValid()) {
        Game::MaterialConfig WoodConfig;
        WoodConfig.AlbedoColor = Math::Vec3(0.4f, 0.25f, 0.1f);
        WoodConfig.Roughness = 0.7f;
        WoodMaterialHandle = m_MaterialPresetManager.CreateMaterial(WoodConfig);
    }

    Game::StructureConfig Config;
    Config.Count = 3;
    Config.TerrainSize = 200.0f; // Use default terrain size
    Config.Seed = 54321;
    Config.MaterialID = WoodMaterialHandle.IsValid() ? WoodMaterialHandle.GetValue() : 0xFFFFFFFF;
    Config.CreatePhysicsBody = false;

    m_SceneBuilder.AddStructures(&m_Scene, m_MeshLibrary.get(), m_MaterialLibrary.get(), Config);
}

void BlizzardGame::InitializeLODMountains() {
    using namespace Solstice::Arzachel;

    // Create mountain material
    static Core::MaterialHandle MountainMaterialHandle = Core::MaterialHandle::Invalid();
    if (!MountainMaterialHandle.IsValid()) {
        Game::MaterialConfig MountainConfig;
        MountainConfig.AlbedoColor = Math::Vec3(0.7f, 0.7f, 0.75f); // Snow-covered rock
        MountainConfig.Roughness = 0.8f;
        MountainMaterialHandle = m_MaterialPresetManager.CreateMaterial(MountainConfig);
    }

    // Place 6 mountains around the map perimeter
    Seed MountainSeed(5000);
    float TerrainSize = 100.0f;
    float MountainDistance = 150.0f; // Far from center

    for (int i = 0; i < 6; ++i) {
        Seed InstanceSeed = MountainSeed.Derive(i);
        float Angle = (static_cast<float>(i) / 6.0f) * 2.0f * 3.14159f;
        float X = std::cos(Angle) * MountainDistance;
        float Z = std::sin(Angle) * MountainDistance;

        // Vary mountain size
        float BaseWidth = 30.0f + Arzachel::NextFloat(InstanceSeed.Derive(0)) * 20.0f;
        float Height = 40.0f + Arzachel::NextFloat(InstanceSeed.Derive(1)) * 30.0f;
        float PeakWidth = BaseWidth * 0.3f;

        auto MountainMesh = Arzachel::CreateLODMountainMesh(BaseWidth, Height, PeakWidth, InstanceSeed);
        if (!MountainMesh || MountainMesh->Vertices.empty()) {
            continue;
        }

        uint32_t MountainMeshID = m_MeshLibrary->AddMesh(std::move(MountainMesh));

        // Set material
        Render::Mesh* MeshPtr = m_MeshLibrary->GetMesh(MountainMeshID);
        if (MeshPtr && !MeshPtr->SubMeshes.empty() && MountainMaterialHandle.IsValid()) {
            MeshPtr->SubMeshes[0].MaterialID = MountainMaterialHandle.GetValue();
        }

        auto MountainObjID = m_Scene.AddObject(
            MountainMeshID,
            Math::Vec3(X, 0, Z),
            Math::Quaternion(),
            Math::Vec3(1, 1, 1),
            Render::ObjectType_Static
        );
    }
}

void BlizzardGame::InitializeSwissTown() {
    // Create building materials
    static Core::MaterialHandle BuildingMaterialHandle = Core::MaterialHandle::Invalid();
    static Core::MaterialHandle RoadMaterialHandle = Core::MaterialHandle::Invalid();

    if (!BuildingMaterialHandle.IsValid()) {
        Game::MaterialConfig BuildingConfig;
        BuildingConfig.AlbedoColor = Math::Vec3(0.5f, 0.4f, 0.3f); // Wood/stone
        BuildingConfig.Roughness = 0.7f;
        BuildingMaterialHandle = m_MaterialPresetManager.CreateMaterial(BuildingConfig);
    }

    if (!RoadMaterialHandle.IsValid()) {
        Game::MaterialConfig RoadConfig;
        RoadConfig.AlbedoColor = Math::Vec3(0.2f, 0.2f, 0.2f); // Dark asphalt
        RoadConfig.Roughness = 0.9f;
        RoadMaterialHandle = m_MaterialPresetManager.CreateMaterial(RoadConfig);
    }

    // Manually place buildings in town layout
    std::vector<Game::BuildingConfig> Buildings;

    // Town center - Church
    Game::BuildingConfig Church;
    Church.BuildingType = Game::BuildingConfig::Type::SwissChurch;
    Church.Position = Math::Vec3(0, 0, 0);
    Church.Rotation = Math::Quaternion();
    Church.MaterialID = BuildingMaterialHandle.GetValue();
    Church.CreatePhysicsBody = true; // Enable collision
    Buildings.push_back(Church);

    // Town hall
    Game::BuildingConfig TownHall;
    TownHall.BuildingType = Game::BuildingConfig::Type::SwissTownHall;
    TownHall.Position = Math::Vec3(-15, 0, 0);
    TownHall.Rotation = Math::Quaternion();
    TownHall.MaterialID = BuildingMaterialHandle.GetValue();
    TownHall.CreatePhysicsBody = true; // Enable collision
    Buildings.push_back(TownHall);

    // Houses around town
    for (int i = 0; i < 8; ++i) {
        float Angle = (static_cast<float>(i) / 8.0f) * 2.0f * 3.14159f;
        float Distance = 25.0f + (i % 3) * 5.0f;
        float X = std::cos(Angle) * Distance;
        float Z = std::sin(Angle) * Distance;

        Game::BuildingConfig House;
        House.BuildingType = Game::BuildingConfig::Type::SwissHouse;
        House.Position = Math::Vec3(X, 0, Z);
        House.Rotation = Math::Quaternion::FromEuler(0, Angle, 0);
        House.Floors = 2 + (i % 2);
        House.MaterialID = BuildingMaterialHandle.GetValue();
        House.CreatePhysicsBody = true; // Enable collision
        Buildings.push_back(House);
    }

    // Shops
    for (int i = 0; i < 3; ++i) {
        float X = 20.0f + i * 8.0f;
        float Z = -10.0f;

        Game::BuildingConfig Shop;
        Shop.BuildingType = Game::BuildingConfig::Type::SwissShop;
        Shop.Position = Math::Vec3(X, 0, Z);
        Shop.Rotation = Math::Quaternion();
        Shop.MaterialID = BuildingMaterialHandle.GetValue();
        Shop.CreatePhysicsBody = true; // Enable collision
        Buildings.push_back(Shop);
    }

    m_SceneBuilder.AddBuildings(&m_Scene, m_MeshLibrary.get(), m_MaterialLibrary.get(), &m_Registry, Buildings);

    // Add roads connecting the town
    Game::RoadConfig RoadConfig;
    RoadConfig.MaterialID = RoadMaterialHandle.GetValue();

    // Main road through town center
    Game::RoadSegment MainRoad;
    MainRoad.Start = Math::Vec3(-40, 0, 0);
    MainRoad.End = Math::Vec3(40, 0, 0);
    MainRoad.Width = 6.0f;
    RoadConfig.Segments.push_back(MainRoad);

    // Cross road
    Game::RoadSegment CrossRoad;
    CrossRoad.Start = Math::Vec3(0, 0, -40);
    CrossRoad.End = Math::Vec3(0, 0, 40);
    CrossRoad.Width = 6.0f;
    RoadConfig.Segments.push_back(CrossRoad);

    // Roads to residential areas
    for (int i = 0; i < 4; ++i) {
        float Angle = (static_cast<float>(i) / 4.0f) * 2.0f * 3.14159f;
        float StartDist = 15.0f;
        float EndDist = 35.0f;

        Game::RoadSegment ResidentialRoad;
        ResidentialRoad.Start = Math::Vec3(std::cos(Angle) * StartDist, 0, std::sin(Angle) * StartDist);
        ResidentialRoad.End = Math::Vec3(std::cos(Angle) * EndDist, 0, std::sin(Angle) * EndDist);
        ResidentialRoad.Width = 4.0f;
        RoadConfig.Segments.push_back(ResidentialRoad);
    }

    m_SceneBuilder.AddRoads(&m_Scene, m_MeshLibrary.get(), m_MaterialLibrary.get(), &m_Registry, RoadConfig);
}

void BlizzardGame::InitializeVegetation() {
    // Create tree material
    static Core::MaterialHandle TreeMaterialHandle = Core::MaterialHandle::Invalid();
    if (!TreeMaterialHandle.IsValid()) {
        Game::MaterialConfig TreeConfig;
        TreeConfig.AlbedoColor = Math::Vec3(0.2f, 0.3f, 0.15f); // Dark green for winter
        TreeConfig.Roughness = 0.8f;
        TreeMaterialHandle = m_MaterialPresetManager.CreateMaterial(TreeConfig);
    }

    Game::TreeConfig Config;
    Config.Count = 30;
    Config.TerrainSize = 100.0f;
    Config.Seed = 7000;
    Config.MaterialID = TreeMaterialHandle.GetValue();
    Config.MinHeight = 4.0f;
    Config.MaxHeight = 8.0f;
    Config.SnowCovered = true;
    Config.CreatePhysicsBody = false;

    m_SceneBuilder.AddTrees(&m_Scene, m_MeshLibrary.get(), m_MaterialLibrary.get(), &m_Registry, Config);
}

void BlizzardGame::InitializeGroundVegetation() {
    // Create vegetation materials
    static Core::MaterialHandle GrassMaterialHandle = Core::MaterialHandle::Invalid();
    static Core::MaterialHandle BushMaterialHandle = Core::MaterialHandle::Invalid();
    static Core::MaterialHandle RockMaterialHandle = Core::MaterialHandle::Invalid();
    static Core::MaterialHandle SnowMaterialHandle = Core::MaterialHandle::Invalid();

    if (!GrassMaterialHandle.IsValid()) {
        Game::MaterialConfig GrassConfig;
        GrassConfig.AlbedoColor = Math::Vec3(0.3f, 0.25f, 0.2f); // Brown/winter grass
        GrassConfig.Roughness = 0.7f;
        GrassMaterialHandle = m_MaterialPresetManager.CreateMaterial(GrassConfig);
    }

    if (!BushMaterialHandle.IsValid()) {
        Game::MaterialConfig BushConfig;
        BushConfig.AlbedoColor = Math::Vec3(0.15f, 0.2f, 0.1f); // Dark green/brown
        BushConfig.Roughness = 0.8f;
        BushMaterialHandle = m_MaterialPresetManager.CreateMaterial(BushConfig);
    }

    if (!RockMaterialHandle.IsValid()) {
        Game::MaterialConfig RockConfig;
        RockConfig.AlbedoColor = Math::Vec3(0.4f, 0.4f, 0.4f); // Gray stone
        RockConfig.Roughness = 0.9f;
        RockMaterialHandle = m_MaterialPresetManager.CreateMaterial(RockConfig);
    }

    if (!SnowMaterialHandle.IsValid()) {
        Game::MaterialConfig SnowConfig;
        SnowConfig.AlbedoColor = Math::Vec3(0.95f, 0.95f, 1.0f); // White snow
        SnowConfig.Roughness = 0.3f;
        SnowMaterialHandle = m_MaterialPresetManager.CreateMaterial(SnowConfig);
    }

    // Collect road positions for exclusion
    std::vector<Math::Vec3> ExcludeAreas;
    ExcludeAreas.push_back(Math::Vec3(0, 0, 0)); // Town center
    ExcludeAreas.push_back(Math::Vec3(-15, 0, 0)); // Town hall
    for (int i = 0; i < 8; ++i) {
        float Angle = (static_cast<float>(i) / 8.0f) * 2.0f * 3.14159f;
        float Distance = 25.0f;
        ExcludeAreas.push_back(Math::Vec3(std::cos(Angle) * Distance, 0, std::sin(Angle) * Distance));
    }

    // Add grass patches
    Game::GroundVegetationConfig GrassConfig;
    GrassConfig.VegetationType = Game::GroundVegetationConfig::Type::GrassPatch;
    GrassConfig.Count = 50;
    GrassConfig.AreaMin = Math::Vec3(-50, 0, -50);
    GrassConfig.AreaMax = Math::Vec3(50, 0, 50);
    GrassConfig.MinSize = 0.5f;
    GrassConfig.MaxSize = 2.0f;
    GrassConfig.Seed = 8000;
    GrassConfig.MaterialID = GrassMaterialHandle.GetValue();
    GrassConfig.ExcludeAreas = ExcludeAreas;
    GrassConfig.ExcludeRadius = 8.0f;
    m_SceneBuilder.AddGroundVegetation(&m_Scene, m_MeshLibrary.get(), m_MaterialLibrary.get(), &m_Registry, GrassConfig);

    // Add bushes
    Game::GroundVegetationConfig BushConfig;
    BushConfig.VegetationType = Game::GroundVegetationConfig::Type::SmallBush;
    BushConfig.Count = 30;
    BushConfig.AreaMin = Math::Vec3(-50, 0, -50);
    BushConfig.AreaMax = Math::Vec3(50, 0, 50);
    BushConfig.MinSize = 0.8f;
    BushConfig.MaxSize = 1.5f;
    BushConfig.Seed = 8100;
    BushConfig.MaterialID = BushMaterialHandle.GetValue();
    BushConfig.ExcludeAreas = ExcludeAreas;
    BushConfig.ExcludeRadius = 8.0f;
    m_SceneBuilder.AddGroundVegetation(&m_Scene, m_MeshLibrary.get(), m_MaterialLibrary.get(), &m_Registry, BushConfig);

    // Add rocks
    Game::GroundVegetationConfig RockConfig;
    RockConfig.VegetationType = Game::GroundVegetationConfig::Type::Rock;
    RockConfig.Count = 20;
    RockConfig.AreaMin = Math::Vec3(-50, 0, -50);
    RockConfig.AreaMax = Math::Vec3(50, 0, 50);
    RockConfig.MinSize = 0.5f;
    RockConfig.MaxSize = 1.2f;
    RockConfig.Seed = 8200;
    RockConfig.MaterialID = RockMaterialHandle.GetValue();
    RockConfig.ExcludeAreas = ExcludeAreas;
    RockConfig.ExcludeRadius = 8.0f;
    m_SceneBuilder.AddGroundVegetation(&m_Scene, m_MeshLibrary.get(), m_MaterialLibrary.get(), &m_Registry, RockConfig);

    // Add snow drifts
    Game::GroundVegetationConfig SnowConfig;
    SnowConfig.VegetationType = Game::GroundVegetationConfig::Type::SnowDrift;
    SnowConfig.Count = 15;
    SnowConfig.AreaMin = Math::Vec3(-50, 0, -50);
    SnowConfig.AreaMax = Math::Vec3(50, 0, 50);
    SnowConfig.MinSize = 2.0f;
    SnowConfig.MaxSize = 4.0f;
    SnowConfig.Seed = 8300;
    SnowConfig.MaterialID = SnowMaterialHandle.GetValue();
    SnowConfig.ExcludeAreas = ExcludeAreas;
    SnowConfig.ExcludeRadius = 8.0f;
    m_SceneBuilder.AddGroundVegetation(&m_Scene, m_MeshLibrary.get(), m_MaterialLibrary.get(), &m_Registry, SnowConfig);
}

void BlizzardGame::FinalizeProceduralTextures() {
    if (!m_TextureManager.IsReady()) {
                return;
    }

    // Finalize textures (register them)
    // We need to create MaterialAssignments for the old Finalize API, but we'll update materials using MaterialPresetManager
    std::vector<Game::MaterialAssignment> MaterialAssignments;
    MaterialAssignments.resize(4);

    // Snow material (texture index 0, detail texture index 4, blend mask index 7)
    MaterialAssignments[0] = {m_SnowMaterialHandle.GetValue(), 0, 4, 7, Math::Vec3(0.95f, 0.95f, 1.0f), 0.2f,
                              static_cast<uint8_t>(Core::TextureBlendMode::Multiply), 0.4f, "assets/materials/snow.json"};

    // Rock material (texture index 1, detail texture index 6, blend mask index 9)
    MaterialAssignments[1] = {m_RockMaterialHandle.GetValue(), 0, 6, 9, Math::Vec3(0.3f, 0.3f, 0.35f), 0.8f,
                              static_cast<uint8_t>(Core::TextureBlendMode::Multiply), 0.5f, "assets/materials/rock.json"};

    // Terrain material (texture index 2, detail texture index 4, blend mask index 10)
    MaterialAssignments[2] = {m_TerrainMaterialHandle.GetValue(), 0, 4, 10, Math::Vec3(0.7f, 0.7f, 0.75f), 0.5f,
                              static_cast<uint8_t>(Core::TextureBlendMode::Multiply), 0.3f, "assets/materials/terrain.json"};

    // Ice material (texture index 3, detail texture index 5, blend mask index 8)
    MaterialAssignments[3] = {m_IceMaterialHandle.GetValue(), 0, 5, 8, Math::Vec3(0.85f, 0.9f, 1.0f), 0.1f,
                              static_cast<uint8_t>(Core::TextureBlendMode::Multiply), 0.35f, "assets/materials/ice.json"};

    m_TextureManager.Finalize(m_Renderer.get(), m_MaterialLibrary.get(), MaterialAssignments);

    // Now update materials using MaterialPresetManager with texture handles
    // Get texture handles after registration
    Core::TextureHandle SnowTex = m_TextureManager.GetTextureHandle(0);
    Core::TextureHandle SnowDetailTex = m_TextureManager.GetTextureHandle(4);
    Core::TextureHandle SnowBlendTex = m_TextureManager.GetTextureHandle(7);

    Core::TextureHandle RockTex = m_TextureManager.GetTextureHandle(1);
    Core::TextureHandle RockDetailTex = m_TextureManager.GetTextureHandle(6);
    Core::TextureHandle RockBlendTex = m_TextureManager.GetTextureHandle(9);

    Core::TextureHandle TerrainTex = m_TextureManager.GetTextureHandle(2);
    Core::TextureHandle TerrainDetailTex = m_TextureManager.GetTextureHandle(4);
    Core::TextureHandle TerrainBlendTex = m_TextureManager.GetTextureHandle(10);

    Core::TextureHandle IceTex = m_TextureManager.GetTextureHandle(3);
    Core::TextureHandle IceDetailTex = m_TextureManager.GetTextureHandle(5);
    Core::TextureHandle IceBlendTex = m_TextureManager.GetTextureHandle(8);

    // Update materials with textures (materials already exist, we just update them)
    // Note: MaterialPresetManager creates new materials, so we update existing ones directly
    if (m_SnowMaterialHandle.IsValid()) {
        Core::Material* Mat = m_MaterialLibrary->GetMaterial(m_SnowMaterialHandle.GetValue());
        if (Mat && SnowTex.IsValid()) {
            Mat->AlbedoTexIndex = SnowTex.GetValue();
            if (SnowDetailTex.IsValid()) Mat->AlbedoTexIndex2 = SnowDetailTex.GetValue();
            if (SnowBlendTex.IsValid()) Mat->AlbedoTexIndex3 = SnowBlendTex.GetValue();
            Mat->SetAlbedoColor(Math::Vec3(0.95f, 0.95f, 1.0f), 0.2f);
            Mat->TextureBlendMode = static_cast<uint8_t>(Core::TextureBlendMode::Multiply);
            Mat->SetTextureBlendFactor(0.4f);
        }
    }

    if (m_RockMaterialHandle.IsValid()) {
        Core::Material* Mat = m_MaterialLibrary->GetMaterial(m_RockMaterialHandle.GetValue());
        if (Mat && RockTex.IsValid()) {
            Mat->AlbedoTexIndex = RockTex.GetValue();
            if (RockDetailTex.IsValid()) Mat->AlbedoTexIndex2 = RockDetailTex.GetValue();
            if (RockBlendTex.IsValid()) Mat->AlbedoTexIndex3 = RockBlendTex.GetValue();
            Mat->SetAlbedoColor(Math::Vec3(0.3f, 0.3f, 0.35f), 0.8f);
            Mat->TextureBlendMode = static_cast<uint8_t>(Core::TextureBlendMode::Multiply);
            Mat->SetTextureBlendFactor(0.5f);
        }
    }

    if (m_TerrainMaterialHandle.IsValid()) {
        Core::Material* Mat = m_MaterialLibrary->GetMaterial(m_TerrainMaterialHandle.GetValue());
        if (Mat && TerrainTex.IsValid()) {
            Mat->AlbedoTexIndex = TerrainTex.GetValue();
            if (TerrainDetailTex.IsValid()) Mat->AlbedoTexIndex2 = TerrainDetailTex.GetValue();
            if (TerrainBlendTex.IsValid()) Mat->AlbedoTexIndex3 = TerrainBlendTex.GetValue();
            Mat->SetAlbedoColor(Math::Vec3(0.7f, 0.7f, 0.75f), 0.5f);
            Mat->TextureBlendMode = static_cast<uint8_t>(Core::TextureBlendMode::Multiply);
            Mat->SetTextureBlendFactor(0.3f);
        }
    }

    if (m_IceMaterialHandle.IsValid()) {
        Core::Material* Mat = m_MaterialLibrary->GetMaterial(m_IceMaterialHandle.GetValue());
        if (Mat && IceTex.IsValid()) {
            Mat->AlbedoTexIndex = IceTex.GetValue();
            if (IceDetailTex.IsValid()) Mat->AlbedoTexIndex2 = IceDetailTex.GetValue();
            if (IceBlendTex.IsValid()) Mat->AlbedoTexIndex3 = IceBlendTex.GetValue();
            Mat->SetAlbedoColor(Math::Vec3(0.85f, 0.9f, 1.0f), 0.1f);
            Mat->TextureBlendMode = static_cast<uint8_t>(Core::TextureBlendMode::Multiply);
            Mat->SetTextureBlendFactor(0.35f);
        }
    }

}

void BlizzardGame::InitializeAudio() {
    // Register audio loader with AssetStreamer
    m_AssetStreamer.SetLoader(Core::AssetType::Audio, [](const std::string& Path) -> std::shared_ptr<Core::AssetData> {
        // For now, just return a placeholder - actual loading would be done by AudioManager
        auto Asset = std::make_shared<Core::AssetData>(Core::AssetType::Audio, Path);
        Asset->Path = Path;
        return Asset;
    });

    // Request audio assets (async loading)
    m_BlizzardAudioHandle = m_AssetStreamer.RequestAsset(
        Core::AssetType::Audio, "assets/blizzard.wav");
    m_MusicAudioHandle = m_AssetStreamer.RequestAsset(
        Core::AssetType::Audio, "assets/music.wav");
    m_FootstepsAudioHandle = m_AssetStreamer.RequestAsset(
        Core::AssetType::Audio, "assets/footsteps.wav");

    // Prefetch assets
    m_AssetStreamer.PrefetchAsset(Core::AssetType::Audio, "assets/blizzard.wav");
    m_AssetStreamer.PrefetchAsset(Core::AssetType::Audio, "assets/music.wav");
    m_AssetStreamer.PrefetchAsset(Core::AssetType::Audio, "assets/footsteps.wav");

    // Don't play audio during loading - will start after textures are ready
    // Audio will be started in Update() when textures are ready
}

void BlizzardGame::Update(float DeltaTime) {
    // Update base FPSGame systems FIRST (processes input and applies movement forces)
    FPSGame::Update(DeltaTime);

    // Update physics system AFTER movement (so movement velocity is applied)
    if (m_SceneInitialized) {
        Physics::PhysicsSystem::Instance().Update(DeltaTime);

        // CRITICAL: Re-apply horizontal velocity after physics runs
        // ReactPhysics3D overwrites velocity during physics step, so we restore it
        // Also apply snow modifiers to the actual physics velocity
        if (m_PlayerEntity != 0 && m_Registry.Has<Game::FPSMovement>(m_PlayerEntity) &&
            m_Registry.Has<Physics::RigidBody>(m_PlayerEntity)) {
            auto& Movement = m_Registry.Get<Game::FPSMovement>(m_PlayerEntity);
            auto& RB = m_Registry.Get<Physics::RigidBody>(m_PlayerEntity);

            // Read actual velocity from ReactPhysics3D (after physics step)
            Physics::ReactPhysics3DBridge& Bridge = Physics::PhysicsSystem::Instance().GetBridge();
            Bridge.SyncFromReactPhysics3D();

            // Re-apply desired horizontal velocity
            RB.Velocity.x = Movement.DesiredHorizontalVelocity.x;
            RB.Velocity.z = Movement.DesiredHorizontalVelocity.z;

            // Apply snow resistance to the re-applied velocity if snow is enabled
            if (Movement.EnableSnowMovement && Movement.SnowResistanceMultiplier > 0.0f) {
                Math::Vec3 horizontalVel = RB.Velocity;
                horizontalVel.y = 0.0f;
                float speed = horizontalVel.Magnitude();
                if (speed > 0.01f) {
                    float resistance = Movement.SnowResistanceMultiplier;
                    horizontalVel = horizontalVel * (1.0f - resistance);
                    RB.Velocity.x = horizontalVel.x;
                    RB.Velocity.z = horizontalVel.z;
                }
            }

            // Immediately sync back to ReactPhysics3D
            Bridge.SyncToReactPhysics3D();
        }
    }

    auto* WindowPtr = GetWindow();
    if (!WindowPtr) return;

    // Update Moonwalk VM (could trigger module logic)
    // For now, we manually tick any needed script functions if not handled by event systems

    // Update script manager (checks deferred execution conditions)
    m_ScriptManager.Update(DeltaTime);

    // Check if textures are ready and finalize them
    if (m_TextureManager.IsReady()) {
        static bool Finalized = false;
        if (!Finalized) {
            try {
                FinalizeProceduralTextures();
                Finalized = true;
            } catch (const std::exception& e) {
                SIMPLE_LOG("ERROR: Exception in FinalizeProceduralTextures: " + std::string(e.what()));
            } catch (...) {
                SIMPLE_LOG("ERROR: Unknown exception in FinalizeProceduralTextures");
            }
        }

        // Initialize scene after textures are ready
        if (!m_SceneInitialized) {
            try {
                InitializeScene();
                m_SceneInitialized = true;
            } catch (const std::exception& e) {
                SIMPLE_LOG("ERROR: Exception in InitializeScene: " + std::string(e.what()));
            } catch (...) {
                SIMPLE_LOG("ERROR: Unknown exception in InitializeScene");
            }
        }

        // Initialize audio listener (scripts handle playback)
        if (m_SceneInitialized && !m_AudioStarted) {
            try {
                Core::Audio::Listener listener;
                listener.Position = m_Camera.Position;
                listener.Forward = m_Camera.Front;
                listener.Up = m_Camera.Up;
                listener.TargetReverb = Core::Audio::ReverbPresetType::None;
                Core::Audio::AudioManager::Instance().SetListener(listener);
                m_AudioStarted = true;
                SIMPLE_LOG("Audio listener initialized (playback handled by scripts)");
            } catch (const std::exception& e) {
                SIMPLE_LOG("ERROR: Exception setting up audio: " + std::string(e.what()));
                m_AudioStarted = false;
            } catch (...) {
                SIMPLE_LOG("ERROR: Unknown exception setting up audio");
                m_AudioStarted = false;
            }
        }
    }

    // Sync camera position from physics body (retaining head-mount logic)
    if (m_TextureManager.IsReady() && m_SceneInitialized && m_PlayerEntity != 0) {
        try {
            if (m_Registry.Has<Physics::RigidBody>(m_PlayerEntity)) {
                auto& PlayerRB = m_Registry.Get<Physics::RigidBody>(m_PlayerEntity);
                // Validate position is finite
                if (std::isfinite(PlayerRB.Position.x) && std::isfinite(PlayerRB.Position.y) && std::isfinite(PlayerRB.Position.z)) {
                float HeadOffset = m_CurrentCapsuleHeight * 0.5f - 0.1f;
                m_Camera.Position = PlayerRB.Position + Vec3(0, HeadOffset, 0);
                m_Camera.WorldUp = Vec3(0, 1, 0);
                m_Camera.ProcessMouseMovement(0, 0, true);
                }
            }
        } catch (const std::exception& e) {
            SIMPLE_LOG("ERROR: Exception updating camera: " + std::string(e.what()));
        } catch (...) {
            SIMPLE_LOG("ERROR: Unknown exception updating camera");
        }
    }

    // Update Audio Listener (only if audio is started)
    // Scripts handle audio source updates, we just update the listener position
    if (m_AudioStarted) {
        try {
            Core::Audio::Listener Listener;
            Listener.Position = m_Camera.Position;
            Listener.Forward = m_Camera.Front;
            Listener.Up = m_Camera.Up;
            Listener.TargetReverb = Core::Audio::ReverbPresetType::None;
            Core::Audio::AudioManager::Instance().SetListener(Listener);
        } catch (const std::exception& e) {
            SIMPLE_LOG("ERROR: Exception setting audio listener: " + std::string(e.what()));
        } catch (...) {
            SIMPLE_LOG("ERROR: Unknown exception setting audio listener");
        }
    }

    // Update snow particles (only if scene is ready)
    if (m_SnowParticles && m_SceneInitialized) {
        try {
        Vec3 WindDir = Vec3(0.5f, 0.0f, 0.8f).Normalized();
            m_SnowParticles->UpdateWithWind(DeltaTime, m_Camera.Position, WindDir);
        } catch (const std::exception& e) {
            SIMPLE_LOG("ERROR: Exception updating snow particles: " + std::string(e.what()));
        } catch (...) {
            SIMPLE_LOG("ERROR: Unknown exception updating snow particles");
        }
    }

    // Update bodycam HUD
    m_BodycamHUD.Update(DeltaTime);

    // Update footsteps
    UpdateFootsteps(DeltaTime);
}

void BlizzardGame::UpdateFootsteps(float DeltaTime) {
    // Don't play footsteps during loading
    if (!m_TextureManager.IsReady() || !m_SceneInitialized) {
        return;
    }

    // Check if player exists and has movement component
    if (m_PlayerEntity == 0 || !m_Registry.Has<Game::FPSMovement>(m_PlayerEntity)) {
        return;
    }

    auto& Movement = m_Registry.Get<Game::FPSMovement>(m_PlayerEntity);

    // Check if player is actually moving and grounded
    Vec3 currentPos = m_Camera.Position;
    Vec3 movement = currentPos - m_LastPosition;
    float movementDistance = movement.Magnitude();

    bool isMoving = movementDistance > 0.01f && Movement.MoveDirection.Magnitude() > 0.01f;
    bool isOnGround = Movement.IsGrounded;

    // Only play footsteps when actually moving on ground
    if (isMoving && isOnGround) {
        m_LastFootstepTime += DeltaTime;
        // Only play if enough time has passed since last footstep
        if (m_LastFootstepTime >= m_FootstepInterval) {
            try {
                auto emitter = Core::Audio::AudioManager::Instance().CreateEmitter(
                    "assets/footsteps.wav", currentPos, 20.0f, false);
                if (emitter != 0) {
                    Core::Audio::AudioManager::Instance().SetEmitterVolume(emitter, 0.2f);
                    Core::Audio::AudioManager::Instance().SetEmitterFlags(emitter, false, false, 0);
                }
            } catch (const std::exception& e) {
                SIMPLE_LOG("ERROR: Exception playing footstep sound: " + std::string(e.what()));
            } catch (...) {
                SIMPLE_LOG("ERROR: Unknown exception playing footstep sound");
            }

            // Reset timer to prevent repeated playing
            m_LastFootstepTime = 0.0f;
        }
    } else {
        // Reset timer when not moving or not on ground
        m_LastFootstepTime = 0.0f;
    }

    m_LastPosition = currentPos;
    m_WasOnGround = isOnGround;
}

void BlizzardGame::ProcessPendingTextures() {
    // Process pending texture data (create bgfx textures on main thread)
    // CRITICAL: Must be called from Render() to ensure valid frame context
    if (m_TextureManager.ProcessPendingTextures()) {
        m_FirstFrameRendered = true;
    }
}

void BlizzardGame::Render() {
    // Begin new ImGui frame immediately
    if (UISystem::Instance().IsInitialized()) {
        UISystem::Instance().NewFrame();
    } else {
        SIMPLE_LOG("WARNING: UISystem not initialized in BlizzardGame::Render");
    }

    // Process pending textures in frame context
    ProcessPendingTextures(); // Process one texture per frame

    // Hide main menu if loading screen is active (prevent overlap)
    // Loading is complete when textures are ready AND scene is initialized
    bool isUIBusy = !(m_TextureManager.IsReady() && m_SceneInitialized);

    if (isUIBusy && m_MainMenu) {
        m_MainMenu->Hide();
    }

    // Begin base FPSGame rendering (handles HUD, loading screen, etc.)
    FPSGame::Render();

    auto* window = GetWindow();
    if (!window) return;
    auto fbSize = window->GetFramebufferSize();
    float aspect = static_cast<float>(fbSize.first) / static_cast<float>(std::max(1, fbSize.second));

    // Only render scene if textures are ready and scene is initialized
    bool canRenderScene = m_TextureManager.IsReady() && m_SceneInitialized;

    if (canRenderScene) {
        try {
            if (m_Scene.GetObjectCount() > 0) {
                m_Scene.UpdateLODs(m_Camera.Position);
            }

            // Render scene
            if (m_Renderer) {
                Vec4 clearColor(0.5f, 0.5f, 0.55f, 1.0f);
                m_Renderer->Clear(clearColor);

                if (m_Scene.GetObjectCount() > 0) {
                m_Renderer->RenderScene(m_Scene, m_Camera, m_Lights);
                } else {
                    SIMPLE_LOG("WARNING: Scene has no objects to render");
                }
            } else {
                SIMPLE_LOG("WARNING: Renderer is null, cannot render scene");
            }

            // Render snow particles (only if there are active particles)
            if (m_SnowParticles && m_Renderer && m_SnowParticles->GetActiveParticleCount() > 0) {
                try {
                Math::Matrix4 view = m_Camera.GetViewMatrix();
                Math::Matrix4 proj = Math::Matrix4::Perspective(
                    m_Camera.GetZoom() * 0.0174533f, aspect, 0.1f, 1000.0f);
                Math::Matrix4 viewProj = proj * view;
                m_SnowParticles->Render(2, viewProj, m_Camera.Right, m_Camera.Up);
                } catch (const std::exception& e) {
                    SIMPLE_LOG("ERROR: Exception rendering snow particles: " + std::string(e.what()));
                } catch (...) {
                    SIMPLE_LOG("ERROR: Unknown exception rendering snow particles");
                }
            }
        } catch (const std::exception& e) {
            SIMPLE_LOG("ERROR: Exception during scene rendering: " + std::string(e.what()));
        } catch (...) {
            SIMPLE_LOG("ERROR: Unknown exception during scene rendering");
        }
    } else if (m_Renderer) {
        m_Renderer->Clear(Vec4(0.04f, 0.04f, 0.06f, 1.0f));
    }

    // Overlay loading screen if textures are not ready
    if (m_LoadingScreen && isUIBusy) {
        m_LoadingScreen->SetTitle("Blizzard");

        // Update loading screen progress from texture manager
        m_LoadingScreen->SetProgress(m_TextureManager.GetProgress());

        std::string currentOp = m_TextureManager.GetCurrentTextureName();
        if (!currentOp.empty()) {
            currentOp = "GENERATING: " + currentOp;
            } else {
                currentOp = "INITIALIZING...";
        }
        m_LoadingScreen->SetProgressText(currentOp);

        // m_LoadingScreen->Render() will be called by FPSGame::Render() which we called above
    } else if (m_LoadingScreen) {
        m_LoadingScreen->Hide();
    }

    // Only render HUD if textures are ready, scene is initialized, and not showing loading screen
    if (m_TextureManager.IsReady() && m_SceneInitialized && !isUIBusy && UISystem::Instance().IsInitialized()) {
        try {
            // Render bodycam HUD effects (realistic camera flicker)
            m_BodycamHUD.Render();

            // Render minimap and instructions
            RenderMinimap();
            RenderInstructions();
        } catch (const std::exception& e) {
            SIMPLE_LOG("ERROR: Exception during HUD rendering: " + std::string(e.what()));
        } catch (...) {
            SIMPLE_LOG("ERROR: Unknown exception during HUD rendering");
        }
    }

    // Render ImGui
    if (UISystem::Instance().IsInitialized()) {
        try {
            UISystem::Instance().Render();
        } catch (const std::exception& e) {
            SIMPLE_LOG("ERROR: UISystem::Render failed: " + std::string(e.what()));
        } catch (...) {
            SIMPLE_LOG("ERROR: UISystem::Render failed with unknown error");
        }
    }

    // Present frame
    if (m_Renderer) {
        m_Renderer->Present();
    }

    // Mark that first frame has been rendered
    if (!m_FirstFrameRendered) {
        m_FirstFrameRendered = true;
        SIMPLE_LOG("First frame rendered - BGFX is now ready for texture creation");
    }
}


void BlizzardGame::RenderMinimap() {
    // Safety checks
    if (!ImGui::GetCurrentContext()) return;
    ImVec2 ScreenSize = ImGui::GetIO().DisplaySize;
    if (ScreenSize.x <= 0.0f || ScreenSize.y <= 0.0f) return;

    // Minimap in top-right corner
    float MinimapSize = 150.0f;
    float MinimapPadding = 20.0f;
    ImVec2 MinimapPos(ScreenSize.x - MinimapSize - MinimapPadding, MinimapPadding);

    ImGui::SetNextWindowPos(MinimapPos);
    ImGui::SetNextWindowSize(ImVec2(MinimapSize, MinimapSize));
    ImGui::Begin("Minimap", nullptr,
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoInputs);

    ImDrawList* MinimapDrawListPtr = ImGui::GetWindowDrawList();
    ImVec2 MinimapMin = ImGui::GetWindowPos();
    ImVec2 MinimapMax = ImVec2(MinimapMin.x + MinimapSize, MinimapMin.y + MinimapSize);
    ImVec2 MinimapCenter = ImVec2(MinimapMin.x + MinimapSize * 0.5f, MinimapMin.y + MinimapSize * 0.5f);

    // Draw semi-transparent background
    MinimapDrawListPtr->AddRectFilled(
        MinimapMin,
        MinimapMax,
        IM_COL32(0, 0, 0, 180) // Semi-transparent black (alpha ~0.7)
    );

    // Draw orange border
    MinimapDrawListPtr->AddRect(
        MinimapMin,
        MinimapMax,
        IM_COL32(255, 165, 0, 255), // Orange
        0.0f, 0, 2.0f // Thickness 2
    );

    // Draw player as orange circle at center
    float PlayerRadius = 4.0f;
    MinimapDrawListPtr->AddCircleFilled(
        MinimapCenter,
        PlayerRadius,
        IM_COL32(255, 165, 0, 255) // Orange
    );

    // Draw orientation arrow (orange line pointing in camera forward direction)
    // Project camera forward to 2D (XZ plane, Y is up)
    Math::Vec3 Forward2D = Math::Vec3(m_Camera.Front.x, 0.0f, m_Camera.Front.z);
    Forward2D = Forward2D.Normalized();
    float ArrowLength = 25.0f;
    ImVec2 ArrowEnd = ImVec2(
        MinimapCenter.x + Forward2D.x * ArrowLength,
        MinimapCenter.y + Forward2D.z * ArrowLength
    );

    // Draw arrow line
    MinimapDrawListPtr->AddLine(
        MinimapCenter,
        ArrowEnd,
        IM_COL32(255, 165, 0, 255), // Orange
        2.0f // Thickness
    );

    // Draw arrow head (triangle)
    Math::Vec3 Right2D = Math::Vec3(m_Camera.Right.x, 0.0f, m_Camera.Right.z);
    Right2D = Right2D.Normalized();
    float ArrowHeadSize = 6.0f;
    ImVec2 ArrowTip = ArrowEnd;
    ImVec2 ArrowLeft = ImVec2(
        ArrowEnd.x - Forward2D.x * ArrowHeadSize + Right2D.x * ArrowHeadSize * 0.5f,
        ArrowEnd.y - Forward2D.z * ArrowHeadSize + Right2D.z * ArrowHeadSize * 0.5f
    );
    ImVec2 ArrowRight = ImVec2(
        ArrowEnd.x - Forward2D.x * ArrowHeadSize - Right2D.x * ArrowHeadSize * 0.5f,
        ArrowEnd.y - Forward2D.z * ArrowHeadSize - Right2D.z * ArrowHeadSize * 0.5f
    );
    MinimapDrawListPtr->AddTriangleFilled(
        ArrowTip,
        ArrowLeft,
        ArrowRight,
        IM_COL32(255, 165, 0, 255) // Orange
    );

    ImGui::End();

    // Optional: FPS display (subtle, below minimap)
    float CurrentFPS = GetCurrentFPS();
    if (CurrentFPS > 0.0f) {
        ImGui::SetNextWindowPos(ImVec2(ScreenSize.x - 100, MinimapPadding + MinimapSize + 10));
        ImGui::SetNextWindowSize(ImVec2(90, 30));
        ImGui::Begin("FPS", nullptr,
            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoInputs);
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 0.5f), "%.0f FPS", CurrentFPS);
        ImGui::End();
    }
}

void BlizzardGame::RenderInstructions() {
    // Safety checks
    if (!ImGui::GetCurrentContext()) return;

    // On-screen instructions when mouse is not locked
    if (!m_MouseLocked) {
        ImVec2 ScreenSize = ImGui::GetIO().DisplaySize;
        if (ScreenSize.x <= 0.0f || ScreenSize.y <= 0.0f) return;
        ImGui::SetNextWindowPos(ImVec2(ScreenSize.x * 0.5f - 150, ScreenSize.y * 0.5f - 80), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(300, 160));
        ImGui::Begin("Instructions", nullptr,
            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoInputs);
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.9f), "Right Click to Lock Mouse");
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 0.8f), "WASD/ZQSD - Move");
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 0.8f), "Space - Jump");
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 0.8f), "Ctrl - Slide");
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 0.8f), "Shift - Sprint");
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 0.8f), "ESC - Unlock");
        ImGui::End();
    }
}

} // namespace Solstice::Blizzard
