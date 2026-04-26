#include "HyperbourneGame.hxx"
#include "Archon.hxx"
#include <Game/FPS/FPSMovement.hxx>
#include <Game/Gameplay/Health.hxx>
#include <Game/UI/PauseMenu.hxx>
#include <Game/App/GamePreferences.hxx>
#include <Game/FPS/Weapon.hxx>
#include <Game/FPS/WeaponSwitcher.hxx>
#include <Game/Gameplay/Inventory.hxx>
#include <Game/Gameplay/SFXManager.hxx>
#include <Arzachel/Presets/Weapons/WeaponPresets.hxx>
#include <Arzachel/Presets/Bullets/BulletPresets.hxx>
#include <UI/Core/UISystem.hxx>
#include <Render/Assets/Mesh.hxx>
#include <Material/Material.hxx>
#include <Arzachel/ProceduralTexture.hxx>
#include <Arzachel/MaterialPresets.hxx>
#include <Arzachel/Seed.hxx>
#include <Arzachel/AssetBuilder.hxx>
#include <Arzachel/Polyhedra.hxx>
#include <Arzachel/GeometryOps.hxx>
#include <Arzachel/Presets/Architecture/ArchitecturePresets.hxx>
#include <Arzachel/Presets/Industrial/IndustrialPresets.hxx>
#include <Arzachel/Presets/Furniture/FurniturePresets.hxx>
#include <Arzachel/Presets/LevelCompositions.hxx>
#include <Render/Scene/Skybox.hxx>
#include <Render/Scene/AuthoringSkyboxApply.hxx>
#include <Render/PhysicsBridge.hxx>
#include <Physics/Integration/PhysicsSystem.hxx>
#include <Physics/Dynamics/RigidBody.hxx>
#include <Physics/Lighting/LightSource.hxx>
#include <Core/System/Async.hxx>
#include <Core/Audio/Audio.hxx>
#include <Asset/Streaming/AssetStream.hxx>
#include <Core/Debug/Debug.hxx>
#include <Solstice.hxx>
#include <imgui.h>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>

// Key constants
static constexpr int KEY_ESCAPE = 27;

using namespace Solstice;
using namespace Solstice::Render;
using namespace Solstice::Core;
using namespace Solstice::UI;
using namespace Solstice::Math;
using namespace Solstice::Game;
using namespace Solstice::Hyperbourne;

