#pragma once

#include "../App/GameBase.hxx"
#include "FPSMovement.hxx"
#include "Weapon.hxx"
#include "../UI/HUD.hxx"
#include "../App/InputManager.hxx"
#include "../App/GameState.hxx"
#include "../UI/LoadingScreen.hxx"
#include "../UI/MainMenu.hxx"
#include "../UI/PauseMenu.hxx"
#include "../../Entity/Registry.hxx"
#include "../../Entity/Scheduler.hxx"
#include <Render/Scene/Camera.hxx>
#include <memory>

namespace Solstice::Game {

// FPS game base class
class SOLSTICE_API FPSGame : public GameBase {
public:
    FPSGame();
    virtual ~FPSGame() = default;

protected:
    // Override these in derived classes
    void Initialize() override;
    void Update(float DeltaTime) override;
    void Render() override;
    void HandleInput() override;

    // FPS-specific initialization
    virtual void InitializeFPSSystems();
    virtual void InitializePlayer();
    virtual void InitializeWeapons();
    virtual void ConfigureECSPhases();

    // Systems
    std::unique_ptr<InputManager> m_InputManager;
    std::unique_ptr<GameState> m_GameState;
    std::unique_ptr<HUD> m_HUD;
    std::unique_ptr<LoadingScreen> m_LoadingScreen;
    std::unique_ptr<MainMenu> m_MainMenu;
    std::unique_ptr<PauseMenu> m_PauseMenu;

    // ECS
    ECS::Registry m_Registry;
    ECS::PhaseScheduler m_ECSScheduler;
    ECS::EntityId m_PlayerEntity{0};

    // Camera
    Render::Camera m_Camera;

    // FPS-specific settings
    bool m_ShowCrosshair{true};
    bool m_ShowWeaponHUD{true};
    bool m_ShowMinimap{false};
    bool m_ShowKillFeed{false};

protected:
    // Override these in derived classes if needed
    virtual void UpdateFPSMovement(float DeltaTime);
    virtual void UpdateWeapons(float DeltaTime);
    void RenderFPSHUD();
};

} // namespace Solstice::Game
