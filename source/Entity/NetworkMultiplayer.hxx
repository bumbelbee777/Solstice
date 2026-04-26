#pragma once

#include "EntityId.hxx"

#include <cstdint>
#include <string>
#include <vector>

namespace Solstice::ECS {

/// Opaque connection handle matching `NetworkingSystem` / `Packet` endpoints; 0 = unassigned.
struct NetworkPeerId {
    std::uint64_t Connection{0};
};

struct TeamMembership {
    std::uint8_t TeamId{0};
    /// 255 = no fixed slot (sentinel).
    std::uint8_t SlotInTeam{255};
};

enum class PartyRole : std::uint8_t {
    Member = 0,
    Leader = 1,
    Invitee = 2,
};

struct PartyMembership {
    std::uint32_t PartyId{0};
    PartyRole Role{PartyRole::Member};
};

/// Per-peer RTT samples (updated by game-layer ping echo).
struct NetPingStats {
    float LastRttMs{0.0f};
    float SmoothedRttMs{0.0f};
    float PingSendAccum{0.0f};
};

struct NetHeartbeatState {
    /// Seconds since any inbound packet for this peer (reset by receive path).
    float InboundSilenceSeconds{0.0f};
    float OutboundHeartbeatAccum{0.0f};
};

struct NetVoiceSignalling {
    bool IsTalking{false};
    bool Muted{false};
};

/// Rolling recent chat lines for tooling / HUD (cap enforced by handler).
struct NetChatInbox {
    std::vector<std::string> Lines;
    std::size_t MaxLines{32};
};

struct LobbySessionTag {};

enum class LobbyPhase : std::uint8_t {
    Gathering = 0,
    Countdown = 1,
    Starting = 2,
};

struct LobbySessionState {
    LobbyPhase Phase{LobbyPhase::Gathering};
    float CountdownRemaining{0.0f};
};

} // namespace Solstice::ECS
