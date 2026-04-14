#pragma once

#include "Packet.hxx"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace Solstice::Networking {

class Socket {
public:
    enum class AddressFamily : uint8_t {
        Any = 0,
        IPv4,
        IPv6,
    };

    enum class RelayPolicy : uint8_t {
        Default = 0,
        PreferRelay,
        ForceRelayOnly,
        DisableRelay,
    };

    using ReceiveCallback = std::function<void(uint64_t connection, const Packet& packet)>;

    enum class ConnectionState : uint8_t {
        None = 0,
        Connecting,
        Connected,
        ClosedByPeer,
        ProblemDetectedLocally,
        FinWait,
        Linger,
        Dead,
    };

    Socket();
    ~Socket();

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    bool Initialize();
    void Shutdown();
    bool IsInitialized() const;

    bool ListenIPv4(std::string_view address, uint16_t port, uint64_t& outListenSocket);
    bool ListenIPv6(std::string_view address, uint16_t port, uint64_t& outListenSocket);
    bool ConnectHost(std::string_view host, uint16_t port, AddressFamily familyPreference, uint64_t& outConnection);
    bool CloseConnection(uint64_t connection);
    bool CloseListenSocket(uint64_t listenSocket);

    bool Send(uint64_t connection, const Packet& packet);
    bool Flush(uint64_t connection);
    bool Receive(uint64_t connection, Packet& outPacket);
    bool ReceiveAny(uint64_t& outConnection, Packet& outPacket);

    bool SetRelayPolicy(RelayPolicy policy);
    RelayPolicy GetRelayPolicy() const;
    void EnableRelayNetworkAccess(bool enable);
    void SetReceiveCallback(ReceiveCallback callback);
    void ClearReceiveCallback();

    ConnectionState GetConnectionState(uint64_t connection) const;
    bool GetRemoteAddress(uint64_t connection, std::string& outAddress, uint16_t& outPort) const;
    int GetActiveConnectionCount() const;
    int GetListenSocketCount() const;
    void Poll();

    struct SocketImpl;

private:
    std::unique_ptr<SocketImpl> m_Impl;
};

} // namespace Solstice::Networking
