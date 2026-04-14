#include "Socket.hxx"

#include "../Core/Debug/Debug.hxx"
#include "../Core/Profiling/Profiler.hxx"
#include "../Core/Profiling/ScopeTimer.hxx"

#include <steam/isteamnetworkingutils.h>
#include <steam/steamnetworkingsockets.h>

#include <cstring>
#include <deque>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <Ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#endif

namespace Solstice::Networking {

namespace {
Socket::SocketImpl* g_ActiveImpl = nullptr;

Socket::ConnectionState ToConnectionState(ESteamNetworkingConnectionState state) {
    switch (state) {
    case k_ESteamNetworkingConnectionState_Connecting:
        return Socket::ConnectionState::Connecting;
    case k_ESteamNetworkingConnectionState_Connected:
        return Socket::ConnectionState::Connected;
    case k_ESteamNetworkingConnectionState_ClosedByPeer:
        return Socket::ConnectionState::ClosedByPeer;
    case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
        return Socket::ConnectionState::ProblemDetectedLocally;
    case k_ESteamNetworkingConnectionState_FinWait:
        return Socket::ConnectionState::FinWait;
    case k_ESteamNetworkingConnectionState_Linger:
        return Socket::ConnectionState::Linger;
    case k_ESteamNetworkingConnectionState_Dead:
        return Socket::ConnectionState::Dead;
    case k_ESteamNetworkingConnectionState_None:
    default:
        return Socket::ConnectionState::None;
    }
}

void DebugOutput(ESteamNetworkingSocketsDebugOutputType, const char* message) {
    if (message) {
        SIMPLE_LOG(std::string("Networking: ") + message);
    }
}

bool ParseEndpointWithPort(
    std::string_view hostOrAddress,
    uint16_t port,
    Socket::AddressFamily family,
    SteamNetworkingIPAddr& outAddress) {
    if (hostOrAddress.empty()) {
        if (family == Socket::AddressFamily::IPv6) {
            outAddress.Clear();
            outAddress.m_port = port;
        } else {
            outAddress.SetIPv4(0, port);
        }
        return true;
    }

    const bool hasColon = hostOrAddress.find(':') != std::string_view::npos;
    const bool hasBrackets = hostOrAddress.front() == '[' && hostOrAddress.back() == ']';
    std::string endpoint(hostOrAddress);
    if (hasColon && !hasBrackets) {
        endpoint = "[" + endpoint + "]";
    }
    endpoint += ":" + std::to_string(port);

    if (!outAddress.ParseString(endpoint.c_str())) {
        return false;
    }

    // Keep family-specific APIs strict to avoid accidental dual-stack misconfiguration.
    if (family == Socket::AddressFamily::IPv6) {
        return !outAddress.IsIPv4();
    }
    if (family == Socket::AddressFamily::IPv4) {
        return outAddress.IsIPv4();
    }
    return true;
}

std::optional<SteamNetworkingIPAddr> ResolveHostToAddress(
    std::string_view host,
    uint16_t port,
    Socket::AddressFamily familyPreference) {
    if (host.empty()) {
        return std::nullopt;
    }

    addrinfo hints = {};
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    switch (familyPreference) {
    case Socket::AddressFamily::IPv4:
        hints.ai_family = AF_INET;
        break;
    case Socket::AddressFamily::IPv6:
        hints.ai_family = AF_INET6;
        break;
    case Socket::AddressFamily::Any:
    default:
        hints.ai_family = AF_UNSPEC;
        break;
    }

    const std::string service = std::to_string(port);
    addrinfo* result = nullptr;
    if (getaddrinfo(std::string(host).c_str(), service.c_str(), &hints, &result) != 0 || !result) {
        return std::nullopt;
    }

    std::optional<SteamNetworkingIPAddr> resolvedAddress;
    for (addrinfo* current = result; current != nullptr; current = current->ai_next) {
        if (current->ai_family == AF_INET && current->ai_addrlen >= static_cast<int>(sizeof(sockaddr_in))) {
            const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(current->ai_addr);
            SteamNetworkingIPAddr addr;
            addr.SetIPv4(ntohl(ipv4->sin_addr.s_addr), ntohs(ipv4->sin_port));
            resolvedAddress = addr;
            break;
        }
        if (current->ai_family == AF_INET6 && current->ai_addrlen >= static_cast<int>(sizeof(sockaddr_in6))) {
            const auto* ipv6 = reinterpret_cast<const sockaddr_in6*>(current->ai_addr);
            SteamNetworkingIPAddr addr;
            addr.SetIPv6(reinterpret_cast<const uint8_t*>(&ipv6->sin6_addr), ntohs(ipv6->sin6_port));
            resolvedAddress = addr;
            break;
        }
    }

    freeaddrinfo(result);
    return resolvedAddress;
}

int ToSteamSendFlags(const Packet& packet) {
    uint32_t sendOptions = packet.SendOptions();
    if (sendOptions == Packet::SendFlags::None) {
        sendOptions = packet.Mode() == Packet::SendMode::Reliable
            ? Packet::SendFlags::ReliableFlag
            : (Packet::SendFlags::UnreliableFlag | Packet::SendFlags::NoNagleFlag);
    }

    int flags = 0;
    if ((sendOptions & Packet::SendFlags::ReliableFlag) != 0) {
        flags |= k_nSteamNetworkingSend_Reliable;
    } else {
        flags |= k_nSteamNetworkingSend_Unreliable;
    }
    if ((sendOptions & Packet::SendFlags::NoNagleFlag) != 0) {
        flags |= k_nSteamNetworkingSend_NoNagle;
    }
    if ((sendOptions & Packet::SendFlags::NoDelayFlag) != 0) {
        flags |= k_nSteamNetworkingSend_NoDelay;
    }
    return flags;
}
} // namespace

struct Socket::SocketImpl {
    ISteamNetworkingSockets* Interface{nullptr};
    bool Initialized{false};
    mutable std::mutex Mutex;
    mutable std::mutex CallbackMutex;
    std::unordered_map<uint64_t, ConnectionState> ConnectionStates;
    std::unordered_set<uint64_t> ActiveConnections;
    std::unordered_set<uint64_t> ListenSockets;
    RelayPolicy RelayMode{RelayPolicy::Default};
    ReceiveCallback OnReceive;

