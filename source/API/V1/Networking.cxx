#include "SolsticeAPI/V1/Networking.h"

#include "Networking/NetworkingSystem.hxx"
#include "Solstice.hxx"

#include <cstring>
#include <mutex>
#include <string>
#include <string_view>

extern "C" {

namespace {

std::mutex g_NetworkingCallbackMutex;
SolsticeV1_NetworkingReceiveCallback g_NetworkingCallback = nullptr;
void* g_NetworkingCallbackUserData = nullptr;

SolsticeV1_NetworkingConnectionState ToApiConnectionState(Solstice::Networking::Socket::ConnectionState state) {
    using EngineState = Solstice::Networking::Socket::ConnectionState;
    switch (state) {
    case EngineState::Connecting:
        return SolsticeV1_NetworkingConnectionStateConnecting;
    case EngineState::Connected:
        return SolsticeV1_NetworkingConnectionStateConnected;
    case EngineState::ClosedByPeer:
        return SolsticeV1_NetworkingConnectionStateClosedByPeer;
    case EngineState::ProblemDetectedLocally:
        return SolsticeV1_NetworkingConnectionStateProblemDetectedLocally;
    case EngineState::FinWait:
        return SolsticeV1_NetworkingConnectionStateFinWait;
    case EngineState::Linger:
        return SolsticeV1_NetworkingConnectionStateLinger;
    case EngineState::Dead:
        return SolsticeV1_NetworkingConnectionStateDead;
    case EngineState::None:
    default:
        return SolsticeV1_NetworkingConnectionStateNone;
    }
}

Solstice::Networking::Socket::AddressFamily ToEngineAddressFamily(SolsticeV1_NetworkingAddressFamily family) {
    using Family = Solstice::Networking::Socket::AddressFamily;
    switch (family) {
    case SolsticeV1_NetworkingAddressFamilyIPv4:
        return Family::IPv4;
    case SolsticeV1_NetworkingAddressFamilyIPv6:
        return Family::IPv6;
    case SolsticeV1_NetworkingAddressFamilyAny:
    default:
        return Family::Any;
    }
}

Solstice::Networking::Socket::RelayPolicy ToEngineRelayPolicy(SolsticeV1_NetworkingRelayPolicy policy) {
    using Policy = Solstice::Networking::Socket::RelayPolicy;
    switch (policy) {
    case SolsticeV1_NetworkingRelayPolicyPreferRelay:
        return Policy::PreferRelay;
    case SolsticeV1_NetworkingRelayPolicyForceRelayOnly:
        return Policy::ForceRelayOnly;
    case SolsticeV1_NetworkingRelayPolicyDisableRelay:
        return Policy::DisableRelay;
    case SolsticeV1_NetworkingRelayPolicyDefault:
    default:
        return Policy::Default;
    }
}

SolsticeV1_NetworkingRelayPolicy ToApiRelayPolicy(Solstice::Networking::Socket::RelayPolicy policy) {
    switch (policy) {
    case Solstice::Networking::Socket::RelayPolicy::PreferRelay:
        return SolsticeV1_NetworkingRelayPolicyPreferRelay;
    case Solstice::Networking::Socket::RelayPolicy::ForceRelayOnly:
        return SolsticeV1_NetworkingRelayPolicyForceRelayOnly;
    case Solstice::Networking::Socket::RelayPolicy::DisableRelay:
        return SolsticeV1_NetworkingRelayPolicyDisableRelay;
    case Solstice::Networking::Socket::RelayPolicy::Default:
    default:
        return SolsticeV1_NetworkingRelayPolicyDefault;
    }
}

void DispatchToCApiReceiveCallback(uint64_t connection, const Solstice::Networking::Packet& packet) {
    SolsticeV1_NetworkingReceiveCallback callback = nullptr;
    void* userData = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_NetworkingCallbackMutex);
        callback = g_NetworkingCallback;
        userData = g_NetworkingCallbackUserData;
    }
    if (!callback) {
        return;
    }

    callback(
        connection,
        packet.Empty() ? nullptr : packet.Data().data(),
        packet.Size(),
        packet.Channel(),
        packet.Mode() == Solstice::Networking::Packet::SendMode::Reliable ? SolsticeV1_True : SolsticeV1_False,
        userData);
}

SolsticeV1_ResultCode CopyPacketToOutput(
    const Solstice::Networking::Packet& packet,
    void* outBuffer,
    size_t bufferSize,
    size_t* outBytesReceived,
    int32_t* outChannel,
    SolsticeV1_Bool* outReliable) {
    if (outBytesReceived) {
        *outBytesReceived = packet.Size();
    }
    if (outChannel) {
        *outChannel = packet.Channel();
    }
    if (outReliable) {
        *outReliable = packet.Mode() == Solstice::Networking::Packet::SendMode::Reliable
            ? SolsticeV1_True
            : SolsticeV1_False;
    }

    if (!outBuffer || bufferSize < packet.Size()) {
        return SolsticeV1_ResultFailure;
    }

    if (!packet.Empty()) {
        std::memcpy(outBuffer, packet.Data().data(), packet.Size());
    }
    return SolsticeV1_ResultSuccess;
}

} // namespace

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NetworkingStart(void) {
    if (!Solstice::Initialized.load(std::memory_order_acquire)) {
        return SolsticeV1_ResultFailure;
    }
    return Solstice::Networking::NetworkingSystem::Instance().Start()
        ? SolsticeV1_ResultSuccess
        : SolsticeV1_ResultFailure;
}

