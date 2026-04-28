#pragma once

#include "WohShipTypes.hxx"
#include "WohSpaceEnvironment.hxx"
#include "WohCombatScoring.hxx"

#include <Game/App/GameBase.hxx>
#include <Game/Match/ApplySmfGameplay.hxx>
#include <Game/Match/Leaderboard.hxx>
#include <Game/Match/TeamLeaderboard.hxx>
#include <Game/Match/MatchScoreState.hxx>
#include <Game/Match/MatchGameplayAuthoringCache.hxx>
#include <Game/Progression/AchievementState.hxx>
#include <Game/Progression/ScoreMultiplierState.hxx>
#include <Entity/Scheduler.hxx>
#include <Game/Integration/ScriptManager.hxx>
#include <UI/Core/Window.hxx>
#include <UI/Core/UISystem.hxx>
#include "WohNetworkSession.hxx"
#include <Render/SoftwareRenderer.hxx>
#include <Render/Scene/Scene.hxx>
#include <Render/Scene/Camera.hxx>
#include <Physics/Lighting/LightSource.hxx>
#include <Entity/Registry.hxx>
#include <memory>
#include <string>
#include <vector>

namespace Solstice::WarsOfHeaven {

class WarsOfHeavenSmoke : public Game::GameBase {
public:
    WarsOfHeavenSmoke();
    ~WarsOfHeavenSmoke() override;

protected:
    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;
    void Render() override;
    void HandleInput() override;

private:
    void InitWindow();
    void ApplyStubGameplay();
    void PollShipInput(float deltaTime);
    void SampleArzachelFragmentation();

    std::unique_ptr<Render::SoftwareRenderer> m_Renderer;
    Render::Scene m_Scene;
    std::unique_ptr<Render::MeshLibrary> m_MeshLibrary;
    std::unique_ptr<Core::MaterialLibrary> m_MaterialLibrary;
    Render::Camera m_Camera;
    std::vector<Physics::LightSource> m_Lights;
    ECS::Registry m_Registry;
    ECS::PhaseScheduler m_Scheduler;
    WohNetworkSession m_Net{};

    Game::MatchScoreState m_Match;
    Game::Leaderboard m_Leaderboard;
    Game::TeamLeaderboard m_TeamBoard;
    Game::AchievementState m_Achievements;
    Game::ScoreMultiplierState m_ScoreMult;
    Game::ScriptManager m_Scripts;

    ECS::EntityId m_PlayerShip{0};
    std::vector<ECS::EntityId> m_EnemyShips;
    WohCombatScoringState m_CombatState{};
    double m_GameTime{0.0};
    /// When true, full-screen post blur + Moonwalk main menu; ship sim and most combat ticks are frozen.
    bool m_MainMenuOpen{true};

    /// Built from `MatchGameplayAuthoringCache` celestials + game-side asteroid field.
    std::vector<PlanetaryGravitySource> m_GravityBodies;
    std::vector<AsteroidBeltRegion> m_AsteroidBelts;
    /// Fake "enemy sensor" for occlusion debug in ImGui
    Math::Vec3 m_EnemyObserverPos{2000.0f, 0.0f, 0.0f};

    /// Arzachel `Damaged` mesh: vertex count (fragmentation smokescreen).
    std::size_t m_FragmentationMeshVertexCount{0};

    bool m_ScriptRan{false};
    bool m_PrevHDown{false};
    bool m_PrevMDown{false};
    bool m_PrevFDown{false};
    float m_LastFrameDelta{0.0f};
};

} // namespace Solstice::WarsOfHeaven