    static void OnConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* callback) {
        if (g_ActiveImpl && callback) {
            g_ActiveImpl->HandleConnectionStatusChanged(*callback);
        }
    }

    bool ApplyRelayPolicy() {
        ISteamNetworkingUtils* utils = SteamNetworkingUtils();
        if (!utils) {
            return false;
        }

        int32_t iceEnable = k_nSteamNetworkingConfig_P2P_Transport_ICE_Enable_Default;
        int32_t sdrPenalty = 0;
        switch (RelayMode) {
        case RelayPolicy::Default:
            iceEnable = k_nSteamNetworkingConfig_P2P_Transport_ICE_Enable_Default;
            sdrPenalty = 0;
            break;
        case RelayPolicy::PreferRelay:
            iceEnable = k_nSteamNetworkingConfig_P2P_Transport_ICE_Enable_All;
            sdrPenalty = -1000;
            break;
        case RelayPolicy::ForceRelayOnly:
            iceEnable = k_nSteamNetworkingConfig_P2P_Transport_ICE_Enable_Relay;
            sdrPenalty = -5000;
            break;
        case RelayPolicy::DisableRelay:
            iceEnable = k_nSteamNetworkingConfig_P2P_Transport_ICE_Enable_Private | k_nSteamNetworkingConfig_P2P_Transport_ICE_Enable_Public;
            sdrPenalty = 1000000;
            break;
        }

        const bool setIce = utils->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_P2P_Transport_ICE_Enable, iceEnable);
        const bool setPenalty = utils->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_P2P_Transport_SDR_Penalty, sdrPenalty);
        if (!setIce || !setPenalty) {
            SIMPLE_LOG("Networking: relay policy controls not fully supported by this backend/config; policy retained for future connections");
        }
        return true;
    }

    void DispatchReceiveCallback(uint64_t connection, const Packet& packet) {
        ReceiveCallback callbackCopy;
        {
            std::lock_guard<std::mutex> lock(CallbackMutex);
            callbackCopy = OnReceive;
        }
        if (callbackCopy) {
            callbackCopy(connection, packet);
        }
    }

    void HandleConnectionStatusChanged(const SteamNetConnectionStatusChangedCallback_t& callback) {
        const uint64_t connection = static_cast<uint64_t>(callback.m_hConn);
        const ConnectionState state = ToConnectionState(callback.m_info.m_eState);

        if (callback.m_info.m_eState == k_ESteamNetworkingConnectionState_Connecting
            && callback.m_info.m_hListenSocket != k_HSteamListenSocket_Invalid) {
            Interface->AcceptConnection(callback.m_hConn);
        }

        {
            std::lock_guard<std::mutex> lock(Mutex);
            ConnectionStates[connection] = state;
            if (state == ConnectionState::Connected || state == ConnectionState::Connecting) {
                ActiveConnections.insert(connection);
            } else if (state == ConnectionState::ClosedByPeer || state == ConnectionState::ProblemDetectedLocally
                       || state == ConnectionState::Dead) {
                ActiveConnections.erase(connection);
            }
        }

        Core::Profiler::Instance().SetCounter("Networking.ConnectionsActive", static_cast<int64_t>(GetActiveConnectionCount()));
    }

    int GetActiveConnectionCount() const {
        std::lock_guard<std::mutex> lock(Mutex);
        return static_cast<int>(ActiveConnections.size());
    }
};

