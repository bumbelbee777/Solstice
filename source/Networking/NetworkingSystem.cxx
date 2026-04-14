#include "NetworkingSystem.hxx"

#include "../Core/Profiling/Profiler.hxx"
#include "../Core/Profiling/ScopeTimer.hxx"
#include <Plugin/SubsystemHooks.hxx>

#include <utility>

namespace Solstice::Networking {

bool NetworkingSystem::Start() {
    PROFILE_SCOPE("Networking.Start");
    if (m_Running) {
        return true;
    }

    m_Running = m_Socket.Initialize();
    return m_Running;
}

void NetworkingSystem::Stop() {
    PROFILE_SCOPE("Networking.Stop");
    if (!m_Running) {
        return;
    }

    m_Socket.Shutdown();
    m_Running = false;
}

void NetworkingSystem::Poll() {
    PROFILE_SCOPE("Networking.Poll");
    if (!m_Running) {
        return;
    }

    Solstice::Plugin::SubsystemHooks::Instance().Invoke(Solstice::Plugin::SubsystemHookKind::NetworkingPrePoll, 0.f);
    m_Socket.Poll();
    Solstice::Plugin::SubsystemHooks::Instance().Invoke(Solstice::Plugin::SubsystemHookKind::NetworkingPostPoll, 0.f);
    Core::Profiler::Instance().SetCounter("Networking.ConnectionsActive", static_cast<int64_t>(m_Socket.GetActiveConnectionCount()));
}

bool NetworkingSystem::ListenIPv4(std::string_view address, uint16_t port, uint64_t& outListenSocket) {
    PROFILE_SCOPE("Networking.ListenIPv4");
    if (!m_Running) {
        return false;
    }
    return m_Socket.ListenIPv4(address, port, outListenSocket);
}

bool NetworkingSystem::ListenIPv6(std::string_view address, uint16_t port, uint64_t& outListenSocket) {
    PROFILE_SCOPE("Networking.ListenIPv6");
    if (!m_Running) {
        return false;
    }
    return m_Socket.ListenIPv6(address, port, outListenSocket);
}

bool NetworkingSystem::ConnectHost(std::string_view host, uint16_t port, Socket::AddressFamily familyPreference, uint64_t& outConnection) {
    PROFILE_SCOPE("Networking.ConnectHost");
    if (!m_Running) {
        return false;
    }
    return m_Socket.ConnectHost(host, port, familyPreference, outConnection);
}

bool NetworkingSystem::CloseConnection(uint64_t connection) {
    PROFILE_SCOPE("Networking.CloseConnection");
    if (!m_Running) {
        return false;
    }
    return m_Socket.CloseConnection(connection);
}

bool NetworkingSystem::CloseListenSocket(uint64_t listenSocket) {
    PROFILE_SCOPE("Networking.CloseListenSocket");
    if (!m_Running) {
        return false;
    }
    return m_Socket.CloseListenSocket(listenSocket);
}

bool NetworkingSystem::Send(uint64_t connection, const Packet& packet) {
    PROFILE_SCOPE("Networking.Send");
    if (!m_Running) {
        return false;
    }
    return m_Socket.Send(connection, packet);
}

bool NetworkingSystem::Flush(uint64_t connection) {
    PROFILE_SCOPE("Networking.Flush");
    if (!m_Running) {
        return false;
    }
    return m_Socket.Flush(connection);
}

bool NetworkingSystem::Receive(uint64_t connection, Packet& outPacket) {
    PROFILE_SCOPE("Networking.Receive");
    if (!m_Running) {
        return false;
    }
    return m_Socket.Receive(connection, outPacket);
}

bool NetworkingSystem::ReceiveAny(uint64_t& outConnection, Packet& outPacket) {
    PROFILE_SCOPE("Networking.ReceiveAny");
    if (!m_Running) {
        outConnection = 0;
        return false;
    }
    return m_Socket.ReceiveAny(outConnection, outPacket);
}

bool NetworkingSystem::SetRelayPolicy(Socket::RelayPolicy policy) {
    if (!m_Running) {
        return false;
    }
    return m_Socket.SetRelayPolicy(policy);
}

Socket::RelayPolicy NetworkingSystem::GetRelayPolicy() const {
    return m_Socket.GetRelayPolicy();
}

void NetworkingSystem::EnableRelayNetworkAccess(bool enable) {
    if (!m_Running) {
        return;
    }
    m_Socket.EnableRelayNetworkAccess(enable);
}

void NetworkingSystem::SetReceiveCallback(ReceiveCallback callback) {
    m_Socket.SetReceiveCallback(std::move(callback));
}

void NetworkingSystem::ClearReceiveCallback() {
    m_Socket.ClearReceiveCallback();
}

Socket::ConnectionState NetworkingSystem::GetConnectionState(uint64_t connection) const {
    if (!m_Running) {
        return Socket::ConnectionState::None;
    }
    return m_Socket.GetConnectionState(connection);
}

bool NetworkingSystem::GetRemoteAddress(uint64_t connection, std::string& outAddress, uint16_t& outPort) const {
    if (!m_Running) {
        return false;
    }
    return m_Socket.GetRemoteAddress(connection, outAddress, outPort);
}

int NetworkingSystem::GetActiveConnectionCount() const {
    if (!m_Running) {
        return 0;
    }
    return m_Socket.GetActiveConnectionCount();
}

int NetworkingSystem::GetListenSocketCount() const {
    if (!m_Running) {
        return 0;
    }
    return m_Socket.GetListenSocketCount();
}

} // namespace Solstice::Networking
