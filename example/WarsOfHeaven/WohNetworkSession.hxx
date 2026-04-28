#pragma once

#include <Entity/Registry.hxx>
#include <Entity/Scheduler.hxx>
#include <Game/Networking/MultiplayerPresets.hxx>
#include <Game/Networking/NetworkSessionCoordinator.hxx>
#include <cstdint>
#include <memory>
#include <string>

namespace Solstice::WarsOfHeaven {

/// Minimal LAN session helper: `NetworkSessionCoordinator` + listen / client connect. Uses
/// `MultiplayerPresets::GetWarsOfHeaven()` unless you pass a custom config to `EnsureCoordinator`.
class WohNetworkSession {
public:
    void EnsureCoordinator(ECS::Registry& registry, ECS::PhaseScheduler& scheduler);
    void EnsureCoordinator(ECS::Registry& registry, ECS::PhaseScheduler& scheduler, Game::MultiplayerSessionConfig config);

    void Shutdown();

    bool IsCoordinatorReady() const { return m_Coordinator != nullptr; }

    /// Listen on all IPv4 interfaces. Idempotent with respect to an existing listen socket.
    bool Host(uint16_t port);
    /// Outbound connect (closes prior client connection if any).
    bool Connect(const char* host, uint16_t port);

    void Disconnect();

    bool IsHosting() const { return m_ListenSocket != 0; }
    bool IsClientConnected() const { return m_ClientConnection != 0; }

    std::string BuildStatusLine() const;

    const std::string& GetLastError() const { return m_LastError; }

private:
    void EnsureCoordinatorImpl(Game::MultiplayerSessionConfig config);

    ECS::Registry* m_Registry{nullptr};
    ECS::PhaseScheduler* m_Scheduler{nullptr};

    std::unique_ptr<Game::NetworkSessionCoordinator> m_Coordinator;
    Game::MultiplayerSessionConfig m_Config{};

    std::uint64_t m_ListenSocket{0};
    std::uint64_t m_ClientConnection{0};

    std::string m_LastError;
};

} // namespace Solstice::WarsOfHeaven