Socket::Socket() : m_Impl(std::make_unique<SocketImpl>()) {}

Socket::~Socket() {
    Shutdown();
}

bool Socket::Initialize() {
    PROFILE_SCOPE("Networking.Socket.Initialize");
    if (m_Impl->Initialized) {
        return true;
    }

    SteamDatagramErrMsg errorMessage;
    if (!GameNetworkingSockets_Init(nullptr, errorMessage)) {
        SIMPLE_LOG(std::string("Networking: GameNetworkingSockets_Init failed: ") + errorMessage);
        return false;
    }

    m_Impl->Interface = SteamNetworkingSockets();
    if (!m_Impl->Interface) {
        SIMPLE_LOG("Networking: failed to get ISteamNetworkingSockets interface");
        GameNetworkingSockets_Kill();
        return false;
    }

    SteamNetworkingUtils()->SetDebugOutputFunction(k_ESteamNetworkingSocketsDebugOutputType_Warning, DebugOutput);
    SteamNetworkingUtils()->SetGlobalCallback_SteamNetConnectionStatusChanged(&SocketImpl::OnConnectionStatusChanged);
    m_Impl->ApplyRelayPolicy();

    g_ActiveImpl = m_Impl.get();
    m_Impl->Initialized = true;
    Core::Profiler::Instance().SetCounter("Networking.ConnectionsActive", 0);
    return true;
}

void Socket::Shutdown() {
    PROFILE_SCOPE("Networking.Socket.Shutdown");
    if (!m_Impl->Initialized) {
        return;
    }

    std::vector<uint64_t> connections;
    std::vector<uint64_t> listenSockets;
    {
        std::lock_guard<std::mutex> lock(m_Impl->Mutex);
        connections.assign(m_Impl->ActiveConnections.begin(), m_Impl->ActiveConnections.end());
        listenSockets.assign(m_Impl->ListenSockets.begin(), m_Impl->ListenSockets.end());
    }

    for (const uint64_t connection : connections) {
        m_Impl->Interface->CloseConnection(static_cast<HSteamNetConnection>(connection), 0, "Shutdown", false);
    }
    for (const uint64_t listenSocket : listenSockets) {
        m_Impl->Interface->CloseListenSocket(static_cast<HSteamListenSocket>(listenSocket));
    }

    m_Impl->Interface->RunCallbacks();
    g_ActiveImpl = nullptr;
    GameNetworkingSockets_Kill();

    {
        std::lock_guard<std::mutex> lock(m_Impl->Mutex);
        m_Impl->ConnectionStates.clear();
        m_Impl->ActiveConnections.clear();
        m_Impl->ListenSockets.clear();
    }
    {
        std::lock_guard<std::mutex> lock(m_Impl->CallbackMutex);
        m_Impl->OnReceive = nullptr;
    }

    m_Impl->Interface = nullptr;
    m_Impl->Initialized = false;
    Core::Profiler::Instance().SetCounter("Networking.ConnectionsActive", 0);
    Core::Profiler::Instance().SetCounter("Networking.ListenSocketsActive", 0);
}

bool Socket::IsInitialized() const {
    return m_Impl->Initialized;
}

bool Socket::ListenIPv4(std::string_view address, uint16_t port, uint64_t& outListenSocket) {
    PROFILE_SCOPE("Networking.Socket.Listen");
    outListenSocket = 0;
    if (!m_Impl->Initialized || !m_Impl->Interface) {
        return false;
    }

    SteamNetworkingIPAddr listenAddress;
    if (!ParseEndpointWithPort(address, port, AddressFamily::IPv4, listenAddress)) {
        return false;
    }

    const HSteamListenSocket listenSocket = m_Impl->Interface->CreateListenSocketIP(listenAddress, 0, nullptr);
    if (listenSocket == k_HSteamListenSocket_Invalid) {
        return false;
    }

    outListenSocket = static_cast<uint64_t>(listenSocket);
    {
        std::lock_guard<std::mutex> lock(m_Impl->Mutex);
        m_Impl->ListenSockets.insert(outListenSocket);
    }

    Core::Profiler::Instance().IncrementCounter("Networking.ListenSocketsCreated");
    Core::Profiler::Instance().SetCounter("Networking.ListenSocketsActive", static_cast<int64_t>(GetListenSocketCount()));
    return true;
}