namespace Solstice::Hyperbourne {

HyperbourneGame::HyperbourneGame() : FPSGame() {
}

HyperbourneGame::~HyperbourneGame() {
    Shutdown();
}

void HyperbourneGame::Initialize() {
    // CRITICAL: Initialize unified engine systems (JobSystem, Audio, etc.)
    Solstice::Initialize();

    // Initialize window first
    InitializeWindow();

    // Initialize FPSGame base systems
    FPSGame::Initialize();

    // Start in Main Menu
    if (m_GameState) {
        m_GameState->SetState(Solstice::Game::GameStateType::MainMenu);
    }

    std::cout << "=== Solstice Engine - Hyperbourne ===" << std::endl;
    std::cout << "A procedural FPS experience in early '00s aesthetic" << std::endl;

    // Start physics system
    Physics::PhysicsSystem::Instance().Start(m_Registry);

    // Initialize script manager
    if (!m_ScriptManager.Initialize("scripts", &m_Registry, &m_Scene, &Physics::PhysicsSystem::Instance(), &m_Camera)) {
        SIMPLE_LOG("WARNING: ScriptManager initialization failed - scripts may not work");
    }

    // Create renderer
    auto fbSize = m_Window->GetFramebufferSize();
    m_Renderer = std::make_unique<SoftwareRenderer>(fbSize.first, fbSize.second, 16, m_Window->NativeWindow());
    m_Renderer->SetWireframe(false);
    m_Renderer->SetShowDebugOverlay(m_ShowDebugOverlay);
    m_Renderer->SetShowCrosshair(true);
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

    // Create default materials early (will be updated with textures later)
    Game::MaterialConfig DefaultConfig;
    m_ConcreteMaterialHandle = m_MaterialPresetManager.CreateMaterial(DefaultConfig);
    m_MetalMaterialHandle = m_MaterialPresetManager.CreateMaterial(DefaultConfig);
    m_PlasticMaterialHandle = m_MaterialPresetManager.CreateMaterial(DefaultConfig);
    m_GlassMaterialHandle = m_MaterialPresetManager.CreateMaterial(DefaultConfig);
    m_RubberMaterialHandle = m_MaterialPresetManager.CreateMaterial(DefaultConfig);

    // Initialize procedural textures (async - will show loading screen)
    InitializeProceduralTextures();

    // Initialize skybox
    InitializeSkybox();

    // Initialize audio
    InitializeAudio();

    // Initialize level selector
    m_LevelSelector = std::make_unique<Game::LevelSelector>();

    // Add endless mode level
    Game::LevelMetadata EndlessLevel;
    EndlessLevel.Name = "Endless Mode";
    EndlessLevel.Description = "Fight Archons and Monoliths in an endless wasteland";
    EndlessLevel.IsUnlocked = true;
    m_LevelSelector->AddLevel(EndlessLevel);

    // Setup Main Menu callbacks
    if (m_MainMenu) {
        m_MainMenu->SetTitle("HYPERBOURNE");
        m_MainMenu->SetSubtitle("2003");
        m_MainMenu->SetShowLevelSelector(true);
        m_MainMenu->SetLevelSelector(m_LevelSelector.get());

        m_MainMenu->SetNewGameCallback([this]() {
            if (m_LevelSelector) {
                // Show level selector - handled by MainMenu
            }
        });

        m_MainMenu->SetSettingsCallback([this]() {
            m_ShowSettingsMenu = true;
            // MainMenu's m_ShowSettingsMenu is already set by ExecuteOption
        });

        m_MainMenu->SetQuitCallback([this]() {
            m_ShouldClose = true;
        });
    }

    // Setup level selector callback
    if (m_LevelSelector) {
        m_LevelSelector->SetLevelSelectedCallback([this](const std::string& LevelName) {
            if (LevelName.empty()) {
                // Back button pressed - close level selector
                if (m_MainMenu) {
                    m_MainMenu->CloseLevelSelector();
                }
            } else {
                LoadLevel(LevelName);
            }
        });
    }

    // Initialize pause menu
    m_PauseMenu = std::make_unique<Game::PauseMenu>();
    m_PauseMenu->SetTitle("PAUSED");
    m_PauseMenu->SetResumeCallback([this]() {
        if (m_GameState) {
            m_GameState->SetState(Solstice::Game::GameStateType::Playing);
        }
        m_PauseMenu->Hide();
    });
    m_PauseMenu->SetSettingsCallback([this]() {
        m_ShowSettingsMenu = true;
        if (m_PauseMenu) {
            m_PauseMenu->SetSettingsOpen(true);
        }
    });
    m_PauseMenu->SetSaveCallback([this]() {
        SIMPLE_LOG("Save game (placeholder)");
    });
    m_PauseMenu->SetQuitCallback([this]() {
        if (m_GameState) {
            m_GameState->SetState(Solstice::Game::GameStateType::MainMenu);
        }
        m_PauseMenu->Hide();
        // Stop level music
        try {
            Core::Audio::AudioManager::Instance().StopMusic();
            m_LevelMusicPlaying = false;
        } catch (...) {
            // Non-fatal
        }
    });

    // Initialize game preferences
    m_GamePreferences = std::make_unique<Game::GamePreferences>();
    if (m_Renderer) {
        m_GamePreferences->SyncRenderer(*m_Renderer);
    }
    m_GamePreferences->SetSettingsClosedCallback([this]() {
        m_ShowSettingsMenu = false;
        if (m_Renderer) {
            m_GamePreferences->SyncRenderer(*m_Renderer);
        }
        // Also close settings menu in MainMenu if it's open
        if (m_MainMenu) {
            m_MainMenu->CloseSettingsMenu();
        }
        if (m_PauseMenu) {
            m_PauseMenu->SetSettingsOpen(false);
        }
    });

    // Initialize mouse position
    if (m_Window) {
        m_MouseX = m_Window->GetFramebufferSize().first * 0.5f;
        m_MouseY = m_Window->GetFramebufferSize().second * 0.5f;
    }
}

void HyperbourneGame::Shutdown() {
    // Shutdown managers (they will wait for async tasks to complete)
    m_TextureManager.Shutdown();
    m_ScriptManager.Shutdown();

    Solstice::Shutdown();
}

void HyperbourneGame::InitializeWindow() {
    // Create window in fullscreen mode
    auto WindowPtr = std::make_unique<Window>(1280, 720, "Solstice Engine - Hyperbourne");
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

void HyperbourneGame::InitializeFPSSystems() {
    FPSGame::InitializeFPSSystems();
    m_ShowCrosshair = true;
    m_ShowWeaponHUD = false;
}

void HyperbourneGame::InitializeWeapons() {
    using namespace Solstice::Game;

    FPSGame::InitializeWeapons();

    if (m_PlayerEntity == 0) {
        SIMPLE_LOG("WARNING: Cannot initialize weapons - player entity not created");
        return;
    }

    // Note: Weapon meshes will be created later when MeshLibrary is ready
    // For now, just set up the inventory/weapon switcher system

    // Add weapons to player entity using Inventory/WeaponSwitcher system
    if (!m_Registry.Has<Inventory>(m_PlayerEntity)) {
        m_Registry.Add<Inventory>(m_PlayerEntity, Inventory());
    }
    if (!m_Registry.Has<WeaponSwitcher>(m_PlayerEntity)) {
        m_Registry.Add<WeaponSwitcher>(m_PlayerEntity, WeaponSwitcher());
    }

    // Create Item entries for weapons
    Item ak47Item;
    ak47Item.ItemID = 1001; // Unique ID for AK-47
    ak47Item.Name = "AK-47";
    ak47Item.Type = ItemType::Weapon;
    ak47Item.Properties["WeaponType"] = static_cast<float>(WeaponType::Rifle);
    ak47Item.Properties["Damage"] = 28.0f;
    ak47Item.Properties["FireRate"] = 10.0f;
    ak47Item.Properties["Range"] = 100.0f;
    ak47Item.Properties["CurrentAmmo"] = 30.0f;
    ak47Item.Properties["MaxAmmo"] = 30.0f;
    InventorySystem::AddItem(m_Registry, m_PlayerEntity, ak47Item);
    WeaponSwitcherSystem::AddWeapon(m_Registry, m_PlayerEntity, 1001);

    Item knifeItem;
    knifeItem.ItemID = 1002; // Unique ID for Knife
    knifeItem.Name = "Knife";
    knifeItem.Type = ItemType::Weapon;
    knifeItem.Properties["WeaponType"] = static_cast<float>(WeaponType::Knife);
    knifeItem.Properties["Damage"] = 18.0f;
    knifeItem.Properties["MeleeRange"] = 2.0f;
    InventorySystem::AddItem(m_Registry, m_PlayerEntity, knifeItem);
    WeaponSwitcherSystem::AddWeapon(m_Registry, m_PlayerEntity, 1002);

    SIMPLE_LOG("HyperbourneGame: Weapon system initialized (meshes will be created when ready)");
}

void HyperbourneGame::CreateWeaponMeshes() {
    using namespace Solstice::Arzachel;

    if (!m_MeshLibrary) {
        SIMPLE_LOG("WARNING: Cannot create weapon meshes - MeshLibrary not initialized");
        return;
    }

    // Create procedural weapon meshes
    // AK-47 (using Rifle preset)
    try {
        auto ak47Gen = Rifle(m_Seed.Derive(400000));
        auto ak47Mesh = ak47Gen(m_Seed.Derive(400000));
        auto ak47RenderMesh = ConvertToRenderMesh(ak47Mesh);
        if (ak47RenderMesh) {
            m_AK47MeshID = m_MeshLibrary->AddMesh(std::move(ak47RenderMesh));
            SIMPLE_LOG("HyperbourneGame: AK-47 mesh created");
        }
    } catch (const std::exception& e) {
        SIMPLE_LOG("ERROR: Failed to create AK-47 mesh: " + std::string(e.what()));
    }

    // Knife (using MeleeWeapon preset with Type=0)
    try {
        auto knifeGen = MeleeWeapon(m_Seed.Derive(400001), 0);
        auto knifeMesh = knifeGen(m_Seed.Derive(400001));
        auto knifeRenderMesh = ConvertToRenderMesh(knifeMesh);
        if (knifeRenderMesh) {
            m_KnifeMeshID = m_MeshLibrary->AddMesh(std::move(knifeRenderMesh));
            SIMPLE_LOG("HyperbourneGame: Knife mesh created");
        }
    } catch (const std::exception& e) {
        SIMPLE_LOG("ERROR: Failed to create Knife mesh: " + std::string(e.what()));
    }

    // Create bullet mesh (9mm for AK-47)
    try {
        auto bulletGen = Bullet(m_Seed.Derive(400002), 9.0f);
        auto bulletMesh = bulletGen(m_Seed.Derive(400002));
        auto bulletRenderMesh = ConvertToRenderMesh(bulletMesh);
        if (bulletRenderMesh) {
            m_BulletMeshID = m_MeshLibrary->AddMesh(std::move(bulletRenderMesh));
            SIMPLE_LOG("HyperbourneGame: Bullet mesh created");
        }
    } catch (const std::exception& e) {
        SIMPLE_LOG("ERROR: Failed to create Bullet mesh: " + std::string(e.what()));
    }

    SIMPLE_LOG("HyperbourneGame: Weapon meshes created");
}

void HyperbourneGame::InitializePlayer() {
    FPSGame::InitializePlayer();

    // Add Health component for HUD rendering
    if (m_PlayerEntity != 0) {
        Game::Health PlayerHealth;
        PlayerHealth.CurrentHealth = 100.0f;
        PlayerHealth.MaxHealth = 100.0f;
        m_Registry.Add<Game::Health>(m_PlayerEntity, PlayerHealth);

        // Add Transform component (required by FPSMovementSystem)
        if (!m_Registry.Has<ECS::Transform>(m_PlayerEntity)) {
            ECS::Transform transform;
            transform.Position = Math::Vec3(0, 1.75f, 0);
            transform.Scale = Math::Vec3(1, 1, 1);
            transform.Matrix = Math::Matrix4::Identity();
            m_Registry.Add<ECS::Transform>(m_PlayerEntity, transform);
        }
    }

    // Spawn at origin
    float SpawnHeight = 2.0f;
    Math::Vec3 SpawnPosition = Math::Vec3(0.0f, SpawnHeight, 0.0f);

    // Customize FPS movement
    if (m_Registry.Has<Game::FPSMovement>(m_PlayerEntity)) {
        auto& Movement = m_Registry.Get<Game::FPSMovement>(m_PlayerEntity);
        Movement.MaxGroundSpeed = 7.0f;
        Movement.GroundAcceleration = 20.0f;
        Movement.GroundFriction = 6.0f;
        Movement.AirAcceleration = 10.0f;
    }

    if (m_Registry.Has<Physics::RigidBody>(m_PlayerEntity)) {
        auto& RB = m_Registry.Get<Physics::RigidBody>(m_PlayerEntity);
        RB.Position = SpawnPosition;
        RB.Friction = 0.6f;
        RB.LinearDamping = 0.05f;

        // Force sync player body to ReactPhysics3D immediately
        Physics::ReactPhysics3DBridge& Bridge = Physics::PhysicsSystem::Instance().GetBridge();
        Bridge.SyncToReactPhysics3D();
    }

    m_Camera.Position = SpawnPosition;
    m_Camera.Zoom = 75.0f;
    m_Camera.ProcessMouseMovement(0, 0, true);
}

void HyperbourneGame::HandleInput() {
    // Input is primarily handled through callbacks
}

void HyperbourneGame::UpdateFPSMovement(float DeltaTime) {
    if (m_PlayerEntity == 0) return;

    // Get input
    Math::Vec3 moveDir(0, 0, 0);
    bool jump = false;
    bool crouch = false;
    bool sprint = false;

    if (m_InputManager) {
        // WASD (QWERTY) and ZQSD (AZERTY) movement support
        // Forward: W (26) for QWERTY or Z (29) for AZERTY (same physical key position)
        if (m_InputManager->IsKeyPressed(26) || m_InputManager->IsKeyPressed(29)) moveDir.z += 1.0f;
        // Backward: S (22) - same in both layouts
        if (m_InputManager->IsKeyPressed(22)) moveDir.z -= 1.0f;
        // Left: A (4) for QWERTY or Q (20) for AZERTY
        if (m_InputManager->IsKeyPressed(4) || m_InputManager->IsKeyPressed(20)) moveDir.x -= 1.0f;
        // Right: D (7) - same in both layouts
        if (m_InputManager->IsKeyPressed(7)) moveDir.x += 1.0f;
        if (m_InputManager->IsKeyPressed(44)) jump = true;
        if (m_InputManager->IsKeyPressed(29)) crouch = true;
        if (m_InputManager->IsKeyPressed(225)) sprint = true;
    }

    // Process movement
    Game::FPSMovementSystem::ProcessInput(m_Registry, m_PlayerEntity, moveDir, jump, crouch, sprint);
    Game::FPSMovementSystem::Update(m_Registry, DeltaTime, m_Camera, nullptr);
}

void HyperbourneGame::HandleWindowResize(int W, int H) {
    m_PendingResize = true;
    m_PendingWidth = W;
    m_PendingHeight = H;
    m_LastResizeEvent = std::chrono::high_resolution_clock::now();
}

void HyperbourneGame::HandleKeyInput(int Key, int Scancode, int Action, int Mods) {
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
            } else {
                WindowPtr->SetFullscreen(true);
                WindowPtr->SetResizable(true);
                m_IsFullscreen = true;
                m_IsMaximized = false;
            }
        }

