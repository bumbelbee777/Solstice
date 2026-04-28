#pragma once

#include "EntityId.hxx"

#include <cstdint>
#include <string>

namespace Solstice::ECS {

/// Team for match scoring and capture logic (`0` = FFA / unassigned, `1`… = real teams).
using TeamId = std::uint8_t;

/// Links a ship/avatar to `Game::MatchScoreState` and optional `TeamId`.
struct MatchPlayerIdComponent {
    std::uint32_t Id{0};
    TeamId Team{0};
};

/// Reference to a capture volume id (string id from level/match authoring).
struct CaptureZoneRef {
    std::string ZoneId;
};

/// Scripted / authored warp drive charge (trajectory in game sim).
struct WarpDriveState {
    float Charge01{0.0f};
    float CooldownRemainingSec{0.0f};
    /// Authoring cap (e.g. 10.0 for 10c); 0 = use default.
    float MaxCFraction{0.0f};
};

} // namespace Solstice::ECS
