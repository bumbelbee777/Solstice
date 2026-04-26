#include "NetworkSessionCoordinator.hxx"

#include "../../Entity/Name.hxx"

#include <chrono>
#include <cmath>
#include <cstring>

namespace Solstice::Game {

NetworkSessionCoordinator::NetworkSessionCoordinator(ECS::Registry& registry, MultiplayerSessionConfig config)
    : m_Registry(registry), m_Config(std::move(config)) {
    InstallReceiveHook();
}

NetworkSessionCoordinator::~NetworkSessionCoordinator() {
    if (m_ReceiveHookInstalled) {
        Networking::NetworkingSystem::Instance().ClearReceiveCallback();
        m_ReceiveHookInstalled = false;
    }
}

void NetworkSessionCoordinator::InstallReceiveHook() {
    if (m_ReceiveHookInstalled) {
        return;
    }
    Networking::NetworkingSystem::Instance().SetReceiveCallback(
        [this](std::uint64_t connection, const Networking::Packet& packet) { OnReceive(connection, packet); });
    m_ReceiveHookInstalled = true;
}

std::uint64_t NetworkSessionCoordinator::SteadyMilliseconds() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

void NetworkSessionCoordinator::OnReceive(std::uint64_t connection, const Networking::Packet& packet) {
    if (connection == 0) {
        return;
    }
    const ECS::EntityId entity = EnsurePeerEntity(connection);
    ResetInboundSilence(entity);
    DispatchInbound(connection, packet);
}

void NetworkSessionCoordinator::ResetInboundSilence(ECS::EntityId entity) {
    if (!m_Registry.Valid(entity)) {
        return;
    }
    if (ECS::NetHeartbeatState* hb = m_Registry.TryGet<ECS::NetHeartbeatState>(entity)) {
        hb->InboundSilenceSeconds = 0.0f;
    }
}

ECS::EntityId NetworkSessionCoordinator::EnsurePeerEntity(std::uint64_t connection) {
    const auto it = m_ConnectionToEntity.find(connection);
    if (it != m_ConnectionToEntity.end()) {
        return it->second;
    }

    const ECS::EntityId e = m_Registry.Create();
    m_Registry.Add<ECS::NetworkPeerId>(e, ECS::NetworkPeerId{connection});
    m_Registry.Add<ECS::TeamMembership>(e, ECS::TeamMembership{});
    m_Registry.Add<ECS::PartyMembership>(e, ECS::PartyMembership{});
    m_Registry.Add<ECS::NetPingStats>(e, ECS::NetPingStats{});
    m_Registry.Add<ECS::NetHeartbeatState>(e, ECS::NetHeartbeatState{});
    m_Registry.Add<ECS::NetVoiceSignalling>(e, ECS::NetVoiceSignalling{});
    ECS::NetChatInbox inbox;
    inbox.MaxLines = m_Config.Chat.MaxScrollbackLines;
    m_Registry.Add<ECS::NetChatInbox>(e, std::move(inbox));
    m_Registry.Add<ECS::Name>(e, ECS::Name{"Peer"});

    m_ConnectionToEntity[connection] = e;
    m_EntityToConnection[e] = connection;
    return e;
}

void NetworkSessionCoordinator::RemovePeer(std::uint64_t connection) {
    const auto it = m_ConnectionToEntity.find(connection);
    if (it == m_ConnectionToEntity.end()) {
        return;
    }
    const ECS::EntityId entity = it->second;
    m_EntityToConnection.erase(entity);
    m_ConnectionToEntity.erase(it);

    if (m_OnPeerRemoved) {
        m_OnPeerRemoved(entity, connection);
    }

    Networking::NetworkingSystem::Instance().CloseConnection(connection);
    if (m_Registry.Valid(entity)) {
        m_Registry.Destroy(entity);
    }
}

void NetworkSessionCoordinator::EnsureLobbySessionEntity() {
    if (m_LobbyEntity != 0 && m_Registry.Valid(m_LobbyEntity)) {
        return;
    }
    m_LobbyEntity = m_Registry.Create();
    m_Registry.Add<ECS::LobbySessionTag>(m_LobbyEntity);
    m_Registry.Add<ECS::LobbySessionState>(m_LobbyEntity, ECS::LobbySessionState{});
}

void NetworkSessionCoordinator::DispatchInbound(std::uint64_t connection, const Networking::Packet& packet) {
    const auto& d = packet.Data();
    if (d.empty()) {
        return;
    }

    const auto msg = static_cast<MultiplayerMessageType>(d[0]);

    if (msg == MultiplayerMessageType::Heartbeat) {
        return;
    }

    if (msg == MultiplayerMessageType::PingRequest) {
        std::uint64_t ts = 0;
        if (packet.ReadPod(1, ts)) {
            Networking::Packet out;
            MultiplayerProtocol::BuildPingResponse(out, ts);
            Networking::NetworkingSystem::Instance().Send(connection, out);
            Networking::NetworkingSystem::Instance().Flush(connection);
        }
        return;
    }

    if (msg == MultiplayerMessageType::PingResponse) {
        std::uint64_t ts = 0;
        if (!packet.ReadPod(1, ts)) {
            return;
        }
        const auto it = m_ConnectionToEntity.find(connection);
        if (it == m_ConnectionToEntity.end()) {
            return;
        }
        ECS::NetPingStats* stats = m_Registry.TryGet<ECS::NetPingStats>(it->second);
        if (!stats) {
            return;
        }
        const std::uint64_t now = SteadyMilliseconds();
        const float rtt = static_cast<float>(now > ts ? (now - ts) : 0);
        stats->LastRttMs = rtt;
        if (stats->SmoothedRttMs <= 0.0f) {
            stats->SmoothedRttMs = rtt;
        } else {
            const float a = m_Config.Ping.SmoothAlpha;
            stats->SmoothedRttMs = stats->SmoothedRttMs * (1.0f - a) + rtt * a;
        }
        return;
    }

    if (msg == MultiplayerMessageType::ChatUtf8) {
        const auto it = m_ConnectionToEntity.find(connection);
        if (it == m_ConnectionToEntity.end()) {
            return;
        }
        std::string text;
        if (MultiplayerProtocol::TryParseChatPacket(packet, text, m_Config.Chat)) {
            ECS::NetChatInbox* inbox = m_Registry.TryGet<ECS::NetChatInbox>(it->second);
            if (inbox) {
                while (inbox->Lines.size() >= inbox->MaxLines && inbox->MaxLines > 0) {
                    inbox->Lines.erase(inbox->Lines.begin());
                }
                inbox->Lines.push_back(std::move(text));
            }
        }
        return;
    }

    if (msg == MultiplayerMessageType::UsernameUtf8) {
        const auto it = m_ConnectionToEntity.find(connection);
        if (it == m_ConnectionToEntity.end()) {
            return;
        }
        std::string name;
        if (MultiplayerProtocol::TryParseUsernamePacket(packet, name, m_Config.Username)) {
            if (ECS::Name* n = m_Registry.TryGet<ECS::Name>(it->second)) {
                n->Value = std::move(name);
            }
        }
        return;
    }

    if (msg == MultiplayerMessageType::VoiceSignalling) {
        const auto it = m_ConnectionToEntity.find(connection);
        if (it == m_ConnectionToEntity.end()) {
            return;
        }
        bool talk = false;
        bool muted = false;
        if (MultiplayerProtocol::TryParseVoiceSignalling(packet, talk, muted)) {
            if (ECS::NetVoiceSignalling* v = m_Registry.TryGet<ECS::NetVoiceSignalling>(it->second)) {
                v->IsTalking = talk;
                v->Muted = muted;
            }
        }
        return;
    }

    if (msg == MultiplayerMessageType::LobbyStub) {
        std::uint8_t phase = 0;
        float countdown = 0.0f;
        if (!MultiplayerProtocol::TryParseLobbyStub(packet, phase, countdown)) {
            return;
        }
        if (m_LobbyEntity != 0 && m_Registry.Valid(m_LobbyEntity)) {
            if (ECS::LobbySessionState* ls = m_Registry.TryGet<ECS::LobbySessionState>(m_LobbyEntity)) {
                ls->Phase = static_cast<ECS::LobbyPhase>(phase);
                ls->CountdownRemaining = countdown;
            }
        }
    }
}

void NetworkSessionCoordinator::TickInboundSilence(ECS::Registry& registry, float deltaTime) {
    registry.ForEach<ECS::NetHeartbeatState>([&](ECS::EntityId e, ECS::NetHeartbeatState& hb) {
        if (!registry.Has<ECS::NetworkPeerId>(e)) {
            return;
        }
        hb.InboundSilenceSeconds += deltaTime;
    });
}

void NetworkSessionCoordinator::TickHeartbeat(ECS::Registry& registry, float deltaTime) {
    registry.ForEach<ECS::NetworkPeerId>([&](ECS::EntityId e, ECS::NetworkPeerId& peer) {
        if (peer.Connection == 0) {
            return;
        }
        ECS::NetHeartbeatState* hb = registry.TryGet<ECS::NetHeartbeatState>(e);
        if (!hb) {
            return;
        }
        hb->OutboundHeartbeatAccum += deltaTime;
        if (hb->OutboundHeartbeatAccum < m_Config.Timeouts.HeartbeatIntervalSec) {
            return;
        }
        hb->OutboundHeartbeatAccum = 0.0f;

        Networking::Packet pkt;
        MultiplayerProtocol::BuildHeartbeatPacket(pkt);
        Networking::NetworkingSystem::Instance().Send(peer.Connection, pkt);
        Networking::NetworkingSystem::Instance().Flush(peer.Connection);
    });
}

void NetworkSessionCoordinator::TickPing(ECS::Registry& registry, float deltaTime) {
    registry.ForEach<ECS::NetworkPeerId>([&](ECS::EntityId e, ECS::NetworkPeerId& peer) {
        if (peer.Connection == 0) {
            return;
        }
        ECS::NetPingStats* stats = registry.TryGet<ECS::NetPingStats>(e);
        if (!stats) {
            return;
        }
        stats->PingSendAccum += deltaTime;
        if (stats->PingSendAccum < m_Config.Ping.PingIntervalSec) {
            return;
        }
        stats->PingSendAccum = 0.0f;

        Networking::Packet pkt;
        MultiplayerProtocol::BuildPingRequest(pkt, SteadyMilliseconds());
        Networking::NetworkingSystem::Instance().Send(peer.Connection, pkt);
        Networking::NetworkingSystem::Instance().Flush(peer.Connection);
    });
}

void NetworkSessionCoordinator::TickTimeout(ECS::Registry& registry, float deltaTime) {
    (void)deltaTime;
    std::vector<std::uint64_t> toDrop;
    registry.ForEach<ECS::NetHeartbeatState>([&](ECS::EntityId e, ECS::NetHeartbeatState& hb) {
        const ECS::NetworkPeerId* pid = registry.TryGet<ECS::NetworkPeerId>(e);
        if (!pid || pid->Connection == 0) {
            return;
        }
        if (hb.InboundSilenceSeconds > m_Config.Timeouts.ConnectionTimeoutSec) {
            toDrop.push_back(pid->Connection);
        }
    });
    for (std::uint64_t c : toDrop) {
        RemovePeer(c);
    }
}

void NetworkSessionCoordinator::RegisterSystems(ECS::PhaseScheduler& scheduler) {
    EnsureLobbySessionEntity();

    scheduler.Register(ECS::SystemPhase::Simulation, "NetInboundSilence", [this](ECS::Registry& registry, float dt) {
        TickInboundSilence(registry, dt);
    });
    scheduler.Register(ECS::SystemPhase::Simulation, "NetHeartbeat", [this](ECS::Registry& registry, float dt) {
        TickHeartbeat(registry, dt);
    });
    scheduler.Register(ECS::SystemPhase::Simulation, "NetPing", [this](ECS::Registry& registry, float dt) {
        TickPing(registry, dt);
    });
    scheduler.Register(ECS::SystemPhase::Simulation, "NetTimeout", [this](ECS::Registry& registry, float dt) {
        TickTimeout(registry, dt);
    });
}

} // namespace Solstice::Game
