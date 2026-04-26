#include "FPSGame.hxx"
#include "FPSMovement.hxx"
#include "Weapon.hxx"
#include "../../Core/Debug/Debug.hxx"
#include "../../Physics/Dynamics/RigidBody.hxx"
#include "../../Physics/Integration/PhysicsSystem.hxx"
#include "../../Entity/Transform.hxx"
#include "../../UI/Core/UISystem.hxx"
#include <bgfx/bgfx.h>

namespace Solstice::Game {

FPSGame::FPSGame() {
    m_InputManager = std::make_unique<InputManager>();
    m_GameState = std::make_unique<GameState>();
    m_HUD = std::make_unique<HUD>();
    m_LoadingScreen = std::make_unique<LoadingScreen>();
    m_LoadingScreen->Show(); // Ensure it's showing for the start
    m_MainMenu = std::make_unique<MainMenu>();
    m_PauseMenu = std::make_unique<PauseMenu>();
}

void FPSGame::Initialize() {
    // Initialize Solstice engine systems first
    Solstice::Initialize();

    GameBase::Initialize();
    InitializeFPSSystems();
    InitializePlayer();
    InitializeWeapons();
    ConfigureECSPhases();

    // Start in Main Menu by default
    if (m_GameState) {
        m_GameState->SetState(GameStateType::MainMenu);
    }

    // Setup Main Menu callbacks
    if (m_MainMenu) {
        m_MainMenu->SetNewGameCallback([this]() {
            if (m_GameState) m_GameState->SetState(GameStateType::Playing);
        });
        m_MainMenu->SetQuitCallback([this]() {
            m_ShouldClose = true;
        });
    }

    // Setup Pause Menu callbacks
    if (m_PauseMenu) {
        m_PauseMenu->SetResumeCallback([this]() {
            if (m_GameState) m_GameState->SetState(GameStateType::Playing);
            if (m_PauseMenu) m_PauseMenu->Hide();
        });
        m_PauseMenu->SetSettingsCallback([this]() {
            if (m_PauseMenu) m_PauseMenu->SetSettingsOpen(true);
        });
        m_PauseMenu->SetSaveCallback([this]() {
            // Derived classes can override to implement save
        });
        m_PauseMenu->SetQuitCallback([this]() {
            if (m_GameState) m_GameState->SetState(GameStateType::MainMenu);
            if (m_PauseMenu) {
                m_PauseMenu->Hide();
                m_PauseMenu->SetSettingsOpen(false);
            }
        });
    }
}

void FPSGame::InitializeFPSSystems() {
    // Initialize input manager
    if (m_Window) {
        m_InputManager->Update(m_Window.get());

        // Initialize UI system if not already done and BGFX is ready
        if (!UI::UISystem::Instance().IsInitialized() && bgfx::getRendererType() != bgfx::RendererType::Noop) {
            UI::UISystem::Instance().Initialize(m_Window->NativeWindow());
        }
    }

    // Initialize camera
    m_Camera.Position = Math::Vec3(0, 1.75f, 0);
    m_Camera.WorldUp = Math::Vec3(0, 1, 0);
    m_Camera.Up = Math::Vec3(0, 1, 0);
    m_Camera.Zoom = 75.0f;
    m_Camera.Yaw = -90.0f;
    m_Camera.Pitch = 0.0f;
    m_Camera.ProcessMouseMovement(0, 0, true);

    SIMPLE_LOG("FPSGame: FPS systems initialized");
}

void FPSGame::ConfigureECSPhases() {
    m_ECSScheduler.Clear();
    if (m_EnableMultiplayerECS) {
        if (!m_NetworkSession) {
            m_NetworkSession = std::make_unique<NetworkSessionCoordinator>(m_Registry, MultiplayerPresets::GetCooperative());
        }
        m_NetworkSession->RegisterSystems(m_ECSScheduler);
    }
    m_ECSScheduler.Register(ECS::SystemPhase::Input, "FPSMovementInput", [this](ECS::Registry&, float) {
        if (m_PlayerEntity == 0) {
            return;
        }

        Math::Vec3 moveDir(0, 0, 0);
        bool jump = false;
        bool crouch = false;
        bool sprint = false;
        if (m_InputManager) {
            if (m_InputManager->IsKeyPressed(26) || m_InputManager->IsKeyPressed(25)) moveDir.z += 1.0f;
            if (m_InputManager->IsKeyPressed(22)) moveDir.z -= 1.0f;
            if (m_InputManager->IsKeyPressed(4) || m_InputManager->IsKeyPressed(20)) moveDir.x -= 1.0f;
            if (m_InputManager->IsKeyPressed(7)) moveDir.x += 1.0f;
            if (m_InputManager->IsKeyPressed(44)) jump = true;
            if (m_InputManager->IsKeyPressed(29)) crouch = true;
            if (m_InputManager->IsKeyPressed(225)) sprint = true;
        }
        FPSMovementSystem::ProcessInput(m_Registry, m_PlayerEntity, moveDir, jump, crouch, sprint);
    });

    m_ECSScheduler.Register(ECS::SystemPhase::Simulation, "FPSMovement", [this](ECS::Registry&, float deltaTime) {
        UpdateFPSMovement(deltaTime);
    });

    m_ECSScheduler.Register(ECS::SystemPhase::Simulation, "PhysicsStep", [this](ECS::Registry&, float deltaTime) {
        auto& physics = Physics::PhysicsSystem::Instance();
        if (!physics.IsRunning() || !physics.IsBoundTo(m_Registry)) {
            physics.Start(m_Registry);
        }
        physics.Update(deltaTime);
    });

    m_ECSScheduler.Register(ECS::SystemPhase::Late, "Weapons", [this](ECS::Registry&, float deltaTime) {
        WeaponSystem::Update(m_Registry, deltaTime);
    });
}

void FPSGame::InitializePlayer() {
    m_PlayerEntity = m_Registry.Create();

    // Add FPS movement
    FPSMovement movement;
    FPSMovementSystem::ApplyPreset(movement, "Quake");
    m_Registry.Add<FPSMovement>(m_PlayerEntity, movement);

    // Add Transform component (required by FPSMovementSystem::Update)
    ECS::Transform transform;
    transform.Position = Math::Vec3(0, 1.75f, 0);
    transform.Scale = Math::Vec3(1, 1, 1);
    transform.Matrix = Math::Matrix4::Identity();
    m_Registry.Add<ECS::Transform>(m_PlayerEntity, transform);

    // Add physics body
    auto& rb = m_Registry.Add<Physics::RigidBody>(m_PlayerEntity);
    rb.Position = Math::Vec3(0, 1.75f, 0);
    rb.Type = Physics::ColliderType::Capsule;
    rb.CapsuleHeight = 1.75f;
    rb.CapsuleRadius = 0.3f;
    rb.SetMass(67.0f); // This sets IsStatic = false
    rb.IsStatic = false; // Explicitly ensure it's dynamic
    rb.Friction = 0.6f;
    rb.Restitution = 0.0f;
    rb.GravityScale = 1.0f;
    rb.LinearDamping = 0.05f; // Low damping for responsive movement

    SIMPLE_LOG("FPSGame: Player initialized");
}

void FPSGame::InitializeWeapons() {
    // Derived classes can override to add weapons
    SIMPLE_LOG("FPSGame: Weapons initialized");
}

void FPSGame::Update(float DeltaTime) {
    GameBase::Update(DeltaTime);

    if (m_InputManager && m_Window) {
        m_InputManager->Update(m_Window.get());
    }

    GameStateType currentState = m_GameState ? m_GameState->GetCurrentState() : GameStateType::Playing;

    if (currentState == GameStateType::MainMenu) {
        if (m_MainMenu && m_InputManager && m_GameState) {
            m_MainMenu->Update(DeltaTime, *m_InputManager, *m_GameState);
        }
    } else if (currentState == GameStateType::Playing || currentState == GameStateType::Paused) {
        if (currentState == GameStateType::Paused) {
            if (m_PauseMenu && m_InputManager && m_GameState) {
                m_PauseMenu->Update(DeltaTime, *m_InputManager, *m_GameState);
            }
        } else {
        m_ECSScheduler.ExecuteAll(m_Registry, DeltaTime);
        }

        if (m_GameState) {
            m_GameState->Update(DeltaTime);
        }
    }

    HandleInput();
}

void FPSGame::Render() {
    GameBase::Render();

    auto size = m_Window ? m_Window->GetFramebufferSize() : std::make_pair(1280, 720);
    GameStateType currentState = m_GameState ? m_GameState->GetCurrentState() : GameStateType::Playing;

    if (currentState == GameStateType::MainMenu) {
        if (m_MainMenu) {
            m_MainMenu->Render(size.first, size.second, m_DeltaTime);
        }
    } else if (currentState == GameStateType::Paused) {
        // Render pause menu
        if (m_PauseMenu) {
            m_PauseMenu->Render(size.first, size.second);
        }
    } else {
        RenderFPSHUD();
    }

    // Overlay loading screen if visible
    if (m_LoadingScreen && m_LoadingScreen->IsVisible()) {
        m_LoadingScreen->Render(size.first, size.second, m_DeltaTime);
    }
}

void FPSGame::HandleInput() {
    if (!m_InputManager) return;

    GameStateType currentState = m_GameState ? m_GameState->GetCurrentState() : GameStateType::Playing;

    // Handle pause only when playing
    if (m_InputManager->IsKeyJustPressed(27)) { // ESC
        if (currentState == GameStateType::Playing) {
            if (m_GameState) m_GameState->SetState(GameStateType::Paused);
            if (m_PauseMenu) m_PauseMenu->Show();
        } else if (currentState == GameStateType::Paused) {
            if (m_GameState) m_GameState->SetState(GameStateType::Playing);
            if (m_PauseMenu) {
                m_PauseMenu->Hide();
                m_PauseMenu->SetSettingsOpen(false);
            }
        } else if (currentState == GameStateType::MainMenu) {
            // Main menu handles its own ESC for submenus, but we can have a fallback
        }
    }
}

void FPSGame::UpdateFPSMovement(float DeltaTime) {
    FPSMovementSystem::Update(m_Registry, DeltaTime, m_Camera);
}

void FPSGame::UpdateWeapons(float DeltaTime) {
    m_ECSScheduler.ExecutePhase(ECS::SystemPhase::Late, m_Registry, DeltaTime);
}

void FPSGame::RenderFPSHUD() {
    if (!m_HUD || !m_Window) return;

    // Don't render HUD if loading screen is visible
    if (m_LoadingScreen && m_LoadingScreen->IsVisible()) {
        return;
    }

    // Safety check: ensure ImGui is initialized
    if (!UI::UISystem::Instance().IsInitialized()) {
        return;
    }

    auto size = m_Window->GetFramebufferSize();
    m_HUD->Render(m_Registry, m_Camera, size.first, size.second);

    // Render HUD separately with pixel-perfect settings
    if (UI::UISystem::Instance().IsInitialized()) {
        UI::UISystem::Instance().RenderWithSettings(UI::UIRenderSettings::HUD());
    }
}

} // namespace Solstice::Game
