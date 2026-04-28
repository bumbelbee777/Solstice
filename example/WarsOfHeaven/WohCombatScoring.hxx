#pragma once

#include "WohSpaceEnvironment.hxx"
#include "WohShipTypes.hxx"

#include <Entity/EntityId.hxx>
#include <Entity/Registry.hxx>
#include <Entity/Transform.hxx>
#include <Game/Match/MatchScoreState.hxx>
#include <Game/Progression/AchievementState.hxx>
#include <Game/Progression/ScoreMultiplierState.hxx>
#include <Entity/MatchGameplay.hxx>

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Solstice::WarsOfHeaven {

struct WohCombatScoringState {
    std::unordered_map<ECS::EntityId, float> LastHullByEntity;
    int KillStreak{0};
    double LastKillGameTime{-1.0e9};
    float SessionMultTimeLeft{0.0f};
    bool JuggernautUnlocked{false};
    int InitialEnemyCount{0};
    bool FirstSnapshot{true};
};

/// Gravity wells / planets: crush ships that get too close (self-damage, scores as environmental).
void ApplyCelestialLethalHazards(ECS::Registry& reg, const std::vector<ECS::EntityId>& ships, float deltaTime,
                                 const std::vector<PlanetaryGravitySource>& bodies);

/// FFA-style scoring: hull loss costs points, deaths cost more, enemy kills / streaks / team wipe. Credits local player `localMatchPlayerId`.
void UpdateMatchCombatScoring(ECS::Registry& reg, WohCombatScoringState& st, float deltaTime, double gameTime,
                              const std::vector<ECS::EntityId>& enemyShips, ECS::EntityId playerShip, std::uint32_t localMatchPlayerId,
                              std::uint8_t playerTeam, std::uint8_t enemyTeam, Game::MatchScoreState& match,
                              Game::AchievementState& ach, Game::ScoreMultiplierState& sm);

} // namespace Solstice::WarsOfHeaven
