#include "WohNetworkSession.hxx"

#include <cstdio>
#include <sstream>

namespace Solstice::WarsOfHeaven {

void WohNetworkSession::EnsureCoordinator(ECS::Registry& registry, ECS::PhaseScheduler& scheduler) {
    Game::MultiplayerSessionConfig c = Game::MultiplayerPresets::GetWarsOfHeaven();
    EnsureCoordinator(registry, scheduler, std::move(c));
}

void WohNetworkSession::EnsureCoordinator(
    ECS::Registry& registry, ECS::PhaseScheduler& scheduler, Game::MultiplayerSessionConfig config) {
    m_Registry = &registry;
    m_Scheduler = &scheduler;
    m_Config = std::move(config);
    if (m_Coordinator) {
        m_Coordinator->SetConfig(m_Config);
        return;
    }
    m_Coordinator = std::make_unique<Game::NetworkSessionCoordinator>(*m_Registry, m_Config);
    m_Coordinator->RegisterSystems(*m_Scheduler);
}

void WohNetworkSession::Shutdown() {
    Disconnect();
    m_Coordinator.reset();
    if (m_Scheduler) {
        m_Scheduler->Clear();
    }
    m_Registry = nullptr;
    m_Scheduler = nullptr;
    m_LastError.clear();
}

void WohNetworkSession::Disconnect() {
    Networking::NetworkingSystem& n = Networking::NetworkingSystem::Instance();
    if (m_ClientConnection != 0) {
        n.CloseConnection(m_ClientConnection);
        m_ClientConnection = 0;
    }
    if (m_ListenSocket != 0) {
        n.CloseListenSocket(m_ListenSocket);
        m_ListenSocket = 0;
    }
}

bool WohNetworkSession::Host(uint16_t port) {
    m_LastError.clear();
    if (m_Registry == nullptr || m_Scheduler == nullptr) {
        m_LastError = "Call EnsureCoordinator(registry, scheduler) first";
        return false;
    }
    if (!m_Coordinator) {
        m_Coordinator = std::make_unique<Game::NetworkSessionCoordinator>(*m_Registry, m_Config);
        m_Coordinator->RegisterSystems(*m_Scheduler);
    }
    if (m_ListenSocket != 0) {
        return true;
    }
    std::uint64_t listen = 0;
    if (!Networking::NetworkingSystem::Instance().ListenIPv4("0.0.0.0", port, listen)) {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "ListenIPv4 0.0.0.0:%u failed", static_cast<unsigned>(port));
        m_LastError = buf;
        return false;
    }
    m_ListenSocket = listen;
    return true;
}

bool WohNetworkSession::Connect(const char* host, uint16_t port) {
    m_LastError.clear();
    if (host == nullptr || host[0] == '\0') {
        m_LastError = "empty host";
        return false;
    }
    if (m_Registry == nullptr || m_Scheduler == nullptr) {
        m_LastError = "Call EnsureCoordinator(registry, scheduler) first";
        return false;
    }
    if (!m_Coordinator) {
        m_Coordinator = std::make_unique<Game::NetworkSessionCoordinator>(*m_Registry, m_Config);
        m_Coordinator->RegisterSystems(*m_Scheduler);
    }
    if (m_ClientConnection != 0) {
        Networking::NetworkingSystem::Instance().CloseConnection(m_ClientConnection);
        m_ClientConnection = 0;
    }
    std::uint64_t conn = 0;
    if (!Networking::NetworkingSystem::Instance().ConnectHost(
            host, port, Networking::Socket::AddressFamily::IPv4, conn)) {
        char buf[192];
        std::snprintf(buf, sizeof(buf), "ConnectHost %s:%u failed", host, static_cast<unsigned>(port));
        m_LastError = buf;
        return false;
    }
    m_ClientConnection = conn;
    return true;
}

std::string WohNetworkSession::BuildStatusLine() const {
    const Networking::NetworkingSystem& n = Networking::NetworkingSystem::Instance();
    std::ostringstream o;
    o << "GNS listen=" << n.GetListenSocketCount() << " conns=" << n.GetActiveConnectionCount();
    if (m_Coordinator) {
        o << " mp=1";
    } else {
        o << " mp=0";
    }
    if (m_ListenSocket != 0) {
        o << " [hosting]";
    }
    if (m_ClientConnection != 0) {
        o << " [outbound]";
    }
    if (!m_LastError.empty()) {
        o << " err=" << m_LastError;
    }
    return o.str();
}

} // namespace Solstice::WarsOfHeaven
