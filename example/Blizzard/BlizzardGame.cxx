#include "BlizzardGame.hxx"
#include <Entity/Transform.hxx>
#include <Game/FPS/FPSMovement.hxx>
#include <Game/FPS/Weapon.hxx>
#include <Game/Gameplay/Health.hxx>
#include <UI/Core/UISystem.hxx>
#include <Render/Assets/Mesh.hxx>
#include <Material/Material.hxx>
#include <Arzachel/ProceduralTexture.hxx>
#include <Arzachel/MaterialPresets.hxx>
#include <Arzachel/MeshFactory.hxx>
#include <Arzachel/GeometryOps.hxx>
#include <Arzachel/Polyhedra.hxx>
#include <Arzachel/AssetPresets.hxx>
#include <Arzachel/AssetBuilder.hxx>
#include <Arzachel/TerrainGenerator.hxx>
#include <Arzachel/Seed.hxx>
#include <Arzachel/MeshData.hxx>
#include <Arzachel/Generator.hxx>
#include <Asset/Loading/AssetLoader.hxx>
#include <Render/Scene/Skybox.hxx>
#include <Render/Scene/AuthoringSkyboxApply.hxx>
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
#include <cstdio>
#include <limits>
#include <filesystem>
#include <array>

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

namespace {

constexpr uint32_t InvalidMaterialID = 0xFFFFFFFFu;

uint32_t MaterialID(Core::MaterialHandle Handle) {
    return Handle.IsValid() ? Handle.GetValue() : InvalidMaterialID;
}

Core::Material MakeMaterial(const Math::Vec3& Color, float Roughness, float Metallic = 0.0f,
                            const Math::Vec3& Emission = Math::Vec3(0.0f, 0.0f, 0.0f),
                            float EmissionStrength = 0.0f) {
    Core::Material Mat;
    Mat.SetAlbedoColor(Color, Roughness);
    Mat.Metallic = static_cast<uint8_t>(std::clamp(Metallic, 0.0f, 1.0f) * 255.0f);
    Mat.SetEmission(Emission, EmissionStrength);
    Mat.ShadingModel = static_cast<uint8_t>(Core::ShadingModel::PhysicallyBased);
    Mat.Flags = Core::MaterialFlag_CastsShadows | Core::MaterialFlag_ReceivesShadows;
    if (EmissionStrength > 0.0f) {
        Mat.Flags |= Core::MaterialFlag_HasEmission;
    }
    return Mat;
}

} // namespace

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
    Core::AssetLoader::SetAssetPath("assets");

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

    // No ground exists until async textures finish and InitializeScene() runs; disable gravity
    // so the player does not free-fall during loading.
    if (m_PlayerEntity != 0 && m_Registry.Has<Physics::RigidBody>(m_PlayerEntity)) {
        m_Registry.Get<Physics::RigidBody>(m_PlayerEntity).GravityScale = 0.0f;
        Physics::PhysicsSystem::Instance().GetBridge().SyncToReactPhysics3D();
    }

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
            m_ActiveWindDirection = Vec3(GetFloat(Args[0]), GetFloat(Args[1]), GetFloat(Args[2])).Normalized();
            m_SnowParticles->SetWindDirection(m_ActiveWindDirection);
        }
        if (Args.size() >= 4 && m_SnowParticles) {
            m_ActiveWindStrength = GetFloat(Args[3]);
            m_SnowParticles->SetWindStrength(m_ActiveWindStrength);
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
    if (m_Renderer->GetPostProcessing()) {
        m_Renderer->GetPostProcessing()->SetHDRExposure(0.28f);
    }

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
    SIMPLE_LOG("Blizzard: initializing .smat material set");
    InitializeBlizzardMaterials();
    SIMPLE_LOG("Blizzard: .smat material set ready");

    // Initialize procedural textures (async - will show loading screen)
    SIMPLE_LOG("Blizzard: initializing procedural textures");
    InitializeProceduralTextures();
    SIMPLE_LOG("Blizzard: procedural textures queued");

    // Initialize skybox (doesn't depend on procedural textures)
    InitializeSkybox();

    // Initialize audio
    InitializeAudio();

    // Initialize snow particle system using preset.
    // The blizzard wants visually dense coverage in every direction the camera looks. We use
    // a smaller spawn radius (so each particle is meaningfully close to the camera and
    // contributes to perceived density) plus a shorter lifetime + higher spawn rate so the
    // distribution refreshes evenly rather than clumping near the camera.
    try {
        Render::SnowParticleConfig SnowConfig;
        SnowConfig.MaxParticles = 30000;
        SnowConfig.SpawnRate = 4500.0f;
        SnowConfig.MaxDistance = 110.0f;
        SnowConfig.BaseSpawnRadius = 70.0f;
        SnowConfig.SpawnHeight = 50.0f;
        SnowConfig.WindDirection = Math::Vec3(0.8f, 0.0f, 1.0f).Normalized();
        SnowConfig.WindStrength = 6.0f;
        SnowConfig.Density = 1.5f;
        SnowConfig.LifeMin = 6.0f;
        SnowConfig.LifeMax = 14.0f;
        SnowConfig.FallSpeed = 2.6f;
        SnowConfig.SizeMin = 0.05f;
        SnowConfig.SizeMax = 0.16f;
        m_ActiveWindDirection = SnowConfig.WindDirection;
        m_ActiveWindStrength = SnowConfig.WindStrength;
        m_SnowParticles = Render::ParticlePresets::CreateSnowParticleSystem(SnowConfig);
        m_SnowParticles->SetSpawnBudgetPerFrame(3000);
        m_SnowParticles->SetWindDirection(SnowConfig.WindDirection);
        m_SnowParticles->SetWindStrength(SnowConfig.WindStrength);
        m_SnowParticles->SetDensity(SnowConfig.Density);
    } catch (...) {
        SIMPLE_LOG("ERROR: Failed to initialize snow particle system");
    }

    // Keep snow present for footsteps/sink feedback without dragging walk speed below design targets.
    m_SnowSystem.SetBaseDepth(0.03f);
    m_SnowSystem.SetDepthVariation(0.04f);
    m_SnowSystem.SetMaxDepth(0.12f);
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
    m_ShowWeaponHUD = true;
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

    // Capsule center for feet on flat ground at Y=0.
    const float SpawnHeight = (m_NormalCapsuleHeight * 0.5f) + m_CapsuleRadius + 0.02f;
    Math::Vec3 SpawnPosition = Math::Vec3(0.0f, SpawnHeight, 0.0f);

    // Bodycam tactical pace: 5 m/s walk, 10 m/s double-tap forward sprint.
    if (m_Registry.Has<Game::FPSMovement>(m_PlayerEntity)) {
        auto& Movement = m_Registry.Get<Game::FPSMovement>(m_PlayerEntity);
        Movement.MoveSpeed = m_WalkSpeed;
        Movement.SprintMultiplier = m_SprintSpeed / m_WalkSpeed;
        Movement.MaxGroundSpeed = m_WalkSpeed;
        Movement.MaxAirSpeed = m_SprintSpeed;
        Movement.GroundAcceleration = 42.0f;
        Movement.GroundFriction = 7.5f;
        Movement.AirAcceleration = 8.0f;
        Movement.EnableBunnyHopping = false;
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
    m_LastFootstepBodyPos = SpawnPosition;
}

void BlizzardGame::InitializeWeapons() {
    FPSGame::InitializeWeapons();
    if (m_PlayerEntity == 0) {
        return;
    }

    Game::Weapon Rifle;
    Rifle.Type = Game::WeaponType::Rifle;
    Rifle.Name = "ARCTIC CARBINE";
    Rifle.Damage = 34.0f;
    Rifle.Range = 120.0f;
    Rifle.FireRate = 8.0f;
    Rifle.CurrentAmmo = 30;
    Rifle.MaxAmmo = 30;
    Rifle.ReserveAmmo = 90;
    Rifle.MaxReserveAmmo = 120;
    Rifle.ReloadTime = 1.7f;
    Rifle.BaseSpread = 0.015f;
    Rifle.MaxSpread = 0.08f;
    Rifle.SpreadIncrease = 0.008f;
    Rifle.SpreadDecayRate = 0.08f;

    if (m_Registry.Has<Game::Weapon>(m_PlayerEntity)) {
        m_Registry.Get<Game::Weapon>(m_PlayerEntity) = Rifle;
    } else {
        m_Registry.Add<Game::Weapon>(m_PlayerEntity, Rifle);
    }
}

void BlizzardGame::HandleInput() {
    // Input is primarily handled through callbacks
}

void BlizzardGame::UpdateFPSMovement(float DeltaTime) {
    if (m_PlayerEntity == 0) return;

    Math::Vec3 MoveDir(0, 0, 0);
    bool Jump = false;
    bool Crouch = false;
    bool ForwardHeld = false;

    if (m_ForwardTapTimer > 0.0f) {
        m_ForwardTapTimer = std::max(0.0f, m_ForwardTapTimer - DeltaTime);
    }

    if (m_InputManager) {
        const bool ForwardPressed = m_InputManager->IsKeyPressed(26) || m_InputManager->IsKeyPressed(25);
        const bool ForwardJustPressed = m_InputManager->IsKeyJustPressed(26) || m_InputManager->IsKeyJustPressed(25);
        ForwardHeld = ForwardPressed;

        if (ForwardPressed) MoveDir.z += 1.0f;
        if (m_InputManager->IsKeyPressed(22)) MoveDir.z -= 1.0f;
        if (m_InputManager->IsKeyPressed(4) || m_InputManager->IsKeyPressed(20)) MoveDir.x -= 1.0f;
        if (m_InputManager->IsKeyPressed(7)) MoveDir.x += 1.0f;
        Jump = m_InputManager->IsKeyPressed(44);
        Crouch = m_InputManager->IsKeyPressed(29);

        if (ForwardJustPressed) {
            m_DoubleTapSprintActive = (m_ForwardTapTimer > 0.0f);
            m_ForwardTapTimer = m_ForwardDoubleTapWindow;
        }
    }

    if (!ForwardHeld) {
        m_DoubleTapSprintActive = false;
    }

    const bool Sprint = m_DoubleTapSprintActive && ForwardHeld && MoveDir.z > 0.0f;
    Game::FPSMovementSystem::ProcessInput(m_Registry, m_PlayerEntity, MoveDir, Jump, Crouch, Sprint);
    if (m_Registry.Has<Game::FPSMovement>(m_PlayerEntity)) {
        auto& Movement = m_Registry.Get<Game::FPSMovement>(m_PlayerEntity);
        Movement.MaxGroundSpeed = m_WalkSpeed;
        Movement.MaxAirSpeed = m_SprintSpeed;
        Movement.MoveSpeed = m_WalkSpeed;
        Movement.SprintMultiplier = m_SprintSpeed / m_WalkSpeed;
    }

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

        if ((Key == 'r' || Key == 'R') && m_PlayerEntity != 0 && m_Registry.Has<Game::Weapon>(m_PlayerEntity)) {
            auto& Weapon = m_Registry.Get<Game::Weapon>(m_PlayerEntity);
            const int Needed = Weapon.MaxAmmo - Weapon.CurrentAmmo;
            const int ToLoad = std::min(Needed, Weapon.ReserveAmmo);
            if (ToLoad > 0) {
                Weapon.CurrentAmmo += ToLoad;
                Weapon.ReserveAmmo -= ToLoad;
            }
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

    if (Button == 0 && Action == 1 && m_MouseLocked && m_PlayerEntity != 0) { // Left mouse button
        if (m_Registry.Has<Game::Weapon>(m_PlayerEntity)) {
            auto& Weapon = m_Registry.Get<Game::Weapon>(m_PlayerEntity);
            if (Weapon.CurrentAmmo > 0 && m_WeaponCooldown <= 0.0f) {
                Weapon.CurrentAmmo--;
                Weapon.IsFiring = true;
                m_PendingShot = true;
                m_WeaponCooldown = 1.0f / std::max(1.0f, Weapon.FireRate);
            }
        }
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

void BlizzardGame::InitializeBlizzardMaterials() {
    // Always register the in-memory material rather than loading a possibly stale .smat from
    // disk. Materials still get serialised to disk so external tools can inspect them, but
    // we treat the in-source definition as the source of truth so colour/roughness tweaks
    // and procedural texture bindings always take effect on the next run.
    auto CreateSmatMaterial = [this](const std::string& Path, const Core::Material& Mat) {
        try {
            std::filesystem::path MaterialPath(Path);
            if (!MaterialPath.parent_path().empty()) {
                std::filesystem::create_directories(MaterialPath.parent_path());
            }
            Arzachel::MaterialSerializer::SaveToFile(Path, Mat);
        } catch (const std::exception& E) {
            SIMPLE_LOG("WARNING: Failed to write Blizzard material " + Path + ": " + std::string(E.what()));
        }
        const uint32_t MaterialID = m_MaterialLibrary->AddMaterial(Mat);
        return Core::MaterialHandle(MaterialID);
    };

    m_PaintedMetalMaterialHandle = CreateSmatMaterial(
        "assets/materials/painted_metal.smat",
        MakeMaterial(Math::Vec3(0.46f, 0.49f, 0.51f), 0.62f, 0.45f));
    m_ContainerMaterialHandle = CreateSmatMaterial(
        "assets/materials/container.smat",
        MakeMaterial(Math::Vec3(0.52f, 0.14f, 0.10f), 0.74f, 0.30f));
    m_ConcreteMaterialHandle = CreateSmatMaterial(
        "assets/materials/concrete.smat",
        MakeMaterial(Math::Vec3(0.66f, 0.66f, 0.64f), 0.92f, 0.04f));
    m_WoodMaterialHandle = CreateSmatMaterial(
        "assets/materials/frozen_wood.smat",
        MakeMaterial(Math::Vec3(0.36f, 0.26f, 0.17f), 0.86f, 0.02f));
    m_UtilityYellowMaterialHandle = CreateSmatMaterial(
        "assets/materials/utility_yellow.smat",
        MakeMaterial(Math::Vec3(0.78f, 0.60f, 0.18f), 0.62f, 0.20f));
    m_DarkRoofMaterialHandle = CreateSmatMaterial(
        "assets/materials/dark_roof.smat",
        MakeMaterial(Math::Vec3(0.10f, 0.11f, 0.12f), 0.78f, 0.30f));
    m_RustedMetalMaterialHandle = CreateSmatMaterial(
        "assets/materials/rusted_metal.smat",
        MakeMaterial(Math::Vec3(0.34f, 0.21f, 0.14f), 0.84f, 0.55f));
    m_PlasticMaterialHandle = CreateSmatMaterial(
        "assets/materials/plastic.smat",
        MakeMaterial(Math::Vec3(0.55f, 0.52f, 0.48f), 0.42f, 0.05f));
    // Glowing windows act as small emissive accents to break up the cold flat lighting.
    m_GlowingWindowMaterialHandle = CreateSmatMaterial(
        "assets/materials/glowing_window.smat",
        MakeMaterial(Math::Vec3(0.95f, 0.74f, 0.32f), 0.36f, 0.0f,
                     Math::Vec3(1.0f, 0.78f, 0.36f), 0.55f));
    m_EnemyMaterialHandle = CreateSmatMaterial(
        "assets/materials/target_orange.smat",
        MakeMaterial(Math::Vec3(0.9f, 0.24f, 0.08f), 0.55f, 0.0f,
                     Math::Vec3(1.0f, 0.18f, 0.03f), 0.25f));
}

void BlizzardGame::InitializeProceduralTextures() {
    using namespace Solstice::Arzachel;


    // Texture indices are tightly coupled to MaterialAssignments below: assignment[I] receives
    // m_Textures[I] as its primary albedo. Detail/blend slots reference textures by their
    // m_Textures index. Reordering this list requires updating FinalizeProceduralTextures.
    std::vector<Game::TextureConfig> TextureConfigs;

    // Indices 0-3: terrain base albedos — picked up by terrain materials in the same order.
    TextureConfigs.push_back({"Snow",    512, 7,  6.0f, 10000, []() { return MaterialPresets::SnowData(Arzachel::Seed(10000)); }});
    TextureConfigs.push_back({"Rock",    512, 7,  8.0f, 15000, []() { return MaterialPresets::RockData(Arzachel::Seed(15000)); }});
    TextureConfigs.push_back({"Terrain", 512, 7,  4.0f, 20000, []() { return ProceduralTexture::GenerateNoise(512, 7, 4.0f, Arzachel::Seed(20000)); }});
    TextureConfigs.push_back({"Ice",     512, 8, 12.0f, 22000, []() { return MaterialPresets::IceData(Arzachel::Seed(22000)); }});

    // Indices 4-12: prop albedos — one per architectural material. Different seeds give
    // each surface its own grain, panel layout, and weathering pattern even when sharing a preset.
    TextureConfigs.push_back({"Concrete",        512, 6,  8.0f, 41000, []() { return MaterialPresets::ConcreteData(Arzachel::Seed(41000)); }});
    TextureConfigs.push_back({"Painted Metal",   512, 5,  6.0f, 42000, []() { return MaterialPresets::MetalData(Arzachel::Seed(42000)); }});
    TextureConfigs.push_back({"Rusted Metal",    512, 5,  6.0f, 42500, []() { return MaterialPresets::MetalData(Arzachel::Seed(42500)); }});
    TextureConfigs.push_back({"Container Steel", 512, 5,  6.0f, 42800, []() { return MaterialPresets::MetalData(Arzachel::Seed(42800)); }});
    TextureConfigs.push_back({"Wood",            512, 6,  5.0f, 43000, []() { return MaterialPresets::WoodData(Arzachel::Seed(43000)); }});
    TextureConfigs.push_back({"Yellow Paint",    512, 5,  6.0f, 43500, []() { return MaterialPresets::PlasticData(Arzachel::Seed(43500)); }});
    TextureConfigs.push_back({"Roof Asphalt",    512, 6,  4.0f, 43800, []() { return MaterialPresets::ConcreteData(Arzachel::Seed(43800)); }});
    TextureConfigs.push_back({"Plastic",         512, 5,  6.0f, 44000, []() { return MaterialPresets::PlasticData(Arzachel::Seed(44000)); }});
    TextureConfigs.push_back({"Glow Glass",      256, 5,  8.0f, 44400, []() { return MaterialPresets::GlassData(Arzachel::Seed(44400), 256); }});

    // Indices 13-14: shared overlays. Frost is sprinkled onto every prop's roughness/blend
    // slot to break up flat surfaces. Dirt acts as a darker streak overlay.
    TextureConfigs.push_back({"Frost Overlay", 256, 6, 14.0f, 45000, []() { return MaterialPresets::FrostData(Arzachel::Seed(45000), 256); }});
    TextureConfigs.push_back({"Dirt Streaks",  256, 5,  4.0f, 45400, []() { return MaterialPresets::DirtData(Arzachel::Seed(45400), 256); }});

    // Indices 15-17: terrain detail textures (referenced from terrain assignments).
    TextureConfigs.push_back({"Detail Snow", 128, 6, 16.0f, 25000, []() { return MaterialPresets::FrostData(Arzachel::Seed(25000), 128); }});
    TextureConfigs.push_back({"Detail Ice",  128, 7, 20.0f, 26000, []() { return ProceduralTexture::GenerateNoise(128, 10, 15.0f, Arzachel::Seed(26000)); }});
    TextureConfigs.push_back({"Detail Rock", 128, 6, 18.0f, 27000, []() { return ProceduralTexture::GenerateNoise(128,  6, 12.0f, Arzachel::Seed(27000)); }});

    // Indices 18-21: terrain blend masks (large-scale variation maps).
    TextureConfigs.push_back({"Blend Snow",    512, 5, 6.0f, 28000, []() { return ProceduralTexture::GenerateNoise(512, 4, 2.0f, Arzachel::Seed(28000)); }});
    TextureConfigs.push_back({"Blend Ice",     512, 5, 6.0f, 29000, []() { return ProceduralTexture::GenerateNoise(512, 4, 2.0f, Arzachel::Seed(29000)); }});
    TextureConfigs.push_back({"Blend Rock",    512, 5, 6.0f, 30000, []() { return ProceduralTexture::GenerateNoise(512, 4, 2.0f, Arzachel::Seed(30000)); }});
    TextureConfigs.push_back({"Blend Terrain", 512, 5, 6.0f, 31000, []() { return ProceduralTexture::GenerateNoise(512, 4, 2.0f, Arzachel::Seed(31000)); }});

    m_TextureManager.Initialize(TextureConfigs, "cache");
}

void BlizzardGame::InitializeSkybox() {
    m_Skybox = std::make_unique<Skybox>();
    m_Skybox->Initialize(512); // 512x512 cubemap faces

    // Set overcast preset for blizzard atmosphere
    m_Skybox->SetPreset(Render::SkyPreset::Overcast);
    // Dark, clean winter overcast. Keep procedural clouds restrained; high density tiles badly
    // across cube faces and reads like a low ceiling texture instead of distant sky.
    m_Skybox->SetHorizonColor(Math::Vec3(0.38f, 0.42f, 0.48f));
    m_Skybox->SetZenithColor(Math::Vec3(0.16f, 0.18f, 0.22f));
    m_Skybox->SetSunIntensity(0.03f);
    m_Skybox->SetCloudDensity(0.28f);
    m_Skybox->SetAtmosphereThickness(0.85f);
    m_Skybox->Regenerate();

    // Set skybox in renderer
    if (m_Renderer) {
        m_Renderer->SetSkybox(m_Skybox.get());
    }
}

void BlizzardGame::InitializeScene() {
    InitializeTerrain();
    InitializeArcticBase();
    InitializeImportedProps();
    InitializeCombatTargets();

    // Update scene transforms after adding all objects
    m_Scene.UpdateTransforms();
    Physics::PhysicsSystem::Instance().GetBridge().SyncToReactPhysics3D();

    ResetPlayerToBlizzardSpawn();

    SIMPLE_LOG("Blizzard: Scene initialized");
}

void BlizzardGame::ResetPlayerToBlizzardSpawn() {
    const float SpawnY = (m_NormalCapsuleHeight * 0.5f) + m_CapsuleRadius + 0.02f;
    Math::Vec3 SpawnPos(m_SafeSpawnPosition.x, SpawnY, m_SafeSpawnPosition.z);
    if (m_PlayerEntity == 0 || !m_Registry.Has<Physics::RigidBody>(m_PlayerEntity)) {
        return;
    }
    auto& RB = m_Registry.Get<Physics::RigidBody>(m_PlayerEntity);
    RB.Position = SpawnPos;
    RB.Velocity = Math::Vec3(0, 0, 0);
    RB.AngularVelocity = Math::Vec3(0, 0, 0);
    RB.GravityScale = 1.0f;
    if (m_Registry.Has<ECS::Transform>(m_PlayerEntity)) {
        m_Registry.Get<ECS::Transform>(m_PlayerEntity).Position = SpawnPos;
    }
    Physics::PhysicsSystem::Instance().GetBridge().SyncToReactPhysics3D();
    float HeadOffset = m_CameraHeight - ((m_CurrentCapsuleHeight * 0.5f) + m_CapsuleRadius);
    m_Camera.Position = SpawnPos + Math::Vec3(0, HeadOffset, 0);
    m_Camera.Yaw = 90.0f;
    m_Camera.Pitch = -2.0f;
    m_Camera.ProcessMouseMovement(0, 0, true);
    m_LastPosition = m_Camera.Position;
    m_LastFootstepBodyPos = SpawnPos;
}

void BlizzardGame::InitializeTerrain() {
    Game::TerrainConfig Config;
    Config.Resolution = 64;
    Config.TerrainSize = 200.0f; // Match structure placement / SceneBuilder
    // Keep flat: SceneBuilder terrain physics is a single thick box; non-zero height would
    // mismatch the AABB. Undulation is visual-only elsewhere (structures, snow, sky).
    Config.HeightScale = 0.0f;
    Config.Seed = 12345;
    Config.LakeMaterialID = m_IceMaterialHandle.IsValid() ? m_IceMaterialHandle.GetValue() : 0xFFFFFFFF;
    Config.TerrainMaterialID = m_TerrainMaterialHandle.IsValid() ? m_TerrainMaterialHandle.GetValue() : 0xFFFFFFFF;
    Config.CreatePhysicsBody = true;
    // Stale `terrain_heightmap.json` from older runs (different HeightScale) desyncs mesh Y from
    // the static ground box; always regenerate for this demo.
    Config.EnableCaching = false;

    m_SceneBuilder.AddTerrain(&m_Scene, m_MeshLibrary.get(), m_MaterialLibrary.get(), &m_Registry, Config);
}

void BlizzardGame::AddStaticBox(const Math::Vec3& Position, const Math::Vec3& Scale, Core::MaterialHandle Material,
                                bool CreateCollision, const Math::Vec3& CollisionHalfExtents) {
    if (!m_MeshLibrary) {
        return;
    }

    auto Mesh = Arzachel::MeshFactory::CreateCube(1.0f);
    if (!Mesh) {
        return;
    }

    if (!Mesh->SubMeshes.empty()) {
        Mesh->SubMeshes[0].MaterialID = MaterialID(Material);
    }

    const uint32_t MeshID = m_MeshLibrary->AddMesh(std::move(Mesh));
    const uint32_t ObjectID = m_Scene.AddObject(
        MeshID,
        Position,
        Math::Quaternion(),
        Scale,
        Render::ObjectType_Static);

    if (CreateCollision) {
        const Math::Vec3 HalfExtents = (CollisionHalfExtents.Magnitude() > 0.0f)
            ? CollisionHalfExtents
            : Scale * 0.5f;
        auto Entity = m_Registry.Create();
        auto& RB = m_Registry.Add<Physics::RigidBody>(Entity);
        RB.Position = Position;
        RB.Rotation = Math::Quaternion();
        RB.IsStatic = true;
        RB.SetMass(0.0f);
        RB.Type = Physics::ColliderType::Box;
        RB.HalfExtents = HalfExtents;
        RB.Friction = 0.85f;
        RB.Restitution = 0.0f;
        RB.RenderObjectID = ObjectID;
    }
}

void BlizzardGame::AddStaticCylinder(const Math::Vec3& Position, float Radius, float Height, Core::MaterialHandle Material,
                                     bool CreateCollision) {
    if (!m_MeshLibrary) {
        return;
    }

    auto Mesh = Arzachel::MeshFactory::CreateCylinder(Radius, Height, 16);
    if (!Mesh) {
        return;
    }

    if (!Mesh->SubMeshes.empty()) {
        Mesh->SubMeshes[0].MaterialID = MaterialID(Material);
    }

    const uint32_t MeshID = m_MeshLibrary->AddMesh(std::move(Mesh));
    const uint32_t ObjectID = m_Scene.AddObject(
        MeshID,
        Position,
        Math::Quaternion(),
        Math::Vec3(1, 1, 1),
        Render::ObjectType_Static);

    if (CreateCollision) {
        auto Entity = m_Registry.Create();
        auto& RB = m_Registry.Add<Physics::RigidBody>(Entity);
        RB.Position = Position;
        RB.Rotation = Math::Quaternion();
        RB.IsStatic = true;
        RB.SetMass(0.0f);
        RB.Type = Physics::ColliderType::Cylinder;
        RB.CylinderRadius = Radius;
        RB.CylinderHeight = Height;
        RB.HalfExtents = Math::Vec3(Radius, Height * 0.5f, Radius);
        RB.Friction = 0.8f;
        RB.Restitution = 0.0f;
        RB.RenderObjectID = ObjectID;
    }
}

void BlizzardGame::AddArzachelProp(const Arzachel::Generator<Arzachel::MeshData>& Gen,
                                   const Math::Vec3& Position,
                                   const Math::Quaternion& Rotation,
                                   const Math::Vec3& Scale,
                                   Core::MaterialHandle Material,
                                   bool CreateCollision,
                                   const Math::Vec3& CollisionHalfExtents,
                                   Physics::ColliderType ColliderType,
                                   uint32_t SeedSalt) {
    if (!m_MeshLibrary) {
        return;
    }
    // Each prop gets a unique derived seed so two props built from the same generator
    // still produce different micro-geometry (Arzachel is purely deterministic on Seed).
    Arzachel::Seed PropSeed = Arzachel::Seed(0xB112ABCDu).Derive(SeedSalt);
    Arzachel::MeshData Data = Gen(PropSeed);
    if (Data.Positions.empty() || Data.Indices.empty()) {
        return;
    }
    Data.CalculateBounds();
    auto Mesh = Arzachel::ConvertToRenderMesh(Data);
    if (!Mesh || Mesh->Vertices.empty() || Mesh->Indices.empty()) {
        return;
    }
    if (Mesh->SubMeshes.empty()) {
        Mesh->AddSubMesh(0, 0, static_cast<uint32_t>(Mesh->Indices.size()));
    }
    for (auto& SM : Mesh->SubMeshes) {
        SM.MaterialID = MaterialID(Material);
    }

    const uint32_t MeshID = m_MeshLibrary->AddMesh(std::move(Mesh));
    const uint32_t ObjectID = m_Scene.AddObject(
        MeshID,
        Position,
        Rotation,
        Scale,
        Render::ObjectType_Static);
    // CRITICAL: SceneRenderer's static-buffer path uses Scene::GetMaterial as an override on
    // top of SubMesh.MaterialID. Without this call the instance material defaults to 0 and
    // every Arzachel prop ends up rendered with the first registered material — which is why
    // the previous build looked like everything shared one texture/colour.
    m_Scene.SetMaterial(ObjectID, MaterialID(Material));

    if (CreateCollision) {
        const Math::Vec3 HalfExtents = (CollisionHalfExtents.Magnitude() > 0.0f)
            ? CollisionHalfExtents
            : Scale * 0.5f;
        auto Entity = m_Registry.Create();
        auto& RB = m_Registry.Add<Physics::RigidBody>(Entity);
        RB.Position = Position;
        RB.Rotation = Rotation;
        RB.IsStatic = true;
        RB.SetMass(0.0f);
        RB.Type = ColliderType;
        RB.HalfExtents = HalfExtents;
        if (ColliderType == Physics::ColliderType::Cylinder) {
            RB.CylinderRadius = std::max(HalfExtents.x, HalfExtents.z);
            RB.CylinderHeight = HalfExtents.y * 2.0f;
        }
        RB.Friction = 0.85f;
        RB.Restitution = 0.0f;
        RB.RenderObjectID = ObjectID;
    }
}

void BlizzardGame::BuildIndustrialBuilding(const Math::Vec3& Center, const Math::Vec3& HalfExtents,
                                           Core::MaterialHandle WallMat, Core::MaterialHandle RoofMat,
                                           Core::MaterialHandle TrimMat, Core::MaterialHandle WindowMat,
                                           bool HasWindowBand, bool HasFloodlight, uint32_t SeedSalt) {
    // A modular, "Blender-style" warehouse: walls + base plinth + cap trim + overhanging roof
    // + optional window band and floodlight. Each piece is its own static mesh so per-mesh
    // materials render correctly.
    using namespace Solstice::Arzachel;
    Generator<MeshData> Box = Cube(1.0f, Seed(SeedSalt));

    // Concrete plinth: wider than the walls, sits at the ground line. Hides the wall/snow seam.
    AddStaticBox(Center + Math::Vec3(0.0f, 0.18f - HalfExtents.y, 0.0f),
                 Math::Vec3(HalfExtents.x * 2.0f + 0.6f, 0.36f, HalfExtents.z * 2.0f + 0.6f),
                 m_ConcreteMaterialHandle, true,
                 Math::Vec3(HalfExtents.x + 0.3f, 0.18f, HalfExtents.z + 0.3f));

    // Body of the building (walls + ceiling), single solid mesh. Collision is just this volume.
    AddArzachelProp(Box, Center, Math::Quaternion(), HalfExtents * 2.0f,
                    WallMat, true, HalfExtents, Physics::ColliderType::Box, SeedSalt + 1);

    // Cap trim band running along the eaves — slightly proud of the wall.
    const float TrimTopY = Center.y + HalfExtents.y - 0.18f;
    AddStaticBox(Math::Vec3(Center.x, TrimTopY, Center.z),
                 Math::Vec3(HalfExtents.x * 2.0f + 0.18f, 0.36f, HalfExtents.z * 2.0f + 0.18f),
                 TrimMat, false);

    // Overhanging roof slab: 0.6m overhang on every side, slightly thicker than wall trim.
    const float RoofY = Center.y + HalfExtents.y + 0.20f;
    AddStaticBox(Math::Vec3(Center.x, RoofY, Center.z),
                 Math::Vec3(HalfExtents.x * 2.0f + 1.2f, 0.40f, HalfExtents.z * 2.0f + 1.2f),
                 RoofMat, false);
    // Drip edge underside lip to read the overhang from below.
    AddStaticBox(Math::Vec3(Center.x, RoofY - 0.22f, Center.z + HalfExtents.z + 0.55f),
                 Math::Vec3(HalfExtents.x * 2.0f + 1.0f, 0.10f, 0.10f), TrimMat, false);
    AddStaticBox(Math::Vec3(Center.x, RoofY - 0.22f, Center.z - HalfExtents.z - 0.55f),
                 Math::Vec3(HalfExtents.x * 2.0f + 1.0f, 0.10f, 0.10f), TrimMat, false);

    if (HasWindowBand) {
        // Continuous warm-lit window strip on the front face (looking back toward spawn).
        const float FaceZ = Center.z - HalfExtents.z - 0.05f;
        const float StripHalfX = HalfExtents.x * 0.78f;
        const float WinY = Center.y + HalfExtents.y * 0.18f;
        AddStaticBox(Math::Vec3(Center.x, WinY, FaceZ),
                     Math::Vec3(StripHalfX * 2.0f, 1.10f, 0.06f), WindowMat, false);
        // Mullions break the strip into panes for an early-'00s look.
        const int Panes = std::max(2, static_cast<int>(StripHalfX / 1.2f));
        for (int I = 1; I < Panes; ++I) {
            const float T = -1.0f + 2.0f * static_cast<float>(I) / static_cast<float>(Panes);
            AddStaticBox(Math::Vec3(Center.x + T * StripHalfX, WinY, FaceZ - 0.01f),
                         Math::Vec3(0.08f, 1.20f, 0.04f), TrimMat, false);
        }
    }

    if (HasFloodlight) {
        const float LightY = RoofY + 0.05f;
        const Math::Vec3 LightPos(Center.x - HalfExtents.x + 1.2f, LightY, Center.z - HalfExtents.z - 0.4f);
        AddStaticBox(LightPos, Math::Vec3(0.40f, 0.18f, 0.30f), m_PaintedMetalMaterialHandle, false);
        AddStaticBox(LightPos + Math::Vec3(0.0f, -0.05f, -0.18f),
                     Math::Vec3(0.34f, 0.14f, 0.06f), m_GlowingWindowMaterialHandle, false);
    }
}

void BlizzardGame::BuildStorageContainer(const Math::Vec3& Center, const Math::Vec3& HalfExtents,
                                         Core::MaterialHandle ShellMat, Core::MaterialHandle TrimMat,
                                         uint32_t SeedSalt) {
    // Shipping container: shell + corner posts + corrugation ribs + door panel.
    using namespace Solstice::Arzachel;
    Generator<MeshData> Box = Cube(1.0f, Seed(SeedSalt));
    AddArzachelProp(Box, Center, Math::Quaternion(), HalfExtents * 2.0f,
                    ShellMat, true, HalfExtents, Physics::ColliderType::Box, SeedSalt + 1);

    // Corner posts (visual only) for the iconic corten silhouette.
    for (float Sx : {-1.0f, 1.0f}) {
        for (float Sz : {-1.0f, 1.0f}) {
            const Math::Vec3 PostC(Center.x + Sx * (HalfExtents.x + 0.04f),
                                   Center.y,
                                   Center.z + Sz * (HalfExtents.z + 0.04f));
            AddStaticBox(PostC, Math::Vec3(0.18f, HalfExtents.y * 2.0f + 0.10f, 0.18f), TrimMat, false);
        }
    }

    // Top rail / bottom rail.
    AddStaticBox(Center + Math::Vec3(0.0f, HalfExtents.y + 0.05f, 0.0f),
                 Math::Vec3(HalfExtents.x * 2.0f + 0.20f, 0.10f, HalfExtents.z * 2.0f + 0.20f),
                 TrimMat, false);
    AddStaticBox(Center + Math::Vec3(0.0f, -HalfExtents.y - 0.05f, 0.0f),
                 Math::Vec3(HalfExtents.x * 2.0f + 0.20f, 0.10f, HalfExtents.z * 2.0f + 0.20f),
                 TrimMat, false);

    // Vertical corrugation ribs along long sides.
    const int Ribs = std::max(4, static_cast<int>(HalfExtents.x * 1.4f));
    for (int I = 0; I < Ribs; ++I) {
        const float T = -1.0f + 2.0f * (static_cast<float>(I) + 0.5f) / static_cast<float>(Ribs);
        const float X = Center.x + T * (HalfExtents.x - 0.15f);
        AddStaticBox(Math::Vec3(X, Center.y, Center.z + HalfExtents.z + 0.04f),
                     Math::Vec3(0.10f, HalfExtents.y * 2.0f - 0.30f, 0.06f), TrimMat, false);
        AddStaticBox(Math::Vec3(X, Center.y, Center.z - HalfExtents.z - 0.04f),
                     Math::Vec3(0.10f, HalfExtents.y * 2.0f - 0.30f, 0.06f), TrimMat, false);
    }

    // Single set of doors on the +X face.
    AddStaticBox(Math::Vec3(Center.x + HalfExtents.x + 0.05f, Center.y, Center.z),
                 Math::Vec3(0.06f, HalfExtents.y * 2.0f - 0.18f, HalfExtents.z * 2.0f - 0.18f),
                 TrimMat, false);
    AddStaticBox(Math::Vec3(Center.x + HalfExtents.x + 0.10f, Center.y, Center.z),
                 Math::Vec3(0.04f, 0.10f, 0.55f), m_GlowingWindowMaterialHandle, false);
}

void BlizzardGame::BuildWatchTower(const Math::Vec3& Center, float ShellHalfX, float ShellHalfZ,
                                   float ShellHeight, float StiltHeight,
                                   Core::MaterialHandle ShellMat, Core::MaterialHandle StiltMat,
                                   Core::MaterialHandle RoofMat, Core::MaterialHandle WindowMat,
                                   uint32_t SeedSalt) {
    // Stilts at the four corners.
    const float StiltRadius = 0.18f;
    const float ShellY = Center.y + StiltHeight;
    for (float Sx : {-1.0f, 1.0f}) {
        for (float Sz : {-1.0f, 1.0f}) {
            const float Px = Center.x + Sx * (ShellHalfX - 0.20f);
            const float Pz = Center.z + Sz * (ShellHalfZ - 0.20f);
            AddStaticCylinder(Math::Vec3(Px, Center.y + StiltHeight * 0.5f, Pz),
                              StiltRadius, StiltHeight, StiltMat, true);
        }
    }
    // Cross-bracing under the shell — diagonal struts give the early-'00s industrial vibe.
    for (float Sx : {-1.0f, 1.0f}) {
        AddStaticBox(Math::Vec3(Center.x + Sx * (ShellHalfX - 0.20f), Center.y + StiltHeight * 0.65f, Center.z),
                     Math::Vec3(0.10f, 0.10f, ShellHalfZ * 1.6f), StiltMat, false);
    }
    for (float Sz : {-1.0f, 1.0f}) {
        AddStaticBox(Math::Vec3(Center.x, Center.y + StiltHeight * 0.45f, Center.z + Sz * (ShellHalfZ - 0.20f)),
                     Math::Vec3(ShellHalfX * 1.6f, 0.10f, 0.10f), StiltMat, false);
    }
    // Tower platform.
    AddStaticBox(Math::Vec3(Center.x, ShellY - ShellHeight * 0.5f - 0.16f, Center.z),
                 Math::Vec3(ShellHalfX * 2.0f + 0.30f, 0.18f, ShellHalfZ * 2.0f + 0.30f),
                 m_ConcreteMaterialHandle, false);

    // Operator shell.
    using namespace Solstice::Arzachel;
    Generator<MeshData> Box = Cube(1.0f, Seed(SeedSalt));
    AddArzachelProp(Box, Math::Vec3(Center.x, ShellY, Center.z), Math::Quaternion(),
                    Math::Vec3(ShellHalfX * 2.0f, ShellHeight, ShellHalfZ * 2.0f),
                    ShellMat, false,
                    Math::Vec3(ShellHalfX, ShellHeight * 0.5f, ShellHalfZ),
                    Physics::ColliderType::Box, SeedSalt + 1);

    // Wraparound window band.
    const float WinY = ShellY + ShellHeight * 0.10f;
    const float WinH = ShellHeight * 0.45f;
    AddStaticBox(Math::Vec3(Center.x, WinY, Center.z + ShellHalfZ + 0.04f),
                 Math::Vec3(ShellHalfX * 2.0f - 0.30f, WinH, 0.06f), WindowMat, false);
    AddStaticBox(Math::Vec3(Center.x, WinY, Center.z - ShellHalfZ - 0.04f),
                 Math::Vec3(ShellHalfX * 2.0f - 0.30f, WinH, 0.06f), WindowMat, false);
    AddStaticBox(Math::Vec3(Center.x + ShellHalfX + 0.04f, WinY, Center.z),
                 Math::Vec3(0.06f, WinH, ShellHalfZ * 2.0f - 0.30f), WindowMat, false);
    AddStaticBox(Math::Vec3(Center.x - ShellHalfX - 0.04f, WinY, Center.z),
                 Math::Vec3(0.06f, WinH, ShellHalfZ * 2.0f - 0.30f), WindowMat, false);

    // Pitched roof: two slabs intersecting on a ridge.
    const float RoofY = ShellY + ShellHeight * 0.5f + 0.12f;
    AddStaticBox(Math::Vec3(Center.x, RoofY, Center.z),
                 Math::Vec3(ShellHalfX * 2.0f + 0.50f, 0.18f, ShellHalfZ * 2.0f + 0.50f),
                 RoofMat, false);
    // Ridge cap and antenna.
    AddStaticBox(Math::Vec3(Center.x, RoofY + 0.20f, Center.z),
                 Math::Vec3(ShellHalfX * 0.30f, 0.20f, ShellHalfZ * 2.0f + 0.10f), RoofMat, false);
    AddStaticCylinder(Math::Vec3(Center.x, RoofY + 1.40f, Center.z),
                      0.06f, 2.40f, m_PaintedMetalMaterialHandle, false);
}

void BlizzardGame::BuildAntennaMast(const Math::Vec3& Base, float Height,
                                    Core::MaterialHandle Mat, uint32_t /*SeedSalt*/) {
    AddStaticCylinder(Math::Vec3(Base.x, Base.y + Height * 0.5f, Base.z),
                      0.14f, Height, Mat, false);
    // Horizontal strut rings every ~6m.
    const int Rings = std::max(2, static_cast<int>(Height / 6.0f));
    for (int I = 1; I <= Rings; ++I) {
        const float T = static_cast<float>(I) / static_cast<float>(Rings + 1);
        const float Y = Base.y + Height * T;
        const float Span = 2.6f - 1.6f * T;
        AddStaticBox(Math::Vec3(Base.x, Y, Base.z),
                     Math::Vec3(Span, 0.10f, 0.10f), Mat, false);
        AddStaticBox(Math::Vec3(Base.x, Y - 0.08f, Base.z),
                     Math::Vec3(0.10f, 0.10f, Span), Mat, false);
    }
    // Aviation warning beacon at the tip.
    AddStaticBox(Math::Vec3(Base.x, Base.y + Height + 0.10f, Base.z),
                 Math::Vec3(0.18f, 0.18f, 0.18f), m_GlowingWindowMaterialHandle, false);
}

namespace {

// Reject triangles whose centroid fails the predicate. This is our local "Difference" since
// Arzachel's CSG ops are stubs — useful for stripping below-ground triangles, capping a
// mountain, or culling outward-facing perimeter triangles that the camera will never see.
template<typename Predicate>
Solstice::Arzachel::Generator<Solstice::Arzachel::MeshData>
TrimByPredicate(const Solstice::Arzachel::Generator<Solstice::Arzachel::MeshData>& Source,
                Predicate Keep) {
    using namespace Solstice::Arzachel;
    return Generator<MeshData>([Source, Keep](const Seed& S) {
        MeshData Input = Source(S);
        MeshData Out;
        Out.Positions = Input.Positions;
        Out.Normals = Input.Normals;
        Out.UVs = Input.UVs;
        Out.Indices.reserve(Input.Indices.size());
        for (size_t I = 0; I + 2 < Input.Indices.size(); I += 3) {
            const Math::Vec3& A = Input.Positions[Input.Indices[I + 0]];
            const Math::Vec3& B = Input.Positions[Input.Indices[I + 1]];
            const Math::Vec3& C = Input.Positions[Input.Indices[I + 2]];
            const Math::Vec3 Centroid = (A + B + C) * (1.0f / 3.0f);
            if (Keep(Centroid)) {
                Out.Indices.push_back(Input.Indices[I + 0]);
                Out.Indices.push_back(Input.Indices[I + 1]);
                Out.Indices.push_back(Input.Indices[I + 2]);
            }
        }
        Out.SubMeshes.clear();
        Out.SubMeshes.emplace_back(0u, 0u, static_cast<uint32_t>(Out.Indices.size()));
        Out.CalculateBounds();
        return Out;
    });
}

} // namespace

void BlizzardGame::BuildSnowMound(const Math::Vec3& Center, float Radius, float HeightScale,
                                  Core::MaterialHandle Mat, uint32_t SeedSalt, bool CreateCollision) {
    // A drift built from a damaged sphere with below-ground geometry trimmed away.
    // Sphere(R) → bounds [-R, R]. After Scale(1, HeightScale, 1), Y range is [-R*HS, +R*HS].
    // We trim anything with centroid y < -R*HS*0.20 so the mesh has a clean flat bottom,
    // then offset upward so the bottom rests at world y = Center.y. This eliminates the
    // "ellipsoid floating above the ground" look the previous version had.
    using namespace Solstice::Arzachel;
    const float Height = Radius * HeightScale;
    Generator<MeshData> Drift = Sphere(Radius, 14, Seed(0x70DEu + SeedSalt));
    Drift = Scale(Drift, Math::Vec3(1.0f, HeightScale, 1.0f));
    Drift = Damaged(Drift, Seed(0x71DEu + SeedSalt), Radius * 0.12f);
    Drift = Smooth(Drift, 1);
    const float TrimY = -Height * 0.20f;
    Drift = TrimByPredicate(Drift, [TrimY](const Math::Vec3& C) { return C.y > TrimY; });
    // After trim, mesh bottom ≈ TrimY; offset by -TrimY to put it at world y=0.
    AddArzachelProp(Drift, Center + Math::Vec3(0.0f, -TrimY, 0.0f),
                    Math::Quaternion(), Math::Vec3(1.0f, 1.0f, 1.0f),
                    Mat, CreateCollision,
                    Math::Vec3(Radius, Height * 0.5f, Radius),
                    Physics::ColliderType::Box, SeedSalt + 7);
}

void BlizzardGame::BuildRockOutcrop(const Math::Vec3& Center, float Size,
                                    Core::MaterialHandle Mat, uint32_t SeedSalt, bool CreateCollision) {
    // Composite outcrop = 3 craggy sub-rocks. Each rock is a Subdivide-Cube (more vertex
    // density gives Damaged enough to work with), Damaged for cliff faces, then triangles
    // with centroid below -Size*0.30 get trimmed so the mesh has a clean flat bottom.
    using namespace Solstice::Arzachel;
    auto MakeRock = [](float S, uint32_t Salt) {
        Generator<MeshData> Base = Subdivide(Cube(S, Seed(Salt)));
        Generator<MeshData> Roughed = Damaged(Base, Seed(Salt + 13), S * 0.30f);
        return TrimByPredicate(Roughed, [S](const Math::Vec3& C) {
            return C.y > -S * 0.30f;
        });
    };
    constexpr uint32_t RockBase = 0x80C0u;
    // Offset by S*0.30 (the trim depth) so the bottom of the trimmed mesh rests on y=0.
    AddArzachelProp(MakeRock(Size, RockBase + SeedSalt),
                    Center + Math::Vec3(0.0f, Size * 0.30f, 0.0f), Math::Quaternion(),
                    Math::Vec3(1.0f, 1.0f, 1.0f), Mat, CreateCollision,
                    Math::Vec3(Size * 0.5f, Size * 0.4f, Size * 0.5f),
                    Physics::ColliderType::Box, SeedSalt + 1);
    const float S2 = Size * 0.55f;
    AddArzachelProp(MakeRock(S2, RockBase + SeedSalt + 17),
                    Center + Math::Vec3(Size * 0.55f, S2 * 0.30f, Size * 0.30f),
                    Math::Quaternion(), Math::Vec3(1.0f, 1.0f, 1.0f),
                    Mat, false, Math::Vec3(0.0f, 0.0f, 0.0f),
                    Physics::ColliderType::Box, SeedSalt + 2);
    const float S3 = Size * 0.42f;
    AddArzachelProp(MakeRock(S3, RockBase + SeedSalt + 31),
                    Center + Math::Vec3(-Size * 0.45f, S3 * 0.30f, -Size * 0.40f),
                    Math::Quaternion(), Math::Vec3(1.0f, 1.0f, 1.0f),
                    Mat, false, Math::Vec3(0.0f, 0.0f, 0.0f),
                    Physics::ColliderType::Box, SeedSalt + 3);
}

void BlizzardGame::BuildMountainPeak(const Math::Vec3& Base, float Radius, float Height,
                                     Core::MaterialHandle RockMat, Core::MaterialHandle SnowMat,
                                     uint32_t SeedSalt) {
    // Tall craggy peak: rock body (subdivided cube → pyramid via Pinch → Damaged for crags)
    // + snow cap (damaged sphere with the bottom trimmed) + 2 side spurs.
    // Cube(1) has bounds [-0.5, 0.5], so Scale(2R, H, 2R) yields total spans (2R, H, 2R)
    // with Y range [-H/2, +H/2]. We then translate up by H/2 so the base sits at the ground.
    using namespace Solstice::Arzachel;

    // Rock body. Two subdivides on a cube give 96 vertices for Damaged to bite into; the
    // Pinch operation pulls the upper vertices toward the apex point, producing a tapered
    // crag silhouette rather than a smooth ellipsoid.
    Generator<MeshData> Body = Subdivide(Subdivide(Cube(1.0f, Seed(0x90A0u + SeedSalt))));
    Body = Scale(Body, Math::Vec3(Radius * 2.0f, Height, Radius * 2.0f));
    // Strong Pinch toward a point well above the apex creates a clear pyramid taper.
    Body = Pinch(Body, Height * 1.6f, Math::Vec3(0.0f, Height * 0.85f, 0.0f));
    Body = Damaged(Body, Seed(0x90A1u + SeedSalt), std::min(Radius, Height) * 0.22f);
    // Cull the half that sits below the ground plane — never visible from the playable area
    // and the player never collides with anything past the boundary walls. Cuts ~40% of tris.
    Body = TrimByPredicate(Body, [](const Math::Vec3& C) { return C.y > 0.0f; });
    AddArzachelProp(Body, Base + Math::Vec3(0.0f, Height * 0.5f, 0.0f), Math::Quaternion(),
                    Math::Vec3(1.0f, 1.0f, 1.0f), RockMat, true,
                    Math::Vec3(Radius * 0.85f, Height * 0.5f, Radius * 0.85f),
                    Physics::ColliderType::Box, SeedSalt + 1);

    // Snow cap on the upper third. Sphere is damaged for wind-shaped roughness, then the
    // bottom 30% is trimmed so the cap sits flush against the rock body's apex rather than
    // floating as a discrete ellipsoid.
    const float CapRadius = Radius * 0.65f;
    Generator<MeshData> Cap = Damaged(Sphere(CapRadius, 14, Seed(0x90A2u + SeedSalt)),
                                      Seed(0x90A3u + SeedSalt), CapRadius * 0.15f);
    Cap = Scale(Cap, Math::Vec3(1.0f, 0.55f, 1.0f));
    Cap = TrimByPredicate(Cap, [CapRadius](const Math::Vec3& C) {
        return C.y > -CapRadius * 0.18f;
    });
    AddArzachelProp(Cap, Base + Math::Vec3(0.0f, Height * 0.92f, 0.0f), Math::Quaternion(),
                    Math::Vec3(1.0f, 1.0f, 1.0f), SnowMat, false,
                    Math::Vec3(0.0f, 0.0f, 0.0f),
                    Physics::ColliderType::Box, SeedSalt + 2);

    // Two asymmetric side spurs that break up the silhouette so the ring of peaks doesn't
    // look like a row of identical pyramids.
    auto MakeSpur = [&](const Math::Vec3& Offset, float SpurR, float SpurH, uint32_t Salt) {
        Generator<MeshData> Spur = Subdivide(Cube(1.0f, Seed(Salt)));
        Spur = Scale(Spur, Math::Vec3(SpurR * 2.0f, SpurH, SpurR * 2.0f));
        Spur = Pinch(Spur, SpurH * 1.2f, Math::Vec3(0.0f, SpurH * 0.85f, 0.0f));
        Spur = Damaged(Spur, Seed(Salt + 7), std::min(SpurR, SpurH) * 0.25f);
        Spur = TrimByPredicate(Spur, [](const Math::Vec3& C) { return C.y > 0.0f; });
        AddArzachelProp(Spur, Base + Offset + Math::Vec3(0.0f, SpurH * 0.5f, 0.0f),
                        Math::Quaternion(), Math::Vec3(1.0f, 1.0f, 1.0f),
                        RockMat, false, Math::Vec3(0.0f, 0.0f, 0.0f),
                        Physics::ColliderType::Box, Salt);
    };
    MakeSpur(Math::Vec3( Radius * 0.85f, 0.0f, Radius * 0.30f),
             Radius * 0.45f, Height * 0.55f, 0x90B0u + SeedSalt);
    MakeSpur(Math::Vec3(-Radius * 0.75f, 0.0f, -Radius * 0.45f),
             Radius * 0.40f, Height * 0.45f, 0x90C0u + SeedSalt);
}

void BlizzardGame::BuildFenceLine(const Math::Vec3& Start, const Math::Vec3& End, float Height,
                                  Core::MaterialHandle PostMat, int PostCount) {
    const Math::Vec3 Delta = End - Start;
    if (PostCount < 2) PostCount = 2;
    for (int I = 0; I < PostCount; ++I) {
        const float T = static_cast<float>(I) / static_cast<float>(PostCount - 1);
        const Math::Vec3 P = Start + Delta * T;
        AddStaticCylinder(Math::Vec3(P.x, P.y + Height * 0.5f, P.z),
                          0.10f, Height, PostMat, true);
    }
    // Two horizontal cables.
    for (float Cy : {Height * 0.35f, Height * 0.85f}) {
        const Math::Vec3 Mid = Start + Delta * 0.5f + Math::Vec3(0.0f, Cy, 0.0f);
        const float Length = Delta.Length();
        AddStaticBox(Mid, Math::Vec3(std::abs(Delta.x) > std::abs(Delta.z) ? Length : 0.04f,
                                     0.04f,
                                     std::abs(Delta.x) > std::abs(Delta.z) ? 0.04f : Length),
                     PostMat, false);
    }
}

void BlizzardGame::InitializeArcticBase() {
    // Compose the arctic base from Arzachel-built props rather than raw boxes.
    // Layout reads, looking toward +Z from the spawn:
    //   - left:    grey container warehouse + storage container annex
    //   - centre:  yellow watchtower on stilts (the screenshot's hero silhouette)
    //   - right:   long concrete depot with window band
    //   - back:    wood barn, secondary container

    // 1. Grey container warehouse (the previously roof-less one) — full roofed building now.
    BuildIndustrialBuilding(Math::Vec3(-32.0f, 2.4f, -3.0f), Math::Vec3(9.0f, 2.4f, 5.5f),
                            m_ContainerMaterialHandle, m_DarkRoofMaterialHandle,
                            m_PaintedMetalMaterialHandle, m_GlowingWindowMaterialHandle,
                            true, true, /*SeedSalt=*/101);

    // 2. Painted metal annex to the left of the warehouse.
    BuildIndustrialBuilding(Math::Vec3(-46.0f, 1.6f, 8.0f), Math::Vec3(6.0f, 1.6f, 4.0f),
                            m_PaintedMetalMaterialHandle, m_DarkRoofMaterialHandle,
                            m_RustedMetalMaterialHandle, m_GlowingWindowMaterialHandle,
                            true, false, /*SeedSalt=*/102);

    // 3. Long concrete depot on the right of the spawn.
    BuildIndustrialBuilding(Math::Vec3(22.0f, 2.4f, 10.0f), Math::Vec3(15.0f, 2.4f, 4.5f),
                            m_ConcreteMaterialHandle, m_DarkRoofMaterialHandle,
                            m_PaintedMetalMaterialHandle, m_GlowingWindowMaterialHandle,
                            true, true, /*SeedSalt=*/103);

    // 4. Wood barn at the back-right.
    BuildIndustrialBuilding(Math::Vec3(3.0f, 1.8f, 24.0f), Math::Vec3(5.0f, 1.8f, 3.5f),
                            m_WoodMaterialHandle, m_DarkRoofMaterialHandle,
                            m_RustedMetalMaterialHandle, m_GlowingWindowMaterialHandle,
                            false, false, /*SeedSalt=*/104);

    // 5. Pair of corten-style storage containers.
    BuildStorageContainer(Math::Vec3(-8.0f, 1.3f, 17.0f), Math::Vec3(4.0f, 1.3f, 2.5f),
                          m_ContainerMaterialHandle, m_RustedMetalMaterialHandle, /*SeedSalt=*/201);
    BuildStorageContainer(Math::Vec3(-2.0f, 1.3f, 17.0f), Math::Vec3(4.0f, 1.3f, 2.5f),
                          m_RustedMetalMaterialHandle, m_DarkRoofMaterialHandle, /*SeedSalt=*/202);

    // 6. Hero watchtower (yellow shell, on stilts, with antenna).
    BuildWatchTower(Math::Vec3(-18.0f, 0.0f, -1.0f),
                    /*ShellHalfX=*/3.5f, /*ShellHalfZ=*/3.0f,
                    /*ShellHeight=*/3.4f, /*StiltHeight=*/5.6f,
                    m_UtilityYellowMaterialHandle, m_PaintedMetalMaterialHandle,
                    m_DarkRoofMaterialHandle, m_GlowingWindowMaterialHandle,
                    /*SeedSalt=*/301);

    // 7. Standalone radio/comms masts framing the base.
    BuildAntennaMast(Math::Vec3(-52.0f, 0.0f, 34.0f), 24.0f, m_PaintedMetalMaterialHandle, 401);
    BuildAntennaMast(Math::Vec3(0.0f, 0.0f, 34.0f),  28.0f, m_PaintedMetalMaterialHandle, 402);
    BuildAntennaMast(Math::Vec3(42.0f, 0.0f, 34.0f), 22.0f, m_PaintedMetalMaterialHandle, 403);

    // 8. Concrete dunnage pads with snow caps in the foreground (matches the screenshot
    //    but each pad is now properly textured concrete with snow on top).
    for (int I = 0; I < 7; ++I) {
        const float X = -18.0f + static_cast<float>(I) * 2.4f;
        AddStaticBox(Math::Vec3(X, 0.38f, -20.0f), Math::Vec3(1.5f, 0.45f, 3.4f), m_ConcreteMaterialHandle, false);
        AddStaticBox(Math::Vec3(X, 0.72f, -20.0f), Math::Vec3(1.2f, 0.12f, 3.1f), m_SnowMaterialHandle, false);
    }

    // 9. Perimeter fence line along the back of the courtyard.
    BuildFenceLine(Math::Vec3(-44.0f, 0.0f, 42.0f), Math::Vec3(36.0f, 0.0f, 42.0f),
                   2.0f, m_PaintedMetalMaterialHandle, /*PostCount=*/9);

    // 10. Boundary geometry. We use thin invisible-ish "guard rails" of rock as a no-fall
    // fallback, then layer a ring of full-blown craggy mountain peaks just behind so the
    // distant horizon reads as a mountain range rather than a flat wall.
    // The walls themselves are kept low (8m) so the mountain peaks dominate the silhouette.
    AddStaticBox(Math::Vec3(  0.0f, 4.0f,  92.0f), Math::Vec3(190.0f, 8.0f, 6.0f), m_RockMaterialHandle);
    AddStaticBox(Math::Vec3(  0.0f, 4.0f, -92.0f), Math::Vec3(190.0f, 8.0f, 6.0f), m_RockMaterialHandle);
    AddStaticBox(Math::Vec3(-92.0f, 4.0f,   0.0f), Math::Vec3(  6.0f, 8.0f, 190.0f), m_RockMaterialHandle);
    AddStaticBox(Math::Vec3( 92.0f, 4.0f,   0.0f), Math::Vec3(  6.0f, 8.0f, 190.0f), m_RockMaterialHandle);

    // Distant snow-capped mountain ring. Each peak is procedurally sculpted so the silhouette
    // reads as crags, not eerie ellipsoids. Heights vary so the ring doesn't look uniform.
    struct PeakSpec { Math::Vec3 Base; float Radius; float Height; };
    const std::array<PeakSpec, 14> Peaks = {{
        // Far back (north).
        {Math::Vec3(-120.0f, 0.0f,  130.0f), 32.0f, 64.0f},
        {Math::Vec3( -55.0f, 0.0f,  140.0f), 28.0f, 52.0f},
        {Math::Vec3(  10.0f, 0.0f,  150.0f), 36.0f, 78.0f},
        {Math::Vec3(  78.0f, 0.0f,  138.0f), 30.0f, 58.0f},
        {Math::Vec3( 140.0f, 0.0f,  118.0f), 32.0f, 60.0f},
        // Far front (south).
        {Math::Vec3(-130.0f, 0.0f, -126.0f), 30.0f, 56.0f},
        {Math::Vec3( -65.0f, 0.0f, -142.0f), 34.0f, 70.0f},
        {Math::Vec3(  20.0f, 0.0f, -146.0f), 28.0f, 50.0f},
        {Math::Vec3(  90.0f, 0.0f, -132.0f), 32.0f, 64.0f},
        {Math::Vec3( 142.0f, 0.0f, -114.0f), 30.0f, 58.0f},
        // Side ridges.
        {Math::Vec3(-140.0f, 0.0f,   38.0f), 30.0f, 62.0f},
        {Math::Vec3(-138.0f, 0.0f,  -28.0f), 28.0f, 54.0f},
        {Math::Vec3( 144.0f, 0.0f,   30.0f), 32.0f, 66.0f},
        {Math::Vec3( 146.0f, 0.0f,  -36.0f), 30.0f, 60.0f},
    }};
    for (size_t I = 0; I < Peaks.size(); ++I) {
        BuildMountainPeak(Peaks[I].Base, Peaks[I].Radius, Peaks[I].Height,
                          m_RockMaterialHandle, m_SnowMaterialHandle,
                          /*SeedSalt=*/800 + static_cast<uint32_t>(I));
    }

    // Mid-distance rock outcrops between the play area and the mountain ring fill the gap so
    // the eye doesn't jump from courtyard scale to mountain scale.
    const std::array<Math::Vec3, 10> RockSpots = {
        Math::Vec3(-72.0f, 0.0f,  72.0f), Math::Vec3(-30.0f, 0.0f,  78.0f),
        Math::Vec3( 12.0f, 0.0f,  78.0f), Math::Vec3( 50.0f, 0.0f,  72.0f),
        Math::Vec3( 80.0f, 0.0f,  62.0f), Math::Vec3(-78.0f, 0.0f, -68.0f),
        Math::Vec3(-32.0f, 0.0f, -78.0f), Math::Vec3( 28.0f, 0.0f, -76.0f),
        Math::Vec3( 78.0f, 0.0f, -54.0f), Math::Vec3( 84.0f, 0.0f,  -8.0f)
    };
    for (size_t I = 0; I < RockSpots.size(); ++I) {
        const float Size = 9.0f + static_cast<float>(I % 3) * 3.0f;
        BuildRockOutcrop(RockSpots[I], Size, m_RockMaterialHandle,
                         /*SeedSalt=*/600 + static_cast<uint32_t>(I), /*CreateCollision=*/true);
    }

    // Snow drifts scattered through the play area to break up the flat ground plane.
    const std::array<Math::Vec3, 14> SnowSpots = {
        Math::Vec3(-58.0f, 0.0f, -28.0f), Math::Vec3(-32.0f, 0.0f, -34.0f),
        Math::Vec3( 12.0f, 0.0f, -32.0f), Math::Vec3( 36.0f, 0.0f, -34.0f),
        Math::Vec3( 60.0f, 0.0f, -22.0f), Math::Vec3(-58.0f, 0.0f,  60.0f),
        Math::Vec3( 30.0f, 0.0f,  56.0f), Math::Vec3( 60.0f, 0.0f,  44.0f),
        Math::Vec3(-12.0f, 0.0f,  60.0f), Math::Vec3(  6.0f, 0.0f,  44.0f),
        Math::Vec3(-72.0f, 0.0f,  10.0f), Math::Vec3( 70.0f, 0.0f,  18.0f),
        Math::Vec3(-28.0f, 0.0f,  -8.0f), Math::Vec3( 22.0f, 0.0f,  -6.0f)
    };
    for (size_t I = 0; I < SnowSpots.size(); ++I) {
        BuildSnowMound(SnowSpots[I], 4.5f + static_cast<float>(I % 3) * 1.4f, 0.55f,
                       m_SnowMaterialHandle, /*SeedSalt=*/700 + static_cast<uint32_t>(I), /*CreateCollision=*/false);
    }

    // 11. Atmospheric lighting cues (cool fill + warm window glow at the depot).
    Physics::LightSource ColdFill;
    ColdFill.Position = Math::Vec3(0.0f, 18.0f, -24.0f);
    ColdFill.Color = Math::Vec3(0.48f, 0.56f, 0.72f);
    ColdFill.Intensity = 2.2f;
    ColdFill.Hue = 0.0f;
    ColdFill.Attenuation = 0.08f;
    ColdFill.Range = 90.0f;
    m_Lights.push_back(ColdFill);

    Physics::LightSource WindowGlow;
    WindowGlow.Position = Math::Vec3(23.0f, 3.0f, 4.5f);
    WindowGlow.Color = Math::Vec3(1.0f, 0.72f, 0.38f);
    WindowGlow.Intensity = 3.0f;
    WindowGlow.Hue = 0.0f;
    WindowGlow.Attenuation = 0.22f;
    WindowGlow.Range = 18.0f;
    m_Lights.push_back(WindowGlow);

    // Warm glow under the watchtower platform (helps anchor it in the snow).
    Physics::LightSource TowerGlow;
    TowerGlow.Position = Math::Vec3(-18.0f, 6.0f, -1.0f);
    TowerGlow.Color = Math::Vec3(1.0f, 0.78f, 0.40f);
    TowerGlow.Intensity = 2.4f;
    TowerGlow.Hue = 0.0f;
    TowerGlow.Attenuation = 0.18f;
    TowerGlow.Range = 22.0f;
    m_Lights.push_back(TowerGlow);
}

void BlizzardGame::InitializeImportedProps() {
    const std::filesystem::path PropsDir("assets/props");
    if (!std::filesystem::exists(PropsDir)) {
        return;
    }

    std::vector<std::filesystem::path> PropFiles;
    for (const auto& Entry : std::filesystem::directory_iterator(PropsDir)) {
        if (!Entry.is_regular_file()) {
            continue;
        }
        const std::string Ext = Entry.path().extension().string();
        if (Ext == ".glb" || Ext == ".gltf" || Ext == ".obj") {
            PropFiles.push_back(Entry.path());
        }
    }
    std::sort(PropFiles.begin(), PropFiles.end());

    const std::array<Math::Vec3, 4> PropPositions = {
        Math::Vec3(-3.0f, 0.0f, 7.0f),
        Math::Vec3(38.0f, 0.0f, 20.0f),
        Math::Vec3(-47.0f, 0.0f, 22.0f),
        Math::Vec3(9.0f, 0.0f, 35.0f)
    };

    for (size_t I = 0; I < PropFiles.size() && I < PropPositions.size(); ++I) {
        try {
            auto Mesh = Core::AssetLoader::LoadMesh(std::filesystem::absolute(PropFiles[I]));
            if (!Mesh) {
                continue;
            }
            for (auto& SubMesh : Mesh->SubMeshes) {
                SubMesh.MaterialID = MaterialID(m_PaintedMetalMaterialHandle);
            }
            const uint32_t MeshID = m_MeshLibrary->AddMesh(std::move(Mesh));
            const uint32_t ObjectID = m_Scene.AddObject(
                MeshID,
                PropPositions[I],
                Math::Quaternion(),
                Math::Vec3(1.0f, 1.0f, 1.0f),
                Render::ObjectType_Static);

            auto Entity = m_Registry.Create();
            auto& RB = m_Registry.Add<Physics::RigidBody>(Entity);
            RB.Position = PropPositions[I] + Math::Vec3(0.0f, 1.0f, 0.0f);
            RB.Rotation = Math::Quaternion();
            RB.IsStatic = true;
            RB.SetMass(0.0f);
            RB.Type = Physics::ColliderType::Box;
            RB.HalfExtents = Math::Vec3(1.5f, 1.0f, 1.5f);
            RB.RenderObjectID = ObjectID;
        } catch (const std::exception& E) {
            SIMPLE_LOG("WARNING: Failed to load optional Blizzard prop " + PropFiles[I].string() + ": " + std::string(E.what()));
        }
    }
}

void BlizzardGame::InitializeCombatTargets() {
    m_CombatTargets.clear();
    m_CombatTargetObjects.clear();

    const std::array<Math::Vec3, 5> TargetPositions = {
        Math::Vec3(-7.0f, 1.1f, 12.0f),
        Math::Vec3(11.0f, 1.1f, 23.0f),
        Math::Vec3(34.0f, 1.1f, 8.0f),
        Math::Vec3(-36.0f, 1.1f, 18.0f),
        Math::Vec3(0.0f, 1.1f, 39.0f)
    };

    for (const Math::Vec3& Pos : TargetPositions) {
        auto Mesh = Arzachel::MeshFactory::CreateCube(1.0f);
        if (!Mesh) {
            continue;
        }
        if (!Mesh->SubMeshes.empty()) {
            Mesh->SubMeshes[0].MaterialID = MaterialID(m_EnemyMaterialHandle);
        }

        const uint32_t MeshID = m_MeshLibrary->AddMesh(std::move(Mesh));
        const uint32_t ObjectID = m_Scene.AddObject(
            MeshID,
            Pos,
            Math::Quaternion(),
            Math::Vec3(1.1f, 2.2f, 1.1f),
            Render::ObjectType_Static);

        const ECS::EntityId TargetEntity = m_Registry.Create();
        auto& Transform = m_Registry.Add<ECS::Transform>(TargetEntity);
        Transform.Position = Pos;
        Transform.Scale = Math::Vec3(1.1f, 2.2f, 1.1f);
        Transform.Matrix = Math::Matrix4::Identity();

        Game::Health Health;
        Health.CurrentHealth = 70.0f;
        Health.MaxHealth = 70.0f;
        m_Registry.Add<Game::Health>(TargetEntity, Health);

        auto& RB = m_Registry.Add<Physics::RigidBody>(TargetEntity);
        RB.Position = Pos;
        RB.Rotation = Math::Quaternion();
        RB.IsStatic = true;
        RB.SetMass(0.0f);
        RB.Type = Physics::ColliderType::Box;
        RB.HalfExtents = Math::Vec3(0.55f, 1.1f, 0.55f);
        RB.RenderObjectID = ObjectID;

        m_CombatTargets.push_back(TargetEntity);
        m_CombatTargetObjects.push_back(ObjectID);
    }
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
    Config.Count = 5;
    Config.TerrainSize = 200.0f; // Aligned with InitializeTerrain
    Config.Seed = 54321;
    Config.MaterialID = WoodMaterialHandle.IsValid() ? WoodMaterialHandle.GetValue() : 0xFFFFFFFF;
    Config.CreatePhysicsBody = true;

    m_SceneBuilder.AddStructures(&m_Scene, m_MeshLibrary.get(), m_MaterialLibrary.get(), &m_Registry, Config);
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

    // Each MaterialAssignment[I] registers m_Textures[I] and assigns it to the listed material
    // as the primary albedo. AlbedoTexIndex2/3 reference other textures by their m_Textures
    // index. Slots beyond the prop materials use an invalid MaterialID so the texture is still
    // registered but no spurious material override happens.
    std::vector<Game::MaterialAssignment> MaterialAssignments;
    MaterialAssignments.resize(22);

    constexpr uint16_t TexFrost     = 13;
    constexpr uint16_t TexDirt      = 14;
    constexpr uint16_t TexDetailSnow = 15;
    constexpr uint16_t TexDetailIce  = 16;
    constexpr uint16_t TexDetailRock = 17;
    constexpr uint16_t TexBlendSnow    = 18;
    constexpr uint16_t TexBlendIce     = 19;
    constexpr uint16_t TexBlendRock    = 20;
    constexpr uint16_t TexBlendTerrain = 21;
    // .smat files are deliberately not loaded back from disk: doing so would freeze the
    // earlier (non-textured) material colours and ignore later tweaks. A fresh fingerprint
    // each launch keeps the layered material updates authoritative.
    const std::string NoSmat;

    // Terrain materials.
    MaterialAssignments[0] = {m_SnowMaterialHandle.GetValue(),    0, TexDetailSnow, TexBlendSnow,    Math::Vec3(0.95f, 0.95f, 1.0f), 0.2f,  static_cast<uint8_t>(Core::TextureBlendMode::Multiply), 0.40f, NoSmat};
    MaterialAssignments[1] = {m_RockMaterialHandle.GetValue(),    0, TexDetailRock, TexBlendRock,    Math::Vec3(0.30f, 0.30f, 0.35f), 0.8f,  static_cast<uint8_t>(Core::TextureBlendMode::Multiply), 0.50f, NoSmat};
    MaterialAssignments[2] = {m_TerrainMaterialHandle.GetValue(), 0, TexDetailSnow, TexBlendTerrain, Math::Vec3(0.70f, 0.70f, 0.75f), 0.5f,  static_cast<uint8_t>(Core::TextureBlendMode::Multiply), 0.30f, NoSmat};
    MaterialAssignments[3] = {m_IceMaterialHandle.GetValue(),     0, TexDetailIce,  TexBlendIce,     Math::Vec3(0.85f, 0.90f, 1.00f), 0.1f,  static_cast<uint8_t>(Core::TextureBlendMode::Multiply), 0.35f, NoSmat};

    // Prop materials. Each picks up its dedicated albedo at the matching texture index, with
    // frost overlay as the detail and dirt streaks as the blend mask for layered weathering.
    MaterialAssignments[4]  = {m_ConcreteMaterialHandle.GetValue(),       0, TexFrost, TexDirt,  Math::Vec3(0.68f, 0.68f, 0.66f), 0.94f, static_cast<uint8_t>(Core::TextureBlendMode::Multiply), 0.55f, NoSmat};
    MaterialAssignments[5]  = {m_PaintedMetalMaterialHandle.GetValue(),   0, TexFrost, TexDirt,  Math::Vec3(0.50f, 0.55f, 0.62f), 0.55f, static_cast<uint8_t>(Core::TextureBlendMode::Multiply), 0.55f, NoSmat};
    MaterialAssignments[6]  = {m_RustedMetalMaterialHandle.GetValue(),    0, TexDirt,  TexFrost, Math::Vec3(0.42f, 0.24f, 0.14f), 0.86f, static_cast<uint8_t>(Core::TextureBlendMode::Multiply), 0.70f, NoSmat};
    MaterialAssignments[7]  = {m_ContainerMaterialHandle.GetValue(),      0, TexDirt,  TexFrost, Math::Vec3(0.55f, 0.18f, 0.12f), 0.78f, static_cast<uint8_t>(Core::TextureBlendMode::Multiply), 0.65f, NoSmat};
    MaterialAssignments[8]  = {m_WoodMaterialHandle.GetValue(),           0, TexFrost, TexDirt,  Math::Vec3(0.42f, 0.30f, 0.20f), 0.84f, static_cast<uint8_t>(Core::TextureBlendMode::Multiply), 0.55f, NoSmat};
    MaterialAssignments[9]  = {m_UtilityYellowMaterialHandle.GetValue(),  0, TexDirt,  TexFrost, Math::Vec3(0.86f, 0.66f, 0.18f), 0.58f, static_cast<uint8_t>(Core::TextureBlendMode::Multiply), 0.55f, NoSmat};
    MaterialAssignments[10] = {m_DarkRoofMaterialHandle.GetValue(),       0, TexFrost, TexDirt,  Math::Vec3(0.13f, 0.13f, 0.14f), 0.78f, static_cast<uint8_t>(Core::TextureBlendMode::Multiply), 0.50f, NoSmat};
    MaterialAssignments[11] = {m_PlasticMaterialHandle.GetValue(),        0, TexFrost, TexDirt,  Math::Vec3(0.55f, 0.52f, 0.48f), 0.42f, static_cast<uint8_t>(Core::TextureBlendMode::Multiply), 0.40f, NoSmat};
    MaterialAssignments[12] = {m_GlowingWindowMaterialHandle.GetValue(),  0, 0xFFFF,   0xFFFF,   Math::Vec3(0.95f, 0.74f, 0.32f), 0.36f, static_cast<uint8_t>(Core::TextureBlendMode::None),     0.00f, NoSmat};

    // Indices 13-21 only exist to register the shared overlay/detail/blend textures so that
    // the higher AlbedoTexIndex2/3 references above resolve to valid registry entries.
    constexpr uint32_t NoMat = 0xFFFFFFFFu;
    for (size_t I = 13; I < MaterialAssignments.size(); ++I) {
        MaterialAssignments[I] = {NoMat, 0, 0xFFFF, 0xFFFF,
                                  Math::Vec3(1.0f, 1.0f, 1.0f), 0.5f,
                                  static_cast<uint8_t>(Core::TextureBlendMode::None), 0.0f, ""};
    }

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
            Arzachel::MaterialSerializer::SaveToFile("assets/materials/snow.smat", *Mat);
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
            Arzachel::MaterialSerializer::SaveToFile("assets/materials/rock.smat", *Mat);
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
            Arzachel::MaterialSerializer::SaveToFile("assets/materials/terrain.smat", *Mat);
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
            Arzachel::MaterialSerializer::SaveToFile("assets/materials/ice_wall.smat", *Mat);
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

    // Physics is stepped once in FPSGame ECS (PhysicsStep). Re-apply horizontal velocity
    // after the internal ReactPhysics3D result so Quake-style movement is preserved.
    if (m_SceneInitialized) {
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

            ResolvePlayerWorldCollisions(RB, Movement);

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
                m_WindHowlEmitter = Core::Audio::AudioManager::Instance().CreateEmitter(
                    "assets/blizzard.wav", m_Camera.Position + Math::Vec3(0.0f, 4.0f, 0.0f), 180.0f, true);
                if (m_WindHowlEmitter != 0) {
                    Core::Audio::AudioManager::Instance().SetEmitterVolume(m_WindHowlEmitter, 0.45f);
                    Core::Audio::AudioManager::Instance().SetEmitterFlags(m_WindHowlEmitter, false, true, 2);
                }
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
                float HeadOffset = m_CameraHeight - ((m_CurrentCapsuleHeight * 0.5f) + m_CapsuleRadius);
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
            if (m_WindHowlEmitter != 0) {
                Core::Audio::AudioManager::Instance().UpdateEmitterTransform(
                    m_WindHowlEmitter, m_Camera.Position + Math::Vec3(0.0f, 4.0f, 0.0f));
            }
        } catch (const std::exception& e) {
            SIMPLE_LOG("ERROR: Exception setting audio listener: " + std::string(e.what()));
        } catch (...) {
            SIMPLE_LOG("ERROR: Unknown exception setting audio listener");
        }
    }

    // Update snow particles (only if scene is ready)
    if (m_SnowParticles && m_SceneInitialized) {
        try {
            m_SnowParticles->SetWindStrength(m_ActiveWindStrength);
            m_SnowParticles->UpdateWithWind(DeltaTime, m_Camera.Position, m_ActiveWindDirection);
        } catch (const std::exception& e) {
            SIMPLE_LOG("ERROR: Exception updating snow particles: " + std::string(e.what()));
        } catch (...) {
            SIMPLE_LOG("ERROR: Unknown exception updating snow particles");
        }
    }

    // Update bodycam HUD
    m_BodycamHUD.Update(DeltaTime);

    UpdateCombat(DeltaTime);
    CheckBoundsAndRecover(DeltaTime);

    // Update footsteps
    UpdateFootsteps(DeltaTime);
}

void BlizzardGame::UpdateCombat(float DeltaTime) {
    if (m_WeaponCooldown > 0.0f) {
        m_WeaponCooldown = std::max(0.0f, m_WeaponCooldown - DeltaTime);
    }

    if (!m_PendingShot || m_PlayerEntity == 0 || !m_Registry.Has<Game::Weapon>(m_PlayerEntity)) {
        return;
    }
    m_PendingShot = false;

    const auto& Weapon = m_Registry.Get<Game::Weapon>(m_PlayerEntity);
    const Math::Vec3 Origin = m_Camera.Position;
    const Math::Vec3 Direction = m_Camera.Front.Normalized();

    float BestDistance = Weapon.Range;
    size_t BestIndex = m_CombatTargets.size();
    for (size_t I = 0; I < m_CombatTargets.size(); ++I) {
        const ECS::EntityId Target = m_CombatTargets[I];
        if (!m_Registry.Has<Game::Health>(Target) || !m_Registry.Has<ECS::Transform>(Target)) {
            continue;
        }
        auto& Health = m_Registry.Get<Game::Health>(Target);
        if (Health.CurrentHealth <= 0.0f) {
            continue;
        }

        const Math::Vec3 TargetPos = m_Registry.Get<ECS::Transform>(Target).Position + Math::Vec3(0.0f, 0.6f, 0.0f);
        const Math::Vec3 ToTarget = TargetPos - Origin;
        const float AlongRay = ToTarget.Dot(Direction);
        if (AlongRay < 0.0f || AlongRay > Weapon.Range) {
            continue;
        }
        const Math::Vec3 Closest = Origin + Direction * AlongRay;
        const float MissDistance = (TargetPos - Closest).Magnitude();
        if (MissDistance <= 0.9f && AlongRay < BestDistance) {
            BestDistance = AlongRay;
            BestIndex = I;
        }
    }

    if (BestIndex < m_CombatTargets.size()) {
        const ECS::EntityId Target = m_CombatTargets[BestIndex];
        auto& Health = m_Registry.Get<Game::Health>(Target);
        Health.CurrentHealth = std::max(0.0f, Health.CurrentHealth - Weapon.Damage);
        if (Health.CurrentHealth <= 0.0f) {
            if (m_Registry.Has<ECS::Transform>(Target)) {
                m_Registry.Get<ECS::Transform>(Target).Position.y = -100.0f;
            }
            if (m_Registry.Has<Physics::RigidBody>(Target)) {
                auto& RB = m_Registry.Get<Physics::RigidBody>(Target);
                RB.Position.y = -100.0f;
                RB.HalfExtents = Math::Vec3(0.01f, 0.01f, 0.01f);
            }
            if (BestIndex < m_CombatTargetObjects.size()) {
                m_Scene.SetPosition(m_CombatTargetObjects[BestIndex], Math::Vec3(0.0f, -100.0f, 0.0f));
            }
        }
    }
}

void BlizzardGame::CheckBoundsAndRecover(float DeltaTime) {
    if (m_BoundsMessageTimer > 0.0f) {
        m_BoundsMessageTimer = std::max(0.0f, m_BoundsMessageTimer - DeltaTime);
    }

    if (!m_SceneInitialized || m_PlayerEntity == 0 || !m_Registry.Has<Physics::RigidBody>(m_PlayerEntity)) {
        return;
    }

    auto& RB = m_Registry.Get<Physics::RigidBody>(m_PlayerEntity);
    const bool OutOfBounds =
        std::abs(RB.Position.x) > m_PlayableHalfExtent ||
        std::abs(RB.Position.z) > m_PlayableHalfExtent ||
        RB.Position.y < m_FallRecoveryY;

    if (!OutOfBounds) {
        return;
    }

    ResetPlayerToBlizzardSpawn();
    if (m_Registry.Has<Game::FPSMovement>(m_PlayerEntity)) {
        auto& Movement = m_Registry.Get<Game::FPSMovement>(m_PlayerEntity);
        Movement.DesiredHorizontalVelocity = Math::Vec3(0.0f, 0.0f, 0.0f);
    }
    m_BoundsMessageTimer = 3.0f;
}

void BlizzardGame::ResolvePlayerWorldCollisions(Physics::RigidBody& PlayerRB, Game::FPSMovement& Movement) {
    const float PlayerRadius = std::max(PlayerRB.CapsuleRadius, 0.3f);
    const float PlayerHalfHeight = PlayerRB.CapsuleHeight * 0.5f + PlayerRB.CapsuleRadius;
    const float PlayerMinY = PlayerRB.Position.y - PlayerHalfHeight;
    const float PlayerMaxY = PlayerRB.Position.y + PlayerHalfHeight;

    bool Grounded = false;
    float BestGroundY = -std::numeric_limits<float>::infinity();

    m_Registry.ForEach<Physics::RigidBody>([&](ECS::EntityId, Physics::RigidBody& StaticRB) {
        if (!StaticRB.IsStatic || StaticRB.Type != Physics::ColliderType::Box) {
            return;
        }

        const Math::Vec3 BoxMin = StaticRB.Position - StaticRB.HalfExtents;
        const Math::Vec3 BoxMax = StaticRB.Position + StaticRB.HalfExtents;
        const bool XZOverlap =
            PlayerRB.Position.x + PlayerRadius >= BoxMin.x &&
            PlayerRB.Position.x - PlayerRadius <= BoxMax.x &&
            PlayerRB.Position.z + PlayerRadius >= BoxMin.z &&
            PlayerRB.Position.z - PlayerRadius <= BoxMax.z;

        if (!XZOverlap) {
            return;
        }

        const bool IsTerrainSlab = StaticRB.HalfExtents.x > 50.0f && StaticRB.HalfExtents.z > 50.0f;
        const float BoxTop = BoxMax.y;

        if (IsTerrainSlab) {
            const float TargetY = BoxTop + PlayerHalfHeight + 0.02f;
            if (PlayerRB.Position.y < TargetY || PlayerRB.Position.y - TargetY < 1.0f || PlayerRB.Position.y < -5.0f) {
                BestGroundY = std::max(BestGroundY, BoxTop);
                Grounded = true;
            }
            return;
        }

        // Structure collision: either stand on the roof/top or push horizontally out of the wall.
        const bool VerticalOverlap = PlayerMinY <= BoxMax.y && PlayerMaxY >= BoxMin.y;
        if (!VerticalOverlap) {
            return;
        }

        if (PlayerMinY >= BoxTop - 0.15f && PlayerRB.Velocity.y <= 0.0f) {
            BestGroundY = std::max(BestGroundY, BoxTop);
            Grounded = true;
            return;
        }

        const float PushLeft = (BoxMin.x - PlayerRadius) - PlayerRB.Position.x;
        const float PushRight = (BoxMax.x + PlayerRadius) - PlayerRB.Position.x;
        const float PushBack = (BoxMin.z - PlayerRadius) - PlayerRB.Position.z;
        const float PushForward = (BoxMax.z + PlayerRadius) - PlayerRB.Position.z;
        const float ResolveX = (std::abs(PushLeft) < std::abs(PushRight)) ? PushLeft : PushRight;
        const float ResolveZ = (std::abs(PushBack) < std::abs(PushForward)) ? PushBack : PushForward;

        if (std::abs(ResolveX) < std::abs(ResolveZ)) {
            PlayerRB.Position.x += ResolveX;
            PlayerRB.Velocity.x = 0.0f;
            Movement.DesiredHorizontalVelocity.x = 0.0f;
        } else {
            PlayerRB.Position.z += ResolveZ;
            PlayerRB.Velocity.z = 0.0f;
            Movement.DesiredHorizontalVelocity.z = 0.0f;
        }
    });

    if (Grounded && BestGroundY > -1000.0f) {
        const float TargetY = BestGroundY + PlayerHalfHeight + 0.02f;
        if (PlayerRB.Position.y < TargetY) {
            PlayerRB.Position.y = TargetY;
        }
        if (PlayerRB.Velocity.y < 0.0f) {
            PlayerRB.Velocity.y = 0.0f;
        }
        Movement.IsGrounded = true;
    }
}

void BlizzardGame::UpdateFootsteps(float DeltaTime) {
    // Don't play footsteps during loading
    if (!m_TextureManager.IsReady() || !m_SceneInitialized) {
        return;
    }

    if (m_PlayerEntity == 0 || !m_Registry.Has<Game::FPSMovement>(m_PlayerEntity) ||
        !m_Registry.Has<Physics::RigidBody>(m_PlayerEntity)) {
        return;
    }

    auto& Movement = m_Registry.Get<Game::FPSMovement>(m_PlayerEntity);
    const auto& RB = m_Registry.Get<Physics::RigidBody>(m_PlayerEntity);
    Vec3 bodyPos = RB.Position;

    // Use horizontal body displacement (camera bob was firing every frame)
    Vec3 planarDelta = bodyPos - m_LastFootstepBodyPos;
    planarDelta.y = 0.0f;
    const float planarMoved = planarDelta.Magnitude();

    const bool wantsWalk = Movement.MoveDirection.Magnitude() > 0.12f;
    const bool isOnGround = Movement.IsGrounded;

    if (!wantsWalk || !isOnGround) {
        m_LastFootstepTime = 0.0f;
        m_LastFootstepBodyPos = bodyPos;
    } else {
        m_LastFootstepTime += DeltaTime;
        if (planarMoved >= m_FootstepStride && m_LastFootstepTime >= m_FootstepMinInterval) {
            try {
                Vec3 earPos = m_Camera.Position;
                auto emitter = Core::Audio::AudioManager::Instance().CreateEmitter(
                    "assets/footsteps.wav", earPos, 12.0f, false);
                if (emitter != 0) {
                    Core::Audio::AudioManager::Instance().SetEmitterVolume(emitter, m_FootstepVolume);
                    Core::Audio::AudioManager::Instance().SetEmitterFlags(emitter, false, false, 0);
                }
            } catch (const std::exception& e) {
                SIMPLE_LOG("ERROR: Exception playing footstep sound: " + std::string(e.what()));
            } catch (...) {
                SIMPLE_LOG("ERROR: Unknown exception playing footstep sound");
            }
            m_LastFootstepBodyPos = bodyPos;
            m_LastFootstepTime = 0.0f;
        }
    }

    m_LastPosition = m_Camera.Position;
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
                if (m_Skybox) {
                    std::string skyErr;
                    Solstice::Render::ApplyAuthoringSkyboxBusToSkybox(*m_Skybox, &skyErr);
                }
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
            RenderCombatHUD();
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
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 0.8f), "LMB - Fire / R - Reload");
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 0.8f), "ESC - Unlock");
        ImGui::End();
    }
}

