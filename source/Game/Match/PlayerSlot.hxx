#pragma once

#include "../../Entity/EntityId.hxx"

#include <cstdint>
#include <string>

namespace Solstice::Game {

/// Logical player slot for local and networked sessions. `MatchPlayerId` matches `MatchScoreState` keys.
/// `TeamId == 0` = FFA; non-zero = team-aggregated scoring via `MatchScoreState::GetTeamScore`.
struct PlayerSlot {
    std::uint32_t MatchPlayerId{0};
    /// Alliance / fleet side for team modes (`ECS::TeamId` alias).
    std::uint8_t TeamId{0};
    std::uint32_t SlotIndex{0};
    ECS::EntityId ControlledEntity{0};
    bool IsLocal{true};
    std::string DisplayName;

    bool IsValid() const { return MatchPlayerId != 0; }
};

/// Stable id for free-for-all scoring and leaderboards (distinct from `ECS::EntityId` ship entity).
using LocalPlayerId = std::uint32_t;

} // namespace Solstice::Game