bool Socket::ListenIPv6(std::string_view address, uint16_t port, uint64_t& outListenSocket) {
    PROFILE_SCOPE("Networking.Socket.Listen");
    outListenSocket = 0;
    if (!m_Impl->Initialized || !m_Impl->Interface) {
        return false;
    }

    SteamNetworkingIPAddr listenAddress;
    if (!ParseEndpointWithPort(address, port, AddressFamily::IPv6, listenAddress)) {
        return false;
    }

    const HSteamListenSocket listenSocket = m_Impl->Interface->CreateListenSocketIP(listenAddress, 0, nullptr);
    if (listenSocket == k_HSteamListenSocket_Invalid) {
        return false;
    }

    outListenSocket = static_cast<uint64_t>(listenSocket);
    {
        std::lock_guard<std::mutex> lock(m_Impl->Mutex);
        m_Impl->ListenSockets.insert(outListenSocket);
    }

    Core::Profiler::Instance().IncrementCounter("Networking.ListenSocketsCreated");
    Core::Profiler::Instance().SetCounter("Networking.ListenSocketsActive", static_cast<int64_t>(GetListenSocketCount()));
    return true;
}

bool Socket::ConnectHost(std::string_view host, uint16_t port, AddressFamily familyPreference, uint64_t& outConnection) {
    PROFILE_SCOPE("Networking.Socket.ConnectHost");
    outConnection = 0;
    if (!m_Impl->Initialized || !m_Impl->Interface || host.empty()) {
        return false;
    }

    auto resolvedAddress = ResolveHostToAddress(host, port, familyPreference);
    if (!resolvedAddress.has_value()) {
        Core::Profiler::Instance().IncrementCounter("Networking.ConnectionFailed");
        return false;
    }

    const HSteamNetConnection connection = m_Impl->Interface->ConnectByIPAddress(*resolvedAddress, 0, nullptr);
    if (connection == k_HSteamNetConnection_Invalid) {
        Core::Profiler::Instance().IncrementCounter("Networking.ConnectionFailed");
        return false;
    }

    outConnection = static_cast<uint64_t>(connection);
    {
        std::lock_guard<std::mutex> lock(m_Impl->Mutex);
        m_Impl->ConnectionStates[outConnection] = ConnectionState::Connecting;
        m_Impl->ActiveConnections.insert(outConnection);
    }

    Core::Profiler::Instance().IncrementCounter("Networking.ConnectionAttempts");
    Core::Profiler::Instance().SetCounter("Networking.ConnectionsActive", static_cast<int64_t>(GetActiveConnectionCount()));
    return true;
}

bool Socket::CloseConnection(uint64_t connection) {
    PROFILE_SCOPE("Networking.Socket.CloseConnection");
    if (!m_Impl->Initialized || !m_Impl->Interface || connection == 0) {
        return false;
    }

    const bool result = m_Impl->Interface->CloseConnection(static_cast<HSteamNetConnection>(connection), 0, "Closed", false);
    {
        std::lock_guard<std::mutex> lock(m_Impl->Mutex);
        m_Impl->ActiveConnections.erase(connection);
        m_Impl->ConnectionStates[connection] = ConnectionState::Dead;
    }
    Core::Profiler::Instance().SetCounter("Networking.ConnectionsActive", static_cast<int64_t>(GetActiveConnectionCount()));
    return result;
}

bool Socket::CloseListenSocket(uint64_t listenSocket) {
    PROFILE_SCOPE("Networking.Socket.CloseListenSocket");
    if (!m_Impl->Initialized || !m_Impl->Interface || listenSocket == 0) {
        return false;
    }

    const bool result = m_Impl->Interface->CloseListenSocket(static_cast<HSteamListenSocket>(listenSocket));
    if (result) {
        std::lock_guard<std::mutex> lock(m_Impl->Mutex);
        m_Impl->ListenSockets.erase(listenSocket);
        Core::Profiler::Instance().SetCounter("Networking.ListenSocketsActive", static_cast<int64_t>(m_Impl->ListenSockets.size()));
    }
    return result;
}