        // ESC - Handle popup menus and pause
        if (Key == KEY_ESCAPE) {
            if (m_GameState) {
                auto currentState = m_GameState->GetCurrentState();

                // If settings menu is open, close it first
                if (m_ShowSettingsMenu) {
                    m_ShowSettingsMenu = false;
                    // If we were in pause menu, stay paused
                    if (currentState == Solstice::Game::GameStateType::Paused) {
                        // Settings closed, stay in pause menu
                        return;
                    }
                }

                if (currentState == Solstice::Game::GameStateType::Playing) {
                    // Pause the game
                    m_GameState->SetState(Solstice::Game::GameStateType::Paused);
                    if (m_PauseMenu) {
                        m_PauseMenu->Show();
                    }
                    // Release mouse lock when pausing
                    m_MouseLocked = false;
                    WindowPtr->SetRelativeMouse(false);
                    WindowPtr->SetCursorGrab(false);
                    WindowPtr->ShowCursor(true);
                    if (m_Renderer) {
                        m_Renderer->SetShowCrosshair(false);
                    }
                } else if (currentState == Solstice::Game::GameStateType::Paused) {
                    // Resume the game
                    m_GameState->SetState(Solstice::Game::GameStateType::Playing);
                    if (m_PauseMenu) {
                        m_PauseMenu->Hide();
                        m_PauseMenu->SetSettingsOpen(false);
                    }
                    m_ShowSettingsMenu = false;
                } else if (m_MouseLocked) {
                    // In other states, just release mouse lock
                    m_MouseLocked = false;
                    WindowPtr->SetRelativeMouse(false);
                    WindowPtr->SetCursorGrab(false);
                    WindowPtr->ShowCursor(true);
                    if (m_Renderer) {
                        m_Renderer->SetShowCrosshair(false);
                    }
                }
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
    }
}

void HyperbourneGame::HandleMouseButton(int Button, int Action, int Mods) {
    (void)Mods;
    auto* WindowPtr = GetWindow();
    if (!WindowPtr) return;

    // Left mouse button - Fire weapon
    if (Button == 1 && Action == 1 && m_PlayerEntity != 0) { // Left click, press
        using namespace Solstice::Game;

        // Get current weapon from WeaponSwitcher
        Weapon* currentWeapon = WeaponSwitcherSystem::GetCurrentWeapon(m_Registry, m_PlayerEntity);
        if (!currentWeapon) {
            // Try to get weapon from Item system
            int weaponIndex = WeaponSwitcherSystem::GetCurrentWeaponIndex(m_Registry, m_PlayerEntity);
            if (weaponIndex >= 0 && m_Registry.Has<WeaponSwitcher>(m_PlayerEntity)) {
                auto& switcher = m_Registry.Get<WeaponSwitcher>(m_PlayerEntity);
                if (weaponIndex < static_cast<int>(switcher.WeaponInventory.size())) {
                    uint32_t weaponItemID = switcher.WeaponInventory[weaponIndex];
                    Item* weaponItem = InventorySystem::GetItem(m_Registry, m_PlayerEntity, weaponItemID);
                    if (weaponItem) {
                        // Create temporary Weapon component for firing
                        Weapon tempWeapon;
                        tempWeapon.Type = static_cast<WeaponType>(static_cast<int>(weaponItem->Properties["WeaponType"]));
                        tempWeapon.Damage = weaponItem->Properties["Damage"];
                        tempWeapon.FireRate = weaponItem->Properties.count("FireRate") > 0 ? weaponItem->Properties["FireRate"] : 10.0f;
                        tempWeapon.Range = weaponItem->Properties.count("Range") > 0 ? weaponItem->Properties["Range"] : 100.0f;
                        tempWeapon.IsHitscan = false; // Use projectiles
                        tempWeapon.ProjectileSpeed = 500.0f;
                        tempWeapon.FireSoundPath = weaponItemID == 1001 ? "assets/ar_fire.wav" : "assets/knife_slash.wav";

                        // Fire in camera direction
                        Math::Vec3 fireDirection = m_Camera.Front;
                        SpawnBullet(m_PlayerEntity, fireDirection, tempWeapon);
                    }
                }
            }
        } else {
            // Fire with Weapon component
            Math::Vec3 fireDirection = m_Camera.Front;
            WeaponSystem::Fire(m_Registry, m_PlayerEntity, fireDirection);
        }
    }

    if (Button == 2 && Action == 1) { // Right mouse button
        m_MouseLocked = !m_MouseLocked;
        WindowPtr->SetRelativeMouse(m_MouseLocked);
        WindowPtr->SetCursorGrab(m_MouseLocked);
        WindowPtr->ShowCursor(!m_MouseLocked);
        m_Renderer->SetShowCrosshair(m_MouseLocked);
    }
}

void HyperbourneGame::HandleCursorPos(double DX, double DY) {
    auto* WindowPtr = GetWindow();
    if (!WindowPtr) return;

    if (m_MouseLocked) {
        m_Camera.ProcessMouseMovement(static_cast<float>(DX), static_cast<float>(-DY));
    } else {
        auto FBSize = WindowPtr->GetFramebufferSize();
        m_MouseX = static_cast<float>(DX);
        m_MouseY = static_cast<float>(DY);
    }
}

void HyperbourneGame::InitializeProceduralTextures() {
    using namespace Solstice::Arzachel;
    using namespace Solstice::Arzachel::MaterialPresets;

    // Prepare texture configurations with custom generators
    std::vector<Game::TextureConfig> TextureConfigs;

    // Main textures (512x512) - Early '00s aesthetic
    TextureConfigs.push_back({"Concrete", 512, 7, 6.0f, 10000, []() {
        return ConcreteData(Arzachel::Seed(10000), 512);
    }});
    TextureConfigs.push_back({"Metal", 512, 7, 8.0f, 15000, []() {
        return MetalData(Arzachel::Seed(15000), 512);
    }});
    TextureConfigs.push_back({"Plastic", 512, 6, 4.0f, 20000, []() {
        return PlasticData(Arzachel::Seed(20000), 512);
    }});
    TextureConfigs.push_back({"Glass", 256, 4, 2.0f, 25000, []() {
        return GlassData(Arzachel::Seed(25000), 256);
    }});
    TextureConfigs.push_back({"Rubber", 256, 5, 3.0f, 30000, []() {
        return RubberData(Arzachel::Seed(30000), 256);
    }});

    // Initialize texture manager
    m_TextureManager.Initialize(TextureConfigs, "cache");
}

void HyperbourneGame::InitializeSkybox() {
    m_Skybox = std::make_unique<Skybox>();
    m_Skybox->Initialize(512);

    // Set industrial/urban preset
    m_Skybox->SetPreset(Render::SkyPreset::Overcast);

    if (m_Renderer) {
        m_Renderer->SetSkybox(m_Skybox.get());
    }
}

void HyperbourneGame::InitializeScene() {
    // Create weapon meshes now that MeshLibrary is ready (only once)
    if (m_AK47MeshID == 0 && m_MeshLibrary) {
        CreateWeaponMeshes();
    }

    // Scene will be initialized when level is loaded
    if (!m_LevelLoaded && m_CurrentLevelName.empty()) {
        // Don't initialize scene until level is selected
        return;
    }

    InitializeCurrentLevel();
    m_Scene.UpdateTransforms();
    SIMPLE_LOG("Hyperbourne: Scene initialized");
}

void HyperbourneGame::InitializeCurrentLevel() {
    if (m_CurrentLevelName == "Endless Mode") {
        InitializeEndlessMode();
    }
}

void HyperbourneGame::LoadLevel(const std::string& LevelName) {
    m_CurrentLevelName = LevelName;
    m_LevelLoaded = false;

    // Note: Scene doesn't have Clear() - objects will be added to existing scene
    // In a full implementation, we'd track and remove level-specific objects

    // Stop menu music before entering gameplay
    try {
        Core::Audio::AudioManager::Instance().StopMusic();
    } catch (...) {
        // Non-fatal if audio fails
    }

    // Change game state to Playing
    if (m_GameState) {
        m_GameState->SetState(Solstice::Game::GameStateType::Playing);
    }

    // Initialize scene if textures are ready
    if (m_TextureManager.IsReady()) {
        InitializeScene();
        m_LevelLoaded = true;
        m_SceneInitialized = true;
    }

    // Play level music
    try {
        Core::Audio::AudioManager::Instance().PlayMusic("assets/level_music.wav", -1);
        m_LevelMusicPlaying = true;
        m_MenuMusicPlaying = false;
        SIMPLE_LOG("Level music started");
    } catch (const std::exception& e) {
        SIMPLE_LOG("WARNING: Failed to play level music: " + std::string(e.what()));
        m_LevelMusicPlaying = false;
    } catch (...) {
        SIMPLE_LOG("WARNING: Failed to play level music (unknown error)");
        m_LevelMusicPlaying = false;
    }
}

void HyperbourneGame::InitializeEndlessMode() {
    using namespace Solstice::Arzachel;

    // Helper function to create a static collision box
    auto CreateStaticCollisionBox = [this](const Math::Vec3& Position, const Math::Vec3& HalfExtents, const std::string& Name) {
        auto entityID = m_Registry.Create();
        auto& rb = m_Registry.Add<Physics::RigidBody>(entityID);
        rb.Position = Position;
        rb.Rotation = Math::Quaternion();
        rb.IsStatic = true;
        rb.SetMass(0.0f);
        rb.Type = Physics::ColliderType::Box;
        rb.HalfExtents = HalfExtents;
        rb.Friction = 0.6f;
        rb.Restitution = 0.0f;
        return entityID;
    };

    // Create infinite plane ground (very large collision box)
    CreateStaticCollisionBox(Math::Vec3(0, -0.1f, 0), Math::Vec3(5000.0f, 0.1f, 5000.0f), "EndlessGround");

    // Create visible ground plane mesh
    float groundSize = 10000.0f; // Large ground plane
    auto GroundGen = Plane(groundSize, groundSize, m_Seed.Derive(99999));
    auto GroundMesh = GroundGen(m_Seed.Derive(99999));
    auto GroundRenderMesh = ConvertToRenderMesh(GroundMesh);
    if (GroundRenderMesh) {
        uint32_t GroundMeshID = m_MeshLibrary->AddMesh(std::move(GroundRenderMesh));
        // Position at y=0, use concrete material
        Render::SceneObjectID groundObjID = m_Scene.AddObject(GroundMeshID, Math::Vec3(0, 0, 0), Math::Quaternion(), Math::Vec3(1, 1, 1), Render::ObjectType_Static);
        // Note: Material assignment would be done via SetMaterial if available
        (void)groundObjID;
    }

    // Force sync ground body to ReactPhysics3D immediately
    Physics::PhysicsSystem::Instance().GetBridge().SyncToReactPhysics3D();

    // Procedurally generate ruined buildings around the player
    // Generate buildings in a grid pattern around origin
    int buildingGridSize = 20; // 20x20 grid of potential building locations
    float gridSpacing = 50.0f; // 50 units between building centers

    for (int x = -buildingGridSize/2; x < buildingGridSize/2; ++x) {
        for (int z = -buildingGridSize/2; z < buildingGridSize/2; ++z) {
            // Skip center area where player spawns
            float distFromOrigin = std::sqrt(x*x + z*z) * gridSpacing;
            if (distFromOrigin < 30.0f) continue;

            // Random chance to spawn building (60% chance)
            float randVal = Arzachel::NextFloat(m_Seed.Derive(x * 1000 + z));
            if (randVal > 0.6f) continue;

            float buildingX = x * gridSpacing + (Arzachel::NextFloat(m_Seed.Derive(x * 2000 + z)) - 0.5f) * 10.0f;
            float buildingZ = z * gridSpacing + (Arzachel::NextFloat(m_Seed.Derive(x * 3000 + z)) - 0.5f) * 10.0f;

            // Create ruined wall structure
            auto WallGen = Wall(m_Seed.Derive(x * 10000 + z * 100), 8.0f + Arzachel::NextFloat(m_Seed.Derive(x * 4000 + z)) * 4.0f,
                                4.0f + Arzachel::NextFloat(m_Seed.Derive(x * 5000 + z)) * 2.0f);
            auto WallMesh = WallGen(m_Seed.Derive(x * 10000 + z * 100));
            auto WallRenderMesh = ConvertToRenderMesh(WallMesh);
            if (WallRenderMesh) {
                uint32_t WallMeshID = m_MeshLibrary->AddMesh(std::move(WallRenderMesh));
                float angle = Arzachel::NextFloat(m_Seed.Derive(x * 6000 + z)) * 2.0f * 3.14159f;
                Math::Quaternion rot = Math::Quaternion::FromEuler(0, angle, 0);
                m_Scene.AddObject(WallMeshID, Math::Vec3(buildingX, 0, buildingZ), rot, Math::Vec3(1, 1, 1), Render::ObjectType_Static);
            }
        }
    }

    // Spawn destroyed vehicles using industrial presets
    int vehicleCount = 30;
    for (int i = 0; i < vehicleCount; ++i) {
        float angle = Arzachel::NextFloat(m_Seed.Derive(10000 + i)) * 2.0f * 3.14159f;
        float radius = 40.0f + Arzachel::NextFloat(m_Seed.Derive(20000 + i)) * 300.0f;
        float x = std::cos(angle) * radius;
        float z = std::sin(angle) * radius;
        float rotY = Arzachel::NextFloat(m_Seed.Derive(30000 + i)) * 2.0f * 3.14159f;

        // Use industrial presets for destroyed vehicles (simplified - using boxes/pipes as placeholders)
        auto PipeGen = Pipe(m_Seed.Derive(40000 + i), 2.0f + Arzachel::NextFloat(m_Seed.Derive(50000 + i)) * 3.0f, 0.2f);
        auto PipeMesh = PipeGen(m_Seed.Derive(40000 + i));
        auto PipeRenderMesh = ConvertToRenderMesh(PipeMesh);
        if (PipeRenderMesh) {
            uint32_t PipeMeshID = m_MeshLibrary->AddMesh(std::move(PipeRenderMesh));
            Math::Quaternion rot = Math::Quaternion::FromEuler(
                Arzachel::NextFloat(m_Seed.Derive(60000 + i)) * 0.5f,
                rotY,
                Arzachel::NextFloat(m_Seed.Derive(70000 + i)) * 0.5f
            );
            m_Scene.AddObject(PipeMeshID, Math::Vec3(x, 0.5f, z), rot, Math::Vec3(1, 1, 1), Render::ObjectType_Static);
        }
    }

    // Update skybox for black-crimson rupture effect
    if (m_Skybox) {
        // Set a darker, more ominous sky preset
        m_Skybox->SetPreset(Render::SkyPreset::Overcast);
        // Note: Full sky rupture shader effect would require skybox modification
        // For now, using overcast preset as placeholder
    }

    // Spawn Archons procedurally around the player
    // Spawn Cruciforms (swarming enemies)
    int cruciformCount = 15;
    for (int i = 0; i < cruciformCount; ++i) {
        float angle = Arzachel::NextFloat(m_Seed.Derive(80000 + i)) * 2.0f * 3.14159f;
        float radius = 30.0f + Arzachel::NextFloat(m_Seed.Derive(90000 + i)) * 100.0f;
        float x = std::cos(angle) * radius;
        float z = std::sin(angle) * radius;
        float y = 1.0f; // Spawn slightly above ground
        Math::Vec3 pos(x, y, z);

        ECS::EntityId cruciformEntity = Hyperbourne::ArchonSystem::SpawnArchon(m_Registry, pos, Hyperbourne::ArchonType::Cruciform);
        SpawnArchonWithVisual(cruciformEntity, pos, Hyperbourne::ArchonType::Cruciform);
    }

    // Spawn Shardhives (less frequently, bigger enemies)
    int shardhiveCount = 3;
    for (int i = 0; i < shardhiveCount; ++i) {
        float angle = Arzachel::NextFloat(m_Seed.Derive(100000 + i)) * 2.0f * 3.14159f;
        float radius = 80.0f + Arzachel::NextFloat(m_Seed.Derive(110000 + i)) * 150.0f;
        float x = std::cos(angle) * radius;
        float z = std::sin(angle) * radius;
        float y = 2.0f; // Spawn higher (they have wings)
        Math::Vec3 pos(x, y, z);

        ECS::EntityId shardhiveEntity = Hyperbourne::ArchonSystem::SpawnArchon(m_Registry, pos, Hyperbourne::ArchonType::Shardhive);
        SpawnArchonWithVisual(shardhiveEntity, pos, Hyperbourne::ArchonType::Shardhive);
    }

    SIMPLE_LOG("Endless Mode level initialized");
}

uint32_t HyperbourneGame::CreateCruciformMesh() {
    using namespace Solstice::Arzachel;
    using namespace Solstice::Math;

    // Create upside-down cross shape
    // Vertical bar (tall, thin) - wider at top
    float verticalWidth = 0.15f;
    float verticalHeight = 0.8f;
    float verticalTopWidth = 0.25f; // Wider at top for upside-down cross
    auto verticalBar = Scale(Cube(1.0f, m_Seed.Derive(200000)), Vec3(verticalTopWidth, verticalHeight, verticalWidth));

    // Horizontal bar (wide, thin)
    float horizontalWidth = 0.6f;
    float horizontalHeight = 0.15f;
    auto horizontalBar = Scale(Cube(1.0f, m_Seed.Derive(200001)), Vec3(horizontalWidth, horizontalHeight, verticalWidth));
    horizontalBar = Transform(horizontalBar, Matrix4::Translation(Vec3(0, -verticalHeight * 0.5f + horizontalHeight * 0.5f, 0)));

    // Merge both bars
    auto crossMesh = Merge(verticalBar, horizontalBar);
    auto meshData = crossMesh(m_Seed.Derive(200000));
    auto renderMesh = ConvertToRenderMesh(meshData);

    if (renderMesh) {
        return m_MeshLibrary->AddMesh(std::move(renderMesh));
    }
    return 0;
}

uint32_t HyperbourneGame::CreateShardhiveMesh() {
    using namespace Solstice::Arzachel;
    using namespace Solstice::Math;

    // Create diamond/hexagon shape using octahedron-like structure
    // Start with an icosphere with low subdivision for diamond shape
    float size = 1.2f;
    auto diamond = Icosphere(size, 0, m_Seed.Derive(300000)); // 0 subdivisions = octahedron

    // Add wing-like protrusions
    float wingSize = 0.8f;
    float wingThickness = 0.1f;
    auto wing1 = Scale(Cube(1.0f, m_Seed.Derive(300001)), Vec3(wingSize, wingThickness, wingThickness));
    wing1 = Transform(wing1, Matrix4::Translation(Vec3(size * 0.7f, 0, 0)));
    auto wing2 = Scale(Cube(1.0f, m_Seed.Derive(300002)), Vec3(wingSize, wingThickness, wingThickness));
    wing2 = Transform(wing2, Matrix4::Translation(Vec3(-size * 0.7f, 0, 0)));
    auto wing3 = Scale(Cube(1.0f, m_Seed.Derive(300003)), Vec3(wingThickness, wingThickness, wingSize));
    wing3 = Transform(wing3, Matrix4::Translation(Vec3(0, 0, size * 0.7f)));
    auto wing4 = Scale(Cube(1.0f, m_Seed.Derive(300004)), Vec3(wingThickness, wingThickness, wingSize));
    wing4 = Transform(wing4, Matrix4::Translation(Vec3(0, 0, -size * 0.7f)));

    // Merge all parts
    auto shardhiveMesh = Merge(diamond, wing1);
    shardhiveMesh = Merge(shardhiveMesh, wing2);
    shardhiveMesh = Merge(shardhiveMesh, wing3);
    shardhiveMesh = Merge(shardhiveMesh, wing4);

    auto meshData = shardhiveMesh(m_Seed.Derive(300000));
    auto renderMesh = ConvertToRenderMesh(meshData);

    if (renderMesh) {
        return m_MeshLibrary->AddMesh(std::move(renderMesh));
    }
    return 0;
}

void HyperbourneGame::SpawnArchonWithVisual(ECS::EntityId Entity, const Math::Vec3& Position, Hyperbourne::ArchonType Type) {
    // Create visual mesh based on type
    uint32_t meshID = 0;
    if (Type == Hyperbourne::ArchonType::Cruciform) {
        meshID = CreateCruciformMesh();
    } else {
        meshID = CreateShardhiveMesh();
    }

    if (meshID != 0) {
        // Add mesh to scene
        Render::SceneObjectID sceneObjectID = m_Scene.AddObject(meshID, Position, Math::Quaternion(), Math::Vec3(1, 1, 1), Render::ObjectType_Dynamic);

        // Store scene object ID in Archon component
        if (m_Registry.Has<Hyperbourne::Archon>(Entity)) {
            auto& archon = m_Registry.Get<Hyperbourne::Archon>(Entity);
            archon.SceneObjectID = sceneObjectID;
            // Note: Material assignment would be done via SetMaterial if available
        }
    }
}

// Removed - replaced by InitializeEndlessMode()
// Old CERN and Geneva Ruins level implementations have been removed

void HyperbourneGame::InitializeAudio() {
    // Register audio loader with AssetStreamer
    m_AssetStreamer.SetLoader(Core::AssetType::Audio, [](const std::string& Path) -> std::shared_ptr<Core::AssetData> {
        auto Asset = std::make_shared<Core::AssetData>(Core::AssetType::Audio, Path);
        Asset->Path = Path;
        return Asset;
    });

    // Request audio assets (async loading)
    m_MenuMusicHandle = m_AssetStreamer.RequestAsset(
        Core::AssetType::Audio, "assets/menu_music.wav");
    m_LevelMusicHandle = m_AssetStreamer.RequestAsset(
        Core::AssetType::Audio, "assets/level_music.wav");

    // Prefetch assets
    m_AssetStreamer.PrefetchAsset(Core::AssetType::Audio, "assets/menu_music.wav");
    m_AssetStreamer.PrefetchAsset(Core::AssetType::Audio, "assets/level_music.wav");
}

void HyperbourneGame::FinalizeProceduralTextures() {
    if (!m_TextureManager.IsReady()) {
        return;
    }

    // Finalize textures (register them)
    std::vector<Game::MaterialAssignment> MaterialAssignments;
    MaterialAssignments.resize(5);

    // Concrete material - HL2 Beta aesthetic: darker, more desaturated
    MaterialAssignments[0] = {m_ConcreteMaterialHandle.GetValue(), 0, 0xFFFF, 0xFFFF,
                              Math::Vec3(0.35f, 0.35f, 0.38f), 0.85f, // Darker base, higher roughness
                              static_cast<uint8_t>(Core::TextureBlendMode::Multiply), 0.5f, "assets/materials/concrete.json"};

    // Metal material - Industrial, slightly reflective
    MaterialAssignments[1] = {m_MetalMaterialHandle.GetValue(), 1, 0xFFFF, 0xFFFF,
                              Math::Vec3(0.28f, 0.32f, 0.36f), 0.25f, // Darker, more reflective
                              static_cast<uint8_t>(Core::TextureBlendMode::Multiply), 0.5f, "assets/materials/metal.json"};

    // Plastic material - Muted beige-gray
    MaterialAssignments[2] = {m_PlasticMaterialHandle.GetValue(), 2, 0xFFFF, 0xFFFF,
                              Math::Vec3(0.35f, 0.33f, 0.31f), 0.55f, // Darker, slightly more reflective
                              static_cast<uint8_t>(Core::TextureBlendMode::Multiply), 0.4f, "assets/materials/plastic.json"};

    // Glass material
    MaterialAssignments[3] = {m_GlassMaterialHandle.GetValue(), 3, 0xFFFF, 0xFFFF,
                              Math::Vec3(0.9f, 0.95f, 1.0f), 0.1f,
                              static_cast<uint8_t>(Core::TextureBlendMode::Multiply), 0.3f, "assets/materials/glass.json"};

    // Rubber material
    MaterialAssignments[4] = {m_RubberMaterialHandle.GetValue(), 4, 0xFFFF, 0xFFFF,
                               Math::Vec3(0.1f, 0.1f, 0.1f), 0.9f,
                               static_cast<uint8_t>(Core::TextureBlendMode::Multiply), 0.5f, "assets/materials/rubber.json"};

    // Finalize textures and assign to materials (this already updates materials)
    // Note: Finalize() handles material updates internally, so we don't need to update them again
    m_TextureManager.Finalize(m_Renderer.get(), m_MaterialLibrary.get(), MaterialAssignments);

    SIMPLE_LOG("Procedural textures finalized and materials updated");
}

void HyperbourneGame::ProcessPendingTextures() {
    // Process pending texture data (create bgfx textures on main thread)
    if (m_TextureManager.ProcessPendingTextures()) {
        m_FirstFrameRendered = true;
    }
}

void HyperbourneGame::Update(float DeltaTime) {
    // Update base FPSGame systems FIRST
    FPSGame::Update(DeltaTime);

    // Handle music based on game state
    if (m_GameState) {
        auto currentState = m_GameState->GetCurrentState();

        // Play menu music when in main menu (only once)
        if (currentState == Solstice::Game::GameStateType::MainMenu && !m_MenuMusicPlaying) {
            try {
                Core::Audio::AudioManager::Instance().StopMusic(); // Stop any playing music first
                Core::Audio::AudioManager::Instance().PlayMusic("assets/menu_music.wav", -1);
                m_MenuMusicPlaying = true;
                m_LevelMusicPlaying = false;
            } catch (...) {
                SIMPLE_LOG("WARNING: Failed to play menu music");
            }
        } else if (currentState != Solstice::Game::GameStateType::MainMenu && m_MenuMusicPlaying) {
            // Stop menu music when leaving main menu
            try {
                Core::Audio::AudioManager::Instance().StopMusic();
                m_MenuMusicPlaying = false;
            } catch (...) {
                // Non-fatal
            }
        }

        // Play level music when in Playing state (if not already playing)
        if (currentState == Solstice::Game::GameStateType::Playing && !m_LevelMusicPlaying && m_LevelLoaded) {
            try {
                Core::Audio::AudioManager::Instance().StopMusic(); // Stop any playing music first
                Core::Audio::AudioManager::Instance().PlayMusic("assets/level_music.wav", -1);
                m_LevelMusicPlaying = true;
                m_MenuMusicPlaying = false;
                SIMPLE_LOG("Level music started in Update");
            } catch (const std::exception& e) {
                SIMPLE_LOG("WARNING: Failed to play level music in Update: " + std::string(e.what()));
            } catch (...) {
                SIMPLE_LOG("WARNING: Failed to play level music in Update (unknown error)");
            }
        }
    }

    // Update physics system AFTER movement
    if (m_SceneInitialized) {
        Physics::PhysicsSystem::Instance().Update(DeltaTime);

        // Re-apply horizontal velocity after physics runs
        if (m_PlayerEntity != 0 && m_Registry.Has<Game::FPSMovement>(m_PlayerEntity) &&
            m_Registry.Has<Physics::RigidBody>(m_PlayerEntity)) {
            auto& Movement = m_Registry.Get<Game::FPSMovement>(m_PlayerEntity);
            auto& RB = m_Registry.Get<Physics::RigidBody>(m_PlayerEntity);

            Physics::ReactPhysics3DBridge& Bridge = Physics::PhysicsSystem::Instance().GetBridge();
            Bridge.SyncFromReactPhysics3D();

            // Hardcoded collision constraints to force player to stay inside level bounds
            // Define level bounds matching boundary walls
            float levelBoundaryX = 30.0f;
            float levelBoundaryZ = 12.0f;
            float boundaryThickness = 1.0f;
            float maxX = levelBoundaryX - boundaryThickness; // Within boundary walls
            float minX = -levelBoundaryX + boundaryThickness; // Within boundary walls
            float maxZ = levelBoundaryZ - boundaryThickness; // Within boundary walls
            float minZ = -levelBoundaryZ + boundaryThickness; // Within boundary walls
            float minY = 0.5f; // Above ground (player capsule radius + margin)
            float maxY = 3.5f; // Below ceiling (matching corridor height)

            // Force player back inside bounds if they try to clip through
            Math::Vec3 correction(0, 0, 0);
            bool needsCorrection = false;

            // X bounds
            if (RB.Position.x > maxX) {
                correction.x = maxX - RB.Position.x;
                needsCorrection = true;
            } else if (RB.Position.x < minX) {
                correction.x = minX - RB.Position.x;
                needsCorrection = true;
            }

            // Z bounds (check if outside corridor or rooms)
            if (RB.Position.z > maxZ) {
                correction.z = maxZ - RB.Position.z;
                needsCorrection = true;
            } else if (RB.Position.z < minZ) {
                correction.z = minZ - RB.Position.z;
                needsCorrection = true;
            }

            // Y bounds (prevent falling through or going above ceiling)
            if (RB.Position.y < minY) {
                correction.y = minY - RB.Position.y;
                RB.Velocity.y = 0; // Stop falling
                needsCorrection = true;
            } else if (RB.Position.y > maxY) {
                correction.y = maxY - RB.Position.y;
                RB.Velocity.y = 0; // Stop rising
                needsCorrection = true;
            }

            // Apply correction if needed
            if (needsCorrection) {
                RB.Position = RB.Position + correction;
                // Cancel velocity in the direction of the wall
                if (std::abs(correction.x) > 0.01f) RB.Velocity.x = 0;
                if (std::abs(correction.z) > 0.01f) RB.Velocity.z = 0;
                if (std::abs(correction.y) > 0.01f) RB.Velocity.y = 0;
            }

            RB.Velocity.x = Movement.DesiredHorizontalVelocity.x;
            RB.Velocity.z = Movement.DesiredHorizontalVelocity.z;

            Bridge.SyncToReactPhysics3D();
        }
    }

    auto* WindowPtr = GetWindow();
    if (!WindowPtr) return;

    // Update script manager
    m_ScriptManager.Update(DeltaTime);

    // Update weapon system
    Game::WeaponSystem::Update(m_Registry, DeltaTime);
    Game::WeaponSwitcherSystem::Update(m_Registry, DeltaTime);

    // Update bullets
    UpdateBullets(DeltaTime);

    // Update Archon system (if in endless mode)
    if (m_SceneInitialized && m_CurrentLevelName == "Endless Mode" && m_PlayerEntity != 0) {
        Hyperbourne::ArchonSystem::Update(m_Registry, DeltaTime, m_PlayerEntity);

        // Sync Archon positions with scene objects
        m_Registry.ForEach<Hyperbourne::Archon>([this](ECS::EntityId entity, Hyperbourne::Archon& archon) {
            if (archon.SceneObjectID != 0 && m_Registry.Has<ECS::Transform>(entity)) {
                auto& transform = m_Registry.Get<ECS::Transform>(entity);
                // Update scene object transform
                Math::Quaternion rot = Math::Quaternion(); // Extract from Matrix if needed
                m_Scene.SetTransform(archon.SceneObjectID, transform.Position, rot, transform.Scale);
            }
        });
    }

    // Check if textures are ready and finalize them (only once)
    if (m_TextureManager.IsReady() && !m_TexturesFinalized) {
        try {
            FinalizeProceduralTextures();
            m_TexturesFinalized = true;
        } catch (const std::exception& e) {
            SIMPLE_LOG("ERROR: Exception in FinalizeProceduralTextures: " + std::string(e.what()));
        } catch (...) {
            SIMPLE_LOG("ERROR: Unknown exception in FinalizeProceduralTextures");
        }
    }

    // Initialize scene after textures are ready
    if (!m_SceneInitialized && m_LevelLoaded && m_TextureManager.IsReady()) {
        try {
            InitializeScene();
            m_SceneInitialized = true;

            // Re-sync player physics body after level loads to ensure it's properly initialized
            if (m_PlayerEntity != 0 && m_Registry.Has<Physics::RigidBody>(m_PlayerEntity)) {
                auto& RB = m_Registry.Get<Physics::RigidBody>(m_PlayerEntity);
                // Ensure player is at spawn height (above ground at y=0)
                float SpawnHeight = 2.0f;
                RB.Position = Math::Vec3(0.0f, SpawnHeight, 0.0f);
                RB.Velocity = Math::Vec3(0, 0, 0);

                // Ensure player body is dynamic and properly configured
                RB.IsStatic = false;
                RB.SetMass(67.0f);
                RB.GravityScale = 1.0f;

                Physics::ReactPhysics3DBridge& Bridge = Physics::PhysicsSystem::Instance().GetBridge();
                Bridge.SyncToReactPhysics3D();
                SIMPLE_LOG("Player physics body re-synced after level load at position (" +
                          std::to_string(RB.Position.x) + ", " +
                          std::to_string(RB.Position.y) + ", " +
                          std::to_string(RB.Position.z) + ")");
            }
        } catch (const std::exception& e) {
            SIMPLE_LOG("ERROR: Exception in InitializeScene: " + std::string(e.what()));
        } catch (...) {
            SIMPLE_LOG("ERROR: Unknown exception in InitializeScene");
        }
    }

    // Initialize audio listener
    if (m_SceneInitialized && !m_AudioStarted) {
        try {
            Core::Audio::Listener listener;
            listener.Position = m_Camera.Position;
            listener.Forward = m_Camera.Front;
            listener.Up = m_Camera.Up;
            listener.TargetReverb = Core::Audio::ReverbPresetType::None;
            Core::Audio::AudioManager::Instance().SetListener(listener);
            m_AudioStarted = true;
        } catch (const std::exception& e) {
            SIMPLE_LOG("ERROR: Exception setting up audio: " + std::string(e.what()));
            m_AudioStarted = false;
        } catch (...) {
            SIMPLE_LOG("ERROR: Unknown exception setting up audio");
            m_AudioStarted = false;
        }
    }

    // Sync camera position from physics body
    if (m_TextureManager.IsReady() && m_SceneInitialized && m_PlayerEntity != 0) {
        try {
            if (m_Registry.Has<Physics::RigidBody>(m_PlayerEntity)) {
                auto& PlayerRB = m_Registry.Get<Physics::RigidBody>(m_PlayerEntity);
                if (std::isfinite(PlayerRB.Position.x) && std::isfinite(PlayerRB.Position.y) && std::isfinite(PlayerRB.Position.z)) {
                    float HeadOffset = 1.75f * 0.5f - 0.1f;
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

    // Update Audio Listener
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
}

void HyperbourneGame::Render() {
    // Begin new ImGui frame immediately
    if (UISystem::Instance().IsInitialized()) {
        UISystem::Instance().NewFrame();
    }

    // Process pending textures in frame context
    ProcessPendingTextures();

    // Hide main menu if loading screen is active (only check texture readiness, not scene initialization)
    // Scene initialization only matters when actually playing a level, not in main menu
    bool isUIBusy = !m_TextureManager.IsReady();

    // Check current game state
    bool isMainMenu = m_GameState && m_GameState->GetCurrentState() == Solstice::Game::GameStateType::MainMenu;
    bool isPaused = m_GameState && m_GameState->GetCurrentState() == Solstice::Game::GameStateType::Paused;

    // Disable debug overlay in main menu to prevent it from rendering on top of UI
    if (m_Renderer) {
        if (isMainMenu || isPaused) {
            m_Renderer->SetShowDebugOverlay(false);
        } else if (m_ShowDebugOverlay) {
            // Re-enable if it was previously enabled (but not in main menu)
            m_Renderer->SetShowDebugOverlay(true);
        }
    }

    if (m_MainMenu) {
        if (isUIBusy) {
            m_MainMenu->Hide();
        } else if (isMainMenu) {
            // Show main menu when textures are ready and we're in MainMenu state
            m_MainMenu->Show();
        }
    }

    // Begin base FPSGame rendering
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
                Vec4 clearColor(0.1f, 0.1f, 0.12f, 1.0f);
                m_Renderer->Clear(clearColor);

                if (m_Scene.GetObjectCount() > 0) {
                    m_Renderer->RenderScene(m_Scene, m_Camera, m_Lights);
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
        m_LoadingScreen->SetTitle("Hyperbourne");
        m_LoadingScreen->SetProgress(m_TextureManager.GetProgress());

        std::string currentOp = m_TextureManager.GetCurrentTextureName();
        if (!currentOp.empty()) {
            currentOp = "GENERATING: " + currentOp;
        } else {
            currentOp = "INITIALIZING...";
        }
        m_LoadingScreen->SetProgressText(currentOp);
    } else if (m_LoadingScreen) {
        m_LoadingScreen->Hide();
    }

    // Update pause menu
    if (m_PauseMenu && isPaused && m_InputManager && m_GameState) {
        m_PauseMenu->Update(m_DeltaTime, *m_InputManager, *m_GameState);
    }

    // Render pause menu
    if (m_PauseMenu && isPaused) {
        auto* window = GetWindow();
        if (window) {
            auto fbSize = window->GetFramebufferSize();
            m_PauseMenu->Render(fbSize.first, fbSize.second);
        }
    }

    // Render level selector if shown (from main menu) - render separately like settings
    // Don't render level selector when paused
    if (m_MainMenu && m_MainMenu->IsLevelSelectorMenuVisible() && m_LevelSelector && !isPaused) {
        auto* window = GetWindow();
        if (window) {
            auto fbSize = window->GetFramebufferSize();
            m_LevelSelector->Render(fbSize.first, fbSize.second);
        }
    }

    // Render settings menu if shown
    if (m_ShowSettingsMenu && m_GamePreferences) {
        auto* window = GetWindow();
        if (window) {
            auto fbSize = window->GetFramebufferSize();
            m_GamePreferences->RenderSettingsMenu(fbSize.first, fbSize.second);

            // Handle ESC to close settings
            if (m_InputManager && m_InputManager->IsKeyJustPressed(27)) {
                m_ShowSettingsMenu = false;
            }
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

void HyperbourneGame::SpawnBullet(ECS::EntityId ShooterEntity, const Math::Vec3& Direction, const Game::Weapon& Weapon) {
    using namespace Solstice::Game;

    if (m_BulletMeshID == 0) return; // No bullet mesh created yet

    // Get shooter position
    Math::Vec3 spawnPos = Math::Vec3(0, 0, 0);
    if (m_Registry.Has<ECS::Transform>(ShooterEntity)) {
        spawnPos = m_Registry.Get<ECS::Transform>(ShooterEntity).Position;
        spawnPos += Direction * 0.5f; // Spawn slightly in front
    }

    // Create bullet entity
    ECS::EntityId bulletEntity = m_Registry.Create();

    // Add Transform
    ECS::Transform transform;
    transform.Position = spawnPos;
    transform.Scale = Math::Vec3(0.1f, 0.1f, 0.1f); // Small bullet
    m_Registry.Add<ECS::Transform>(bulletEntity, transform);

    // Add RigidBody for physics
    Physics::RigidBody rb;
    rb.Position = spawnPos;
    rb.Rotation = Math::Quaternion();
    rb.IsStatic = false;
    rb.SetMass(0.01f); // Very light
    rb.Type = Physics::ColliderType::Sphere;
    rb.HalfExtents = Math::Vec3(0.01f, 0.01f, 0.01f); // Small sphere
    rb.Velocity = Direction * Weapon.ProjectileSpeed;
    rb.Friction = 0.0f;
    rb.Restitution = 0.0f;
    m_Registry.Add<Physics::RigidBody>(bulletEntity, rb);

    // Add visual mesh to scene
    uint32_t sceneObjectID = m_Scene.AddObject(m_BulletMeshID, spawnPos, Math::Quaternion(), Math::Vec3(0.1f, 0.1f, 0.1f), Render::ObjectType_Dynamic);

    // Store bullet data (we'll use a simple component or store in a map)
    // For now, track in m_ActiveProjectiles
    m_ActiveProjectiles.push_back(bulletEntity);

    // Store scene object ID in a simple way (we could add a Projectile component)
    // For now, we'll track bullet lifetime and update positions

    // Play fire sound
    if (!Weapon.FireSoundPath.empty()) {
        using namespace Solstice::Game;
        SFXManager::Instance().PlaySound(Weapon.FireSoundPath, SFXCategory::Combat, 1.0f, false);
    }
}

void HyperbourneGame::UpdateBullets(float DeltaTime) {
    using namespace Solstice::Game;

    // Update bullet positions and check collisions
    auto it = m_ActiveProjectiles.begin();
    while (it != m_ActiveProjectiles.end()) {
        ECS::EntityId bulletEntity = *it;

        if (!m_Registry.Has<ECS::Transform>(bulletEntity) || !m_Registry.Has<Physics::RigidBody>(bulletEntity)) {
            it = m_ActiveProjectiles.erase(it);
            continue;
        }

        auto& transform = m_Registry.Get<ECS::Transform>(bulletEntity);
        auto& rb = m_Registry.Get<Physics::RigidBody>(bulletEntity);

        // Update transform from physics
        transform.Position = rb.Position;

        // Check collision with Archons (enemies)
        bool hit = false;
        m_Registry.ForEach<Hyperbourne::Archon, Game::Health>([&](ECS::EntityId enemyEntity, Hyperbourne::Archon& archon, Game::Health& health) {
            if (hit) return;
            if (!m_Registry.Has<ECS::Transform>(enemyEntity)) return;

            auto& enemyTransform = m_Registry.Get<ECS::Transform>(enemyEntity);
            Math::Vec3 toEnemy = enemyTransform.Position - transform.Position;
            float distance = toEnemy.Length();

            if (distance < 0.5f) { // Hit detection radius
                // Apply damage
                Game::HealthSystem::ApplyDamage(m_Registry, enemyEntity, 28.0f, transform.Position, bulletEntity);
                hit = true;
            }
        });

        // Remove bullet if hit or too far
        if (hit || transform.Position.Length() > 500.0f) {
            // Remove from scene (would need scene object ID tracking)
            m_Registry.Destroy(bulletEntity);
            it = m_ActiveProjectiles.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace Solstice::Hyperbourne