void BlizzardGame::RenderCombatHUD() {
    if (!ImGui::GetCurrentContext()) return;
    ImVec2 ScreenSize = ImGui::GetIO().DisplaySize;
    if (ScreenSize.x <= 0.0f || ScreenSize.y <= 0.0f) return;

    int CurrentAmmo = 0;
    int ReserveAmmo = 0;
    if (m_PlayerEntity != 0 && m_Registry.Has<Game::Weapon>(m_PlayerEntity)) {
        const auto& Weapon = m_Registry.Get<Game::Weapon>(m_PlayerEntity);
        CurrentAmmo = Weapon.CurrentAmmo;
        ReserveAmmo = Weapon.ReserveAmmo;
    }

    int HealthValue = 100;
    if (m_PlayerEntity != 0 && m_Registry.Has<Game::Health>(m_PlayerEntity)) {
        HealthValue = static_cast<int>(m_Registry.Get<Game::Health>(m_PlayerEntity).CurrentHealth);
    }

    int TargetsRemaining = 0;
    for (ECS::EntityId Target : m_CombatTargets) {
        if (m_Registry.Has<Game::Health>(Target) && m_Registry.Get<Game::Health>(Target).CurrentHealth > 0.0f) {
            TargetsRemaining++;
        }
    }

    ImDrawList* Draw = ImGui::GetForegroundDrawList();
    const ImU32 HudColor = IM_COL32(245, 238, 186, 230);
    const ImU32 ShadowColor = IM_COL32(0, 0, 0, 180);

    auto DrawTextShadow = [&](ImVec2 Pos, const char* Text) {
        Draw->AddText(ImVec2(Pos.x + 2.0f, Pos.y + 2.0f), ShadowColor, Text);
        Draw->AddText(Pos, HudColor, Text);
    };

    char HealthText[64];
    std::snprintf(HealthText, sizeof(HealthText), "HEALTH\n%03d", HealthValue);
    DrawTextShadow(ImVec2(28.0f, ScreenSize.y - 88.0f), HealthText);

    char AmmoText[64];
    std::snprintf(AmmoText, sizeof(AmmoText), "AMMO\n%02d|%03d", CurrentAmmo, ReserveAmmo);
    DrawTextShadow(ImVec2(ScreenSize.x - 126.0f, ScreenSize.y - 88.0f), AmmoText);

    char TargetText[64];
    std::snprintf(TargetText, sizeof(TargetText), "TARGETS %d", TargetsRemaining);
    DrawTextShadow(ImVec2(ScreenSize.x * 0.5f - 45.0f, 28.0f), TargetText);

    const ImVec2 WeaponBase(ScreenSize.x * 0.66f, ScreenSize.y * 0.78f);
    const ImU32 WeaponBody = IM_COL32(18, 20, 20, 230);
    const ImU32 WeaponEdge = IM_COL32(95, 101, 96, 220);
    Draw->AddRectFilled(WeaponBase, ImVec2(WeaponBase.x + 230.0f, WeaponBase.y + 46.0f), WeaponBody, 4.0f);
    Draw->AddRectFilled(ImVec2(WeaponBase.x + 38.0f, WeaponBase.y - 20.0f),
                        ImVec2(WeaponBase.x + 88.0f, WeaponBase.y + 2.0f), WeaponBody, 3.0f);
    Draw->AddRectFilled(ImVec2(WeaponBase.x + 220.0f, WeaponBase.y + 12.0f),
                        ImVec2(WeaponBase.x + 294.0f, WeaponBase.y + 24.0f), WeaponBody, 2.0f);
    Draw->AddRectFilled(ImVec2(WeaponBase.x + 82.0f, WeaponBase.y + 42.0f),
                        ImVec2(WeaponBase.x + 128.0f, WeaponBase.y + 88.0f), WeaponBody, 3.0f);
    Draw->AddRect(ImVec2(WeaponBase.x, WeaponBase.y),
                  ImVec2(WeaponBase.x + 230.0f, WeaponBase.y + 46.0f), WeaponEdge, 4.0f, 0, 2.0f);

    if (m_BoundsMessageTimer > 0.0f) {
        DrawTextShadow(ImVec2(ScreenSize.x * 0.5f - 118.0f, ScreenSize.y * 0.34f),
                       "Woopsies! Sorry about that");
    }
}

} // namespace Solstice::Blizzard