bool Socket::Send(uint64_t connection, const Packet& packet) {
    PROFILE_SCOPE("Networking.Socket.Send");
    if (!m_Impl->Initialized || !m_Impl->Interface || connection == 0 || packet.Empty()) {
        return false;
    }

    const int sendFlags = ToSteamSendFlags(packet);
    constexpr size_t kPacketHeaderSize = sizeof(int32_t) + sizeof(uint32_t);
    if (packet.Size() > static_cast<size_t>(std::numeric_limits<uint32>::max()) - kPacketHeaderSize) {
        return false;
    }

    std::vector<uint8_t> wireData;
    wireData.resize(kPacketHeaderSize + packet.Size());

    const int32_t channel = static_cast<int32_t>(packet.Channel());
    const uint32_t sendOptions = packet.SendOptions();
    std::memcpy(wireData.data(), &channel, sizeof(channel));
    std::memcpy(wireData.data() + sizeof(channel), &sendOptions, sizeof(sendOptions));
    if (!packet.Empty()) {
        std::memcpy(wireData.data() + kPacketHeaderSize, packet.Data().data(), packet.Size());
    }

    const EResult result = m_Impl->Interface->SendMessageToConnection(
        static_cast<HSteamNetConnection>(connection),
        wireData.data(),
        static_cast<uint32>(wireData.size()),
        sendFlags,
        nullptr);
    if (result != k_EResultOK) {
        return false;
    }

    Core::Profiler::Instance().IncrementCounter("Networking.PacketsSent");
    Core::Profiler::Instance().IncrementCounter("Networking.BytesSent", static_cast<int64_t>(packet.Size()));
    return true;
}

bool Socket::Flush(uint64_t connection) {
    PROFILE_SCOPE("Networking.Socket.Flush");
    if (!m_Impl->Initialized || !m_Impl->Interface || connection == 0) {
        return false;
    }
    return m_Impl->Interface->FlushMessagesOnConnection(static_cast<HSteamNetConnection>(connection)) == k_EResultOK;
}

bool Socket::Receive(uint64_t connection, Packet& outPacket) {
    PROFILE_SCOPE("Networking.Socket.Receive");
    if (!m_Impl->Initialized || !m_Impl->Interface || connection == 0) {
        return false;
    }

    ISteamNetworkingMessage* message = nullptr;
    const int messageCount = m_Impl->Interface->ReceiveMessagesOnConnection(
        static_cast<HSteamNetConnection>(connection),
        &message,
        1);

    if (messageCount <= 0 || !message) {
        return false;
    }

    constexpr size_t kPacketHeaderSize = sizeof(int32_t) + sizeof(uint32_t);
    const auto* messageData = static_cast<const uint8_t*>(message->GetData());
    const size_t messageSize = static_cast<size_t>(message->GetSize());
    Packet::SendMode mode = (message->m_nFlags & (k_nSteamNetworkingSend_Unreliable | k_nSteamNetworkingSend_UnreliableNoNagle)) != 0
        ? Packet::SendMode::Unreliable
        : Packet::SendMode::Reliable;
    uint32_t sendOptions = Packet::SendFlags::None;
    int channel = 0;
    size_t payloadOffset = 0;

    if (messageData && messageSize >= kPacketHeaderSize) {
        int32_t parsedChannel = 0;
        std::memcpy(&parsedChannel, messageData, sizeof(parsedChannel));
        std::memcpy(&sendOptions, messageData + sizeof(parsedChannel), sizeof(sendOptions));
        channel = static_cast<int>(parsedChannel);
        mode = (sendOptions & Packet::SendFlags::UnreliableFlag) != 0
            ? Packet::SendMode::Unreliable
            : Packet::SendMode::Reliable;
        payloadOffset = kPacketHeaderSize;
    } else {
        sendOptions = (mode == Packet::SendMode::Reliable)
            ? Packet::SendFlags::ReliableFlag
            : Packet::SendFlags::UnreliableFlag;
    }

    outPacket = Packet(
        messageData ? (messageData + payloadOffset) : nullptr,
        messageSize >= payloadOffset ? (messageSize - payloadOffset) : 0,
        channel,
        mode);
    outPacket.SetSendOptions(sendOptions);

    Core::Profiler::Instance().IncrementCounter("Networking.PacketsRecv");
    Core::Profiler::Instance().IncrementCounter("Networking.BytesRecv", static_cast<int64_t>(outPacket.Size()));
    message->Release();
    return true;
}

