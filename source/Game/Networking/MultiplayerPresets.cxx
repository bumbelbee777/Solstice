#include "MultiplayerPresets.hxx"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <string_view>

namespace Solstice::Game {

MultiplayerSessionConfig MultiplayerPresets::GetLanParty() {
    MultiplayerSessionConfig c;
    c.Chat.MaxMessageLength = 1024;
    c.Chat.MaxScrollbackLines = 128;
    c.Party.MaxPartySize = 8;
    c.Party.PartyChatIsolated = false;
    c.Lobby.MaxPlayers = 32;
    c.Lobby.ReadyTimeoutSec = 600.0f;
    c.Timeouts.ConnectionTimeoutSec = 60.0f;
    c.Ping.PingStaleThresholdMs = 800.0f;
    return c;
}

MultiplayerSessionConfig MultiplayerPresets::GetCompetitive() {
    MultiplayerSessionConfig c;
    c.Chat.MaxMessageLength = 256;
    c.Chat.MaxScrollbackLines = 32;
    c.Party.MaxPartySize = 5;
    c.Party.PartyChatIsolated = true;
    c.Team.MaxTeams = 2;
    c.Team.MaxPlayersPerTeam = 6;
    c.Team.AllowTeamSwitchInLobby = false;
    c.Lobby.MinPlayersToStart = 2;
    c.Lobby.MaxPlayers = 12;
    c.Lobby.ReadyTimeoutSec = 120.0f;
    c.Lobby.LobbyCountdownSec = 3.0f;
    c.Timeouts.HeartbeatIntervalSec = 1.0f;
    c.Timeouts.ConnectionTimeoutSec = 15.0f;
    c.Ping.PingIntervalSec = 1.0f;
    c.Ping.PingStaleThresholdMs = 200.0f;
    c.Ping.SmoothAlpha = 0.25f;
    return c;
}

MultiplayerSessionConfig MultiplayerPresets::GetCooperative() {
    MultiplayerSessionConfig c;
    c.Chat.MaxMessageLength = 512;
    c.Party.MaxPartySize = 4;
    c.Party.PartyChatIsolated = true;
    c.Lobby.MaxPlayers = 8;
    c.Timeouts.ConnectionTimeoutSec = 45.0f;
    c.Ping.PingIntervalSec = 2.5f;
    c.Ping.PingStaleThresholdMs = 600.0f;
    return c;
}

namespace MultiplayerProtocol {

bool AppendMessageHeader(Networking::Packet& packet, MultiplayerMessageType type) {
    const auto t = static_cast<std::uint8_t>(type);
    packet.Data().push_back(t);
    return true;
}

bool BuildChatPacket(Networking::Packet& packet, std::string_view utf8, const ChatConfig& cfg) {
    packet.Data().clear();
    if (!AppendMessageHeader(packet, MultiplayerMessageType::ChatUtf8)) {
        return false;
    }
    const std::size_t n = std::min<std::size_t>(utf8.size(), cfg.MaxMessageLength);
    const auto len = static_cast<std::uint16_t>(n);
    packet.AppendPod(len);
    if (n > 0) {
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(utf8.data());
        packet.Data().insert(packet.Data().end(), bytes, bytes + n);
    }
    packet.SetChannel(cfg.Channel);
    packet.SetMode(cfg.Mode);
    return true;
}

bool TryParseChatPacket(const Networking::Packet& packet, std::string& outUtf8, const ChatConfig& cfg) {
    outUtf8.clear();
    const auto& d = packet.Data();
    if (d.size() < 1 + sizeof(std::uint16_t)) {
        return false;
    }
    if (d[0] != static_cast<std::uint8_t>(MultiplayerMessageType::ChatUtf8)) {
        return false;
    }
    std::uint16_t len = 0;
    if (!packet.ReadPod(1, len)) {
        return false;
    }
    if (len > cfg.MaxMessageLength) {
        return false;
    }
    if (1u + sizeof(std::uint16_t) + static_cast<std::size_t>(len) > d.size()) {
        return false;
    }
    outUtf8.assign(reinterpret_cast<const char*>(d.data() + 1 + sizeof(std::uint16_t)), len);
    return true;
}

bool BuildUsernamePacket(Networking::Packet& packet, std::string_view utf8, const UsernameConfig& cfg) {
    packet.Data().clear();
    if (!AppendMessageHeader(packet, MultiplayerMessageType::UsernameUtf8)) {
        return false;
    }
    const std::size_t n = std::min<std::size_t>(utf8.size(), cfg.MaxLength);
    const auto len = static_cast<std::uint8_t>(n);
    packet.Data().push_back(len);
    if (n > 0) {
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(utf8.data());
        packet.Data().insert(packet.Data().end(), bytes, bytes + n);
    }
    packet.SetChannel(MultiplayerChannels::Control);
    packet.SetMode(Networking::Packet::SendMode::Reliable);
    return true;
}

bool TryParseUsernamePacket(const Networking::Packet& packet, std::string& outUtf8, const UsernameConfig& cfg) {
    outUtf8.clear();
    const auto& d = packet.Data();
    if (d.size() < 2) {
        return false;
    }
    if (d[0] != static_cast<std::uint8_t>(MultiplayerMessageType::UsernameUtf8)) {
        return false;
    }
    const auto len = static_cast<std::size_t>(d[1]);
    if (len > cfg.MaxLength) {
        return false;
    }
    if (2 + len > d.size()) {
        return false;
    }
    outUtf8.assign(reinterpret_cast<const char*>(d.data() + 2), len);
    return true;
}

bool BuildVoiceSignallingPacket(Networking::Packet& packet, bool talking, bool muted, int channel) {
    packet.Data().clear();
    if (!AppendMessageHeader(packet, MultiplayerMessageType::VoiceSignalling)) {
        return false;
    }
    std::uint8_t flags = 0;
    if (talking) {
        flags |= 1u;
    }
    if (muted) {
        flags |= 2u;
    }
    packet.Data().push_back(flags);
    packet.SetChannel(channel);
    packet.SetMode(Networking::Packet::SendMode::Reliable);
    return true;
}

bool TryParseVoiceSignalling(const Networking::Packet& packet, bool& talking, bool& muted) {
    talking = false;
    muted = false;
    const auto& d = packet.Data();
    if (d.size() < 2) {
        return false;
    }
    if (d[0] != static_cast<std::uint8_t>(MultiplayerMessageType::VoiceSignalling)) {
        return false;
    }
    const std::uint8_t flags = d[1];
    talking = (flags & 1u) != 0;
    muted = (flags & 2u) != 0;
    return true;
}

bool BuildPingRequest(Networking::Packet& packet, std::uint64_t timestampMs) {
    packet.Data().clear();
    if (!AppendMessageHeader(packet, MultiplayerMessageType::PingRequest)) {
        return false;
    }
    packet.AppendPod(timestampMs);
    packet.SetChannel(MultiplayerChannels::Control);
    packet.SetMode(Networking::Packet::SendMode::Unreliable);
    packet.SetSendOptions(Networking::Packet::UnreliableFlag);
    return true;
}

bool BuildPingResponse(Networking::Packet& packet, std::uint64_t timestampMs) {
    packet.Data().clear();
    if (!AppendMessageHeader(packet, MultiplayerMessageType::PingResponse)) {
        return false;
    }
    packet.AppendPod(timestampMs);
    packet.SetChannel(MultiplayerChannels::Control);
    packet.SetMode(Networking::Packet::SendMode::Unreliable);
    packet.SetSendOptions(Networking::Packet::UnreliableFlag);
    return true;
}

bool TryParsePingTimestamp(const Networking::Packet& packet, std::uint64_t& outTs) {
    const auto& d = packet.Data();
    if (d.size() < 1 + sizeof(std::uint64_t)) {
        return false;
    }
    const auto t = static_cast<MultiplayerMessageType>(d[0]);
    if (t != MultiplayerMessageType::PingRequest && t != MultiplayerMessageType::PingResponse) {
        return false;
    }
    return packet.ReadPod(1, outTs);
}

bool BuildHeartbeatPacket(Networking::Packet& packet) {
    packet.Data().clear();
    if (!AppendMessageHeader(packet, MultiplayerMessageType::Heartbeat)) {
        return false;
    }
    packet.SetChannel(MultiplayerChannels::Control);
    packet.SetMode(Networking::Packet::SendMode::Unreliable);
    packet.SetSendOptions(Networking::Packet::UnreliableFlag);
    return true;
}

bool BuildLobbyStubPacket(Networking::Packet& packet, std::uint8_t phase, float countdown) {
    packet.Data().clear();
    if (!AppendMessageHeader(packet, MultiplayerMessageType::LobbyStub)) {
        return false;
    }
    packet.Data().push_back(phase);
    packet.AppendPod(countdown);
    packet.SetChannel(MultiplayerChannels::Control);
    packet.SetMode(Networking::Packet::SendMode::Reliable);
    return true;
}

bool TryParseLobbyStub(const Networking::Packet& packet, std::uint8_t& phase, float& countdown) {
    const auto& d = packet.Data();
    if (d.size() < 1 + 1 + sizeof(float)) {
        return false;
    }
    if (d[0] != static_cast<std::uint8_t>(MultiplayerMessageType::LobbyStub)) {
        return false;
    }
    phase = d[1];
    std::memcpy(&countdown, d.data() + 2, sizeof(float));
    return true;
}

} // namespace MultiplayerProtocol

} // namespace Solstice::Game
