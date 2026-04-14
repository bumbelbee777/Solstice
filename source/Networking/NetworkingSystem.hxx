#pragma once

#include "../Solstice.hxx"
#include "Packet.hxx"
#include "Socket.hxx"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace Solstice::Networking {

class SOLSTICE_API NetworkingSystem {
public:
    using ReceiveCallback = Socket::ReceiveCallback;

    static NetworkingSystem& Instance() {
        static NetworkingSystem instance;
        return instance;
    }

    bool Start();
    void Stop();
    bool IsRunning() const { return m_Running; }

    void Poll();

    bool ListenIPv4(std::string_view address, uint16_t port, uint64_t& outListenSocket);
    bool ListenIPv6(std::string_view address, uint16_t port, uint64_t& outListenSocket);
    bool ConnectHost(std::string_view host, uint16_t port, Socket::AddressFamily familyPreference, uint64_t& outConnection);
    bool CloseConnection(uint64_t connection);
    bool CloseListenSocket(uint64_t listenSocket);

    bool Send(uint64_t connection, const Packet& packet);
    bool Flush(uint64_t connection);
    bool Receive(uint64_t connection, Packet& outPacket);
    bool ReceiveAny(uint64_t& outConnection, Packet& outPacket);
    bool SetRelayPolicy(Socket::RelayPolicy policy);
    Socket::RelayPolicy GetRelayPolicy() const;
    void EnableRelayNetworkAccess(bool enable);
    void SetReceiveCallback(ReceiveCallback callback);
    void ClearReceiveCallback();
    Socket::ConnectionState GetConnectionState(uint64_t connection) const;
    bool GetRemoteAddress(uint64_t connection, std::string& outAddress, uint16_t& outPort) const;
    int GetActiveConnectionCount() const;
    int GetListenSocketCount() const;

private:
    NetworkingSystem() = default;

    Socket m_Socket;
    bool m_Running{false};
};

} // namespace Solstice::Networking