bool Socket::ReceiveAny(uint64_t& outConnection, Packet& outPacket) {
    PROFILE_SCOPE("Networking.Socket.ReceiveAny");
    outConnection = 0;
    if (!m_Impl->Initialized || !m_Impl->Interface) {
        return false;
    }

    std::vector<uint64_t> activeConnections;
    {
        std::lock_guard<std::mutex> lock(m_Impl->Mutex);
        activeConnections.assign(m_Impl->ActiveConnections.begin(), m_Impl->ActiveConnections.end());
    }

    for (const uint64_t connection : activeConnections) {
        if (Receive(connection, outPacket)) {
            outConnection = connection;
            return true;
        }
    }

    return false;
}

bool Socket::SetRelayPolicy(RelayPolicy policy) {
    if (!m_Impl->Initialized || !m_Impl->Interface) {
        return false;
    }
    m_Impl->RelayMode = policy;
    return m_Impl->ApplyRelayPolicy();
}

Socket::RelayPolicy Socket::GetRelayPolicy() const {
    return m_Impl->RelayMode;
}

void Socket::EnableRelayNetworkAccess(bool enable) {
    if (!m_Impl->Initialized || !m_Impl->Interface || !enable) {
        return;
    }
    SteamNetworkingUtils()->InitRelayNetworkAccess();
}

void Socket::SetReceiveCallback(ReceiveCallback callback) {
    std::lock_guard<std::mutex> lock(m_Impl->CallbackMutex);
    m_Impl->OnReceive = std::move(callback);
}

void Socket::ClearReceiveCallback() {
    std::lock_guard<std::mutex> lock(m_Impl->CallbackMutex);
    m_Impl->OnReceive = nullptr;
}

Socket::ConnectionState Socket::GetConnectionState(uint64_t connection) const {
    if (!m_Impl->Initialized || !m_Impl->Interface || connection == 0) {
        return ConnectionState::None;
    }

    SteamNetConnectionInfo_t info;
    if (m_Impl->Interface->GetConnectionInfo(static_cast<HSteamNetConnection>(connection), &info)) {
        return ToConnectionState(info.m_eState);
    }

    std::lock_guard<std::mutex> lock(m_Impl->Mutex);
    const auto found = m_Impl->ConnectionStates.find(connection);
    if (found != m_Impl->ConnectionStates.end()) {
        return found->second;
    }
    return ConnectionState::None;
}

int Socket::GetActiveConnectionCount() const {
    return m_Impl->GetActiveConnectionCount();
}

int Socket::GetListenSocketCount() const {
    std::lock_guard<std::mutex> lock(m_Impl->Mutex);
    return static_cast<int>(m_Impl->ListenSockets.size());
}

bool Socket::GetRemoteAddress(uint64_t connection, std::string& outAddress, uint16_t& outPort) const {
    outAddress.clear();
    outPort = 0;
    if (!m_Impl->Initialized || !m_Impl->Interface || connection == 0) {
        return false;
    }

    SteamNetConnectionInfo_t info;
    if (!m_Impl->Interface->GetConnectionInfo(static_cast<HSteamNetConnection>(connection), &info)) {
        return false;
    }

    char addressBuffer[SteamNetworkingIPAddr::k_cchMaxString] = {};
    info.m_addrRemote.ToString(addressBuffer, sizeof(addressBuffer), false);
    outAddress = addressBuffer;
    outPort = info.m_addrRemote.m_port;
    return true;
}

void Socket::Poll() {
    PROFILE_SCOPE("Networking.Socket.Poll");
    if (!m_Impl->Initialized || !m_Impl->Interface) {
        return;
    }
    m_Impl->Interface->RunCallbacks();
    bool hasCallback = false;
    {
        std::lock_guard<std::mutex> lock(m_Impl->CallbackMutex);
        hasCallback = static_cast<bool>(m_Impl->OnReceive);
    }
    if (hasCallback) {
        constexpr int kMaxPacketsPerPoll = 1024;
        for (int i = 0; i < kMaxPacketsPerPoll; ++i) {
            uint64_t connection = 0;
            Packet packet;
            if (!ReceiveAny(connection, packet)) {
                break;
            }
            m_Impl->DispatchReceiveCallback(connection, packet);
        }
    }
    Core::Profiler::Instance().SetCounter("Networking.ConnectionsActive", static_cast<int64_t>(GetActiveConnectionCount()));
}

} // namespace Solstice::Networking
