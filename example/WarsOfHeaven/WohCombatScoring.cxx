#include "WohCombatScoring.hxx"

#include <Entity/MatchGameplay.hxx>
#include <Entity/Transform.hxx>
#include <cmath>
#include <algorithm>

namespace Solstice::WarsOfHeaven {
namespace {

constexpr double kMultikillWindowSec = 4.0;
constexpr int64_t kPerFiveHullLost = 1;
constexpr int64_t kDeathPenalty = 350;
constexpr int64_t kBonusDouble = 200;
constexpr int64_t kBonusTriple = 450;
constexpr int64_t kBonusMultikill = 800;
constexpr int64_t kBonusJuggernaut = 5000;
constexpr float kSessionDoubleTime = 7.0f;
constexpr float kSessionTripleTime = 8.0f;
constexpr float kSessionMultiTime = 10.0f;
constexpr float kSessionJuggernautTime = 18.0f;

inline int64_t PlayerHullDamageToScoreDelta(float amount) {
    if (amount <= 0.0f) {
        return 0;
    }
    return static_cast<int64_t>(std::llround((amount / 5.0f) * static_cast<float>(-kPerFiveHullLost)));
}

} // namespace

void ApplyCelestialLethalHazards(ECS::Registry& reg, const std::vector<ECS::EntityId>& ships, float deltaTime,
                                 const std::vector<PlanetaryGravitySource>& bodies) {
    const float dt = (deltaTime > 0.0f) ? deltaTime : 0.016f;
    for (ECS::EntityId e : ships) {
        if (e == 0 || !reg.Has<ECS::Transform>(e) || !reg.Has<ShipHullHealth>(e)) {
            continue;
        }
        const auto& t = reg.Get<ECS::Transform>(e);
        auto& h = reg.Get<ShipHullHealth>(e);
        if (h.Current <= 0.0f) {
            continue;
        }
        for (const auto& b : bodies) {
            const Math::Vec3 d = t.Position - b.Center;
            const float dist = d.Length();
            if (dist < b.Radius * 0.58f) {
                h.ApplyDamage(4000.0f * dt + 80.0f);
            } else if (dist < b.Radius * 1.15f) {
                h.ApplyDamage(12.0f * dt);
            }
        }
    }
}

void UpdateMatchCombatScoring(ECS::Registry& reg, WohCombatScoringState& st, float deltaTime, double gameTime,
                              const std::vector<ECS::EntityId>& enemyShips, ECS::EntityId playerShip, std::uint32_t localMatchPlayerId,
                              std::uint8_t /*playerTeam*/, std::uint8_t /*enemyTeam*/, Game::MatchScoreState& match,
                              Game::AchievementState& ach, Game::ScoreMultiplierState& sm) {
    if (st.FirstSnapshot) {
        st.InitialEnemyCount = static_cast<int>(enemyShips.size());
        st.FirstSnapshot = false;
    }

    if (st.SessionMultTimeLeft > 0.0f) {
        st.SessionMultTimeLeft -= std::max(0.0f, deltaTime);
        if (st.SessionMultTimeLeft <= 0.0f) {
            sm.SetSession(1.0f);
        }
    }

    if (playerShip != 0 && reg.Has<ECS::MatchPlayerIdComponent>(playerShip) && reg.Has<ShipHullHealth>(playerShip)) {
        auto& h = reg.Get<ShipHullHealth>(playerShip);
        if (st.LastHullByEntity.find(playerShip) == st.LastHullByEntity.end()) {
            st.LastHullByEntity[playerShip] = h.Current;
        } else {
            const float prev = st.LastHullByEntity[playerShip];
            st.LastHullByEntity[playerShip] = h.Current;
            const float lost = prev - h.Current;
            if (lost > 0.01f) {
                const int64_t d = PlayerHullDamageToScoreDelta(lost);
                if (d != 0) {
                    const std::uint32_t pid = reg.Get<ECS::MatchPlayerIdComponent>(playerShip).Id;
                    match.AddScore(pid, d);
                }
            }
            if (prev > 0.5f && h.Current <= 0.5f) {
                const std::uint32_t pid = reg.Get<ECS::MatchPlayerIdComponent>(playerShip).Id;
                match.AddScore(pid, -kDeathPenalty);
            }
        }
    }

    for (ECS::EntityId eid : enemyShips) {
        if (eid == 0 || !reg.Has<ShipHullHealth>(eid)) {
            continue;
        }
        auto& h = reg.Get<ShipHullHealth>(eid);
        if (st.LastHullByEntity.find(eid) == st.LastHullByEntity.end()) {
            st.LastHullByEntity[eid] = h.Current;
            continue;
        }
        const float prev = st.LastHullByEntity[eid];
        st.LastHullByEntity[eid] = h.Current;
        if (prev > 0.5f && h.Current <= 0.5f) {
            if (gameTime - st.LastKillGameTime < kMultikillWindowSec) {
                st.KillStreak += 1;
            } else {
                st.KillStreak = 1;
            }
            st.LastKillGameTime = gameTime;

            if (st.KillStreak == 2) {
                match.AddScore(localMatchPlayerId, kBonusDouble);
                ach.Unlock("woh.kill.double");
                sm.SetSession(1.4f);
                st.SessionMultTimeLeft = kSessionDoubleTime;
            } else if (st.KillStreak == 3) {
                match.AddScore(localMatchPlayerId, kBonusTriple);
                ach.Unlock("woh.kill.triple");
                sm.SetSession(1.6f);
                st.SessionMultTimeLeft = kSessionTripleTime;
            } else if (st.KillStreak >= 4) {
                match.AddScore(localMatchPlayerId, kBonusMultikill);
                ach.Unlock("woh.kill.multikill");
                sm.SetSession(2.0f);
                st.SessionMultTimeLeft = kSessionMultiTime;
            } else {
                match.AddScore(localMatchPlayerId, 100);
            }
        }
    }

    int livingEnemies = 0;
    for (ECS::EntityId eid : enemyShips) {
        if (eid == 0 || !reg.Has<ShipHullHealth>(eid)) {
            continue;
        }
        if (reg.Get<ShipHullHealth>(eid).Current > 0.5f) {
            livingEnemies++;
        }
    }
    if (!st.JuggernautUnlocked && st.InitialEnemyCount > 0 && livingEnemies == 0) {
        st.JuggernautUnlocked = true;
        match.AddScore(localMatchPlayerId, kBonusJuggernaut);
        ach.Unlock("woh.kill.juggernaut");
        sm.SetSession(2.5f);
        st.SessionMultTimeLeft = kSessionJuggernautTime;
    }
}

} // namespace Solstice::WarsOfHeaven
