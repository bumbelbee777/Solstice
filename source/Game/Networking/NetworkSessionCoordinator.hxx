#pragma once

#include "MultiplayerPresets.hxx"

#include "../../Entity/NetworkMultiplayer.hxx"
#include "../../Entity/Registry.hxx"
#include "../../Entity/Scheduler.hxx"
#include "../../Networking/NetworkingSystem.hxx"

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>

namespace Solstice::Game {

/// Bridges `NetworkingSystem` and ECS: receive dispatch, peer entities, and Simulation-phase systems
/// (heartbeat, ping echo, timeouts). Install at most **one** coordinator per process; it owns the
/// global `SetReceiveCallback` slot.
class SOLSTICE_API NetworkSessionCoordinator {
public:
    NetworkSessionCoordinator(ECS::Registry& registry, MultiplayerSessionConfig config);
    ~NetworkSessionCoordinator();

    NetworkSessionCoordinator(const NetworkSessionCoordinator&) = delete;
    NetworkSessionCoordinator& operator=(const NetworkSessionCoordinator&) = delete;

    void RegisterSystems(ECS::PhaseScheduler& scheduler);

    const MultiplayerSessionConfig& GetConfig() const { return m_Config; }
    void SetConfig(MultiplayerSessionConfig config) { m_Config = std::move(config); }

    ECS::EntityId GetLobbySessionEntity() const { return m_LobbyEntity; }

    using PeerEventCallback = std::function<void(ECS::EntityId, std::uint64_t connection)>;
    void SetOnPeerRemoved(PeerEventCallback cb) { m_OnPeerRemoved = std::move(cb); }

private:
    void InstallReceiveHook();
    void OnReceive(std::uint64_t connection, const Networking::Packet& packet);

    ECS::EntityId EnsurePeerEntity(std::uint64_t connection);
    void RemovePeer(std::uint64_t connection);
    void EnsureLobbySessionEntity();

    void DispatchInbound(std::uint64_t connection, const Networking::Packet& packet);
    void ResetInboundSilence(ECS::EntityId entity);

    void TickHeartbeat(ECS::Registry& registry, float deltaTime);
    void TickPing(ECS::Registry& registry, float deltaTime);
    void TickTimeout(ECS::Registry& registry, float deltaTime);
    void TickInboundSilence(ECS::Registry& registry, float deltaTime);

    static std::uint64_t SteadyMilliseconds();

    ECS::Registry& m_Registry;
    MultiplayerSessionConfig m_Config;

    std::unordered_map<std::uint64_t, ECS::EntityId> m_ConnectionToEntity;
    std::unordered_map<ECS::EntityId, std::uint64_t> m_EntityToConnection;

    ECS::EntityId m_LobbyEntity{0};
    bool m_ReceiveHookInstalled{false};

    PeerEventCallback m_OnPeerRemoved;
};

} // namespace Solstice::Game