SOLSTICE_V1_API void SolsticeV1_NetworkingStop(void) {
    {
        std::lock_guard<std::mutex> lock(g_NetworkingCallbackMutex);
        g_NetworkingCallback = nullptr;
        g_NetworkingCallbackUserData = nullptr;
    }
    Solstice::Networking::NetworkingSystem::Instance().ClearReceiveCallback();
    Solstice::Networking::NetworkingSystem::Instance().Stop();
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NetworkingPoll(void) {
    auto& system = Solstice::Networking::NetworkingSystem::Instance();
    if (!system.IsRunning()) {
        return SolsticeV1_ResultFailure;
    }
    system.Poll();
    return SolsticeV1_ResultSuccess;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NetworkingIsRunning(SolsticeV1_Bool* OutRunning) {
    if (!OutRunning) {
        return SolsticeV1_ResultFailure;
    }
    *OutRunning = Solstice::Networking::NetworkingSystem::Instance().IsRunning() ? SolsticeV1_True : SolsticeV1_False;
    return SolsticeV1_ResultSuccess;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NetworkingGetActiveConnectionCount(uint32_t* OutCount) {
    if (!OutCount) {
        return SolsticeV1_ResultFailure;
    }
    *OutCount = static_cast<uint32_t>(Solstice::Networking::NetworkingSystem::Instance().GetActiveConnectionCount());
    return SolsticeV1_ResultSuccess;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NetworkingGetListenSocketCount(uint32_t* OutCount) {
    if (!OutCount) {
        return SolsticeV1_ResultFailure;
    }
    *OutCount = static_cast<uint32_t>(Solstice::Networking::NetworkingSystem::Instance().GetListenSocketCount());
    return SolsticeV1_ResultSuccess;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NetworkingListenIPv4(
    const char* Address,
    uint16_t Port,
    SolsticeV1_ListenSocketHandle* OutListenSocket) {
    if (!OutListenSocket) {
        return SolsticeV1_ResultFailure;
    }
    *OutListenSocket = 0;

    auto& system = Solstice::Networking::NetworkingSystem::Instance();
    const std::string_view address = Address ? std::string_view(Address) : std::string_view();
    const bool ok = system.ListenIPv4(address, Port, *OutListenSocket);
    return ok ? SolsticeV1_ResultSuccess : SolsticeV1_ResultFailure;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NetworkingListenIPv6(
    const char* Address,
    uint16_t Port,
    SolsticeV1_ListenSocketHandle* OutListenSocket) {
    if (!OutListenSocket) {
        return SolsticeV1_ResultFailure;
    }
    *OutListenSocket = 0;

    auto& system = Solstice::Networking::NetworkingSystem::Instance();
    const std::string_view address = Address ? std::string_view(Address) : std::string_view();
    const bool ok = system.ListenIPv6(address, Port, *OutListenSocket);
    return ok ? SolsticeV1_ResultSuccess : SolsticeV1_ResultFailure;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NetworkingConnectHost(
    const char* Host,
    uint16_t Port,
    SolsticeV1_NetworkingAddressFamily FamilyPreference,
    SolsticeV1_ConnectionHandle* OutConnection) {
    if (!Host || !OutConnection) {
        return SolsticeV1_ResultFailure;
    }
    *OutConnection = 0;

    const bool ok = Solstice::Networking::NetworkingSystem::Instance().ConnectHost(
        Host,
        Port,
        ToEngineAddressFamily(FamilyPreference),
        *OutConnection);
    return ok ? SolsticeV1_ResultSuccess : SolsticeV1_ResultFailure;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NetworkingCloseConnection(SolsticeV1_ConnectionHandle Connection) {
    const bool ok = Solstice::Networking::NetworkingSystem::Instance().CloseConnection(Connection);
    return ok ? SolsticeV1_ResultSuccess : SolsticeV1_ResultFailure;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NetworkingCloseListenSocket(SolsticeV1_ListenSocketHandle ListenSocket) {
    const bool ok = Solstice::Networking::NetworkingSystem::Instance().CloseListenSocket(ListenSocket);
    return ok ? SolsticeV1_ResultSuccess : SolsticeV1_ResultFailure;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NetworkingSendEx(
    SolsticeV1_ConnectionHandle Connection,
    const void* Data,
    size_t Size,
    uint32_t SendFlags,
    int32_t Channel) {
    if (!Data || Size == 0) {
        return SolsticeV1_ResultFailure;
    }

    const bool reliable = (SendFlags & SolsticeV1_NetworkingSendFlagUnreliable) == 0;
    Solstice::Networking::Packet packet(
        Data,
        Size,
        Channel,
        reliable ? Solstice::Networking::Packet::SendMode::Reliable : Solstice::Networking::Packet::SendMode::Unreliable);
    packet.SetSendOptions(SendFlags);

    const bool ok = Solstice::Networking::NetworkingSystem::Instance().Send(Connection, packet);
    return ok ? SolsticeV1_ResultSuccess : SolsticeV1_ResultFailure;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NetworkingFlush(SolsticeV1_ConnectionHandle Connection) {
    const bool ok = Solstice::Networking::NetworkingSystem::Instance().Flush(Connection);
    return ok ? SolsticeV1_ResultSuccess : SolsticeV1_ResultFailure;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NetworkingReceive(
    SolsticeV1_ConnectionHandle Connection,
    void* OutBuffer,
    size_t BufferSize,
    size_t* OutBytesReceived,
    int32_t* OutChannel,
    SolsticeV1_Bool* OutReliable) {
    Solstice::Networking::Packet packet;
    if (!Solstice::Networking::NetworkingSystem::Instance().Receive(Connection, packet)) {
        return SolsticeV1_ResultFailure;
    }
    return CopyPacketToOutput(packet, OutBuffer, BufferSize, OutBytesReceived, OutChannel, OutReliable);
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NetworkingReceiveAny(
    SolsticeV1_ConnectionHandle* OutConnection,
    void* OutBuffer,
    size_t BufferSize,
    size_t* OutBytesReceived,
    int32_t* OutChannel,
    SolsticeV1_Bool* OutReliable) {
    if (!OutConnection) {
        return SolsticeV1_ResultFailure;
    }
    *OutConnection = 0;

    Solstice::Networking::Packet packet;
    uint64_t connection = 0;
    if (!Solstice::Networking::NetworkingSystem::Instance().ReceiveAny(connection, packet)) {
        return SolsticeV1_ResultFailure;
    }

    *OutConnection = connection;
    return CopyPacketToOutput(packet, OutBuffer, BufferSize, OutBytesReceived, OutChannel, OutReliable);
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NetworkingGetConnectionState(
    SolsticeV1_ConnectionHandle Connection,
    SolsticeV1_NetworkingConnectionState* OutState) {
    if (!OutState) {
        return SolsticeV1_ResultFailure;
    }

    *OutState = ToApiConnectionState(Solstice::Networking::NetworkingSystem::Instance().GetConnectionState(Connection));
    return SolsticeV1_ResultSuccess;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NetworkingGetRemoteAddress(
    SolsticeV1_ConnectionHandle Connection,
    char* OutAddressBuffer,
    size_t AddressBufferSize,
    uint16_t* OutPort) {
    if (!OutAddressBuffer || AddressBufferSize == 0 || !OutPort) {
        return SolsticeV1_ResultFailure;
    }

    std::string address;
    uint16_t port = 0;
    if (!Solstice::Networking::NetworkingSystem::Instance().GetRemoteAddress(Connection, address, port)) {
        return SolsticeV1_ResultFailure;
    }
    if (address.size() + 1 > AddressBufferSize) {
        return SolsticeV1_ResultFailure;
    }

    std::memcpy(OutAddressBuffer, address.c_str(), address.size() + 1);
    *OutPort = port;
    return SolsticeV1_ResultSuccess;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NetworkingSetRelayPolicy(SolsticeV1_NetworkingRelayPolicy Policy) {
    const bool ok = Solstice::Networking::NetworkingSystem::Instance().SetRelayPolicy(ToEngineRelayPolicy(Policy));
    return ok ? SolsticeV1_ResultSuccess : SolsticeV1_ResultFailure;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NetworkingGetRelayPolicy(SolsticeV1_NetworkingRelayPolicy* OutPolicy) {
    if (!OutPolicy) {
        return SolsticeV1_ResultFailure;
    }
    *OutPolicy = ToApiRelayPolicy(Solstice::Networking::NetworkingSystem::Instance().GetRelayPolicy());
    return SolsticeV1_ResultSuccess;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NetworkingEnableRelayNetworkAccess(SolsticeV1_Bool Enable) {
    Solstice::Networking::NetworkingSystem::Instance().EnableRelayNetworkAccess(Enable == SolsticeV1_True);
    return SolsticeV1_ResultSuccess;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NetworkingSetReceiveCallback(
    SolsticeV1_NetworkingReceiveCallback Callback,
    void* UserData) {
    {
        std::lock_guard<std::mutex> lock(g_NetworkingCallbackMutex);
        g_NetworkingCallback = Callback;
        g_NetworkingCallbackUserData = UserData;
    }

    if (Callback) {
        Solstice::Networking::NetworkingSystem::Instance().SetReceiveCallback(&DispatchToCApiReceiveCallback);
    } else {
        Solstice::Networking::NetworkingSystem::Instance().ClearReceiveCallback();
    }
    return SolsticeV1_ResultSuccess;
}

} // extern "C"
