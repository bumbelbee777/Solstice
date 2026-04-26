#pragma once

#include "../../Solstice.hxx"
#include "../../Networking/Packet.hxx"

#include <cstdint>
#include <imgui.h>
#include <string>
#include <string_view>

namespace Solstice::Game {

/// Logical channels for `Packet::SetChannel` (GameNetworkingSockets n_channels default is sufficient for these indices).
namespace MultiplayerChannels {
inline constexpr int Control = 0;
inline constexpr int Chat = 1;
inline constexpr int GameState = 2;
inline constexpr int VoiceSignalling = 3;
} // namespace MultiplayerChannels

enum class MultiplayerMessageType : std::uint8_t {
    Heartbeat = 1,
    PingRequest = 2,
    PingResponse = 3,
    ChatUtf8 = 10,
    UsernameUtf8 = 11,
    VoiceSignalling = 12,
    LobbyStub = 32,
};

struct ChatConfig {
    std::uint32_t MaxMessageLength{512};
    std::uint32_t MaxScrollbackLines{64};
    int Channel{MultiplayerChannels::Chat};
    Networking::Packet::SendMode Mode{Networking::Packet::SendMode::Reliable};
    float BackgroundAlpha{0.75f};
    ImVec4 BackgroundColor{0.05f, 0.05f, 0.08f, 1.0f};
    ImVec4 TextColor{1.0f, 1.0f, 1.0f, 1.0f};
};

struct UsernameConfig {
    std::uint32_t MaxLength{32};
    bool ShowOverhead{true};
    bool ShowScoreboard{true};
    ImVec4 AccentColor{0.9f, 0.85f, 0.7f, 1.0f};
};

struct VoiceUIConfig {
    int SignallingChannel{MultiplayerChannels::VoiceSignalling};
    bool DefaultMuted{false};
    bool ShowPushToTalkHint{true};
};

struct PartyConfig {
    std::uint32_t MaxPartySize{4};
    float InviteTimeoutSec{120.0f};
    bool PartyChatIsolated{true};
};

struct TeamConfig {
    std::uint32_t MaxTeams{16};
    std::uint32_t MaxPlayersPerTeam{8};
    bool AllowTeamSwitchInLobby{true};
};

struct LobbyConfig {
    std::uint32_t MinPlayersToStart{1};
    std::uint32_t MaxPlayers{16};
    float ReadyTimeoutSec{300.0f};
    float LobbyCountdownSec{5.0f};
    float MapVoteTimeoutSec{60.0f};
};

struct TimeoutConfig {
    float HeartbeatIntervalSec{2.0f};
    float ConnectionTimeoutSec{30.0f};
    float HandshakeTimeoutSec{15.0f};
};

struct PingConfig {
    float PingIntervalSec{2.0f};
    float PingStaleThresholdMs{500.0f};
    float SmoothAlpha{0.15f};
};

struct MultiplayerSessionConfig {
    ChatConfig Chat{};
    UsernameConfig Username{};
    VoiceUIConfig Voice{};
    PartyConfig Party{};
    TeamConfig Team{};
    LobbyConfig Lobby{};
    TimeoutConfig Timeouts{};
    PingConfig Ping{};
};

class SOLSTICE_API MultiplayerPresets {
public:
    static MultiplayerSessionConfig GetLanParty();
    static MultiplayerSessionConfig GetCompetitive();
    static MultiplayerSessionConfig GetCooperative();
};

namespace MultiplayerProtocol {

SOLSTICE_API bool AppendMessageHeader(Networking::Packet& packet, MultiplayerMessageType type);

/// Chat: [type][uint16_t len][utf8 bytes]
SOLSTICE_API bool BuildChatPacket(Networking::Packet& packet, std::string_view utf8, const ChatConfig& cfg);
SOLSTICE_API bool TryParseChatPacket(const Networking::Packet& packet, std::string& outUtf8, const ChatConfig& cfg);

/// Username: [type][uint8_t len][utf8 bytes]
SOLSTICE_API bool BuildUsernamePacket(Networking::Packet& packet, std::string_view utf8, const UsernameConfig& cfg);
SOLSTICE_API bool TryParseUsernamePacket(const Networking::Packet& packet, std::string& outUtf8, const UsernameConfig& cfg);

/// Voice signalling: [type][uint8_t flags] bit0=talking bit1=muted
SOLSTICE_API bool BuildVoiceSignallingPacket(Networking::Packet& packet, bool talking, bool muted, int channel);
SOLSTICE_API bool TryParseVoiceSignalling(const Networking::Packet& packet, bool& talking, bool& muted);

/// Control ping: [type][uint64_t timestampMs]
SOLSTICE_API bool BuildPingRequest(Networking::Packet& packet, std::uint64_t timestampMs);
SOLSTICE_API bool BuildPingResponse(Networking::Packet& packet, std::uint64_t timestampMs);
SOLSTICE_API bool TryParsePingTimestamp(const Networking::Packet& packet, std::uint64_t& outTs);

/// Heartbeat: [type] only
SOLSTICE_API bool BuildHeartbeatPacket(Networking::Packet& packet);

/// Lobby stub: [type][uint8_t phase][float countdown] (packed as pod)
SOLSTICE_API bool BuildLobbyStubPacket(Networking::Packet& packet, std::uint8_t phase, float countdown);
SOLSTICE_API bool TryParseLobbyStub(const Networking::Packet& packet, std::uint8_t& phase, float& countdown);

} // namespace MultiplayerProtocol

} // namespace Solstice::Game
