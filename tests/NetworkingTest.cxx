#include "Solstice.hxx"
#include "Networking/NetworkingSystem.hxx"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <thread>

namespace {

int Fail(int code, const char* message) {
    std::fprintf(stderr, "[NetworkingTest] FAIL (%d): %s\n", code, message);
    return code;
}

bool WaitForConnected(
    Solstice::Networking::NetworkingSystem& networking,
    uint64_t connection,
    std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        networking.Poll();
        const auto state = networking.GetConnectionState(connection);
        if (state == Solstice::Networking::Socket::ConnectionState::Connected) {
            return true;
        }
        if (state == Solstice::Networking::Socket::ConnectionState::ProblemDetectedLocally
            || state == Solstice::Networking::Socket::ConnectionState::ClosedByPeer
            || state == Solstice::Networking::Socket::ConnectionState::Dead) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return false;
}

int ExerciseLoopbackScenario(
    Solstice::Networking::NetworkingSystem& networking,
    int failBase,
    std::string_view listenAddress,
    std::string_view connectHost,
    uint16_t port,
    bool useIPv6) {
    uint64_t listenSocket = 0;
    const bool listenOk = useIPv6
        ? networking.ListenIPv6(listenAddress, port, listenSocket)
        : networking.ListenIPv4(listenAddress, port, listenSocket);
    if (!listenOk || listenSocket == 0) {
        return Fail(failBase + 1, "Listen failed");
    }

    uint64_t clientConnection = 0;
    const bool connectOk = networking.ConnectHost(
        connectHost,
        port,
        useIPv6 ? Solstice::Networking::Socket::AddressFamily::IPv6 : Solstice::Networking::Socket::AddressFamily::IPv4,
        clientConnection);
    if (!connectOk || clientConnection == 0) {
        networking.CloseListenSocket(listenSocket);
        return Fail(failBase + 2, "Connect failed");
    }

    if (!WaitForConnected(networking, clientConnection, std::chrono::seconds(3))) {
        networking.CloseConnection(clientConnection);
        networking.CloseListenSocket(listenSocket);
        return Fail(failBase + 3, "Client connection did not reach Connected state");
    }

    const char* payload = "hello-from-client";
    Solstice::Networking::Packet outbound(
        payload,
        std::strlen(payload),
        1,
        Solstice::Networking::Packet::SendMode::Reliable);
    outbound.SetSendOptions(
        Solstice::Networking::Packet::ReliableFlag
        | Solstice::Networking::Packet::NoDelayFlag);
    if (!networking.Send(clientConnection, outbound)) {
        networking.CloseConnection(clientConnection);
        networking.CloseListenSocket(listenSocket);
        return Fail(failBase + 4, "Send reliable packet failed");
    }

    uint64_t serverConnection = 0;
    Solstice::Networking::Packet inboundFromClient;
    const auto receiveDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < receiveDeadline) {
        networking.Poll();
        if (networking.ReceiveAny(serverConnection, inboundFromClient)) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (serverConnection == 0 || inboundFromClient.Empty()) {
        networking.CloseConnection(clientConnection);
        networking.CloseListenSocket(listenSocket);
        return Fail(failBase + 5, "ReceiveAny did not return server-side packet");
    }

    if (inboundFromClient.Size() != std::strlen(payload)
        || std::memcmp(inboundFromClient.Data().data(), payload, inboundFromClient.Size()) != 0
        || inboundFromClient.Channel() != 1
        || inboundFromClient.Mode() != Solstice::Networking::Packet::SendMode::Reliable) {
        networking.CloseConnection(serverConnection);
        networking.CloseConnection(clientConnection);
        networking.CloseListenSocket(listenSocket);
        return Fail(failBase + 6, "Received payload metadata mismatch");
    }

    const char* response = "ack";
    Solstice::Networking::Packet serverPacket(
        response,
        std::strlen(response),
        2,
        Solstice::Networking::Packet::SendMode::Unreliable);
    serverPacket.SetSendOptions(
        Solstice::Networking::Packet::UnreliableFlag
        | Solstice::Networking::Packet::NoNagleFlag);
    if (!networking.Send(serverConnection, serverPacket)) {
        networking.CloseConnection(serverConnection);
        networking.CloseConnection(clientConnection);
        networking.CloseListenSocket(listenSocket);
        return Fail(failBase + 7, "Send server response failed");
    }

    Solstice::Networking::Packet clientInbound;
    const auto responseDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    bool gotResponse = false;
    while (std::chrono::steady_clock::now() < responseDeadline) {
        networking.Poll();
        if (networking.Receive(clientConnection, clientInbound)) {
            gotResponse = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (!gotResponse) {
        networking.CloseConnection(serverConnection);
        networking.CloseConnection(clientConnection);
        networking.CloseListenSocket(listenSocket);
        return Fail(failBase + 8, "Client did not receive response");
    }

    if (clientInbound.Size() != std::strlen(response)
        || std::memcmp(clientInbound.Data().data(), response, clientInbound.Size()) != 0
        || clientInbound.Channel() != 2
        || clientInbound.Mode() != Solstice::Networking::Packet::SendMode::Unreliable) {
        networking.CloseConnection(serverConnection);
        networking.CloseConnection(clientConnection);
        networking.CloseListenSocket(listenSocket);
        return Fail(failBase + 9, "Client response packet mismatch");
    }

    std::string remoteAddress;
    uint16_t remotePort = 0;
    if (!networking.GetRemoteAddress(clientConnection, remoteAddress, remotePort) || remoteAddress.empty() || remotePort == 0) {
        networking.CloseConnection(serverConnection);
        networking.CloseConnection(clientConnection);
        networking.CloseListenSocket(listenSocket);
        return Fail(failBase + 10, "GetRemoteAddress failed");
    }

    networking.CloseConnection(serverConnection);
    networking.CloseConnection(clientConnection);
    networking.CloseListenSocket(listenSocket);
    return 0;
}

} // namespace

int main() {
    if (!Solstice::Initialize()) {
        return Fail(1, "Solstice::Initialize failed");
    }

    auto& networking = Solstice::Networking::NetworkingSystem::Instance();
    if (!networking.Start()) {
        Solstice::Shutdown();
        return Fail(2, "NetworkingSystem::Start failed");
    }
    if (!networking.SetRelayPolicy(Solstice::Networking::Socket::RelayPolicy::PreferRelay)
        || !networking.SetRelayPolicy(Solstice::Networking::Socket::RelayPolicy::Default)) {
        networking.Stop();
        Solstice::Shutdown();
        return Fail(3, "Relay policy controls failed");
    }
    networking.EnableRelayNetworkAccess(true);

    int result = ExerciseLoopbackScenario(networking, 10, "127.0.0.1", "127.0.0.1", 37070, false);
    if (result != 0) {
        networking.Stop();
        Solstice::Shutdown();
        return result;
    }

    result = ExerciseLoopbackScenario(networking, 30, "::1", "::1", 37072, true);
    if (result != 0) {
        networking.Stop();
        Solstice::Shutdown();
        return result;
    }

    uint64_t callbackListenSocket = 0;
    if (!networking.ListenIPv4("127.0.0.1", 37074, callbackListenSocket) || callbackListenSocket == 0) {
        networking.Stop();
        Solstice::Shutdown();
        return Fail(40, "Callback listen failed");
    }
    uint64_t callbackClientConnection = 0;
    if (!networking.ConnectHost("localhost", 37074, Solstice::Networking::Socket::AddressFamily::IPv4, callbackClientConnection)
        || callbackClientConnection == 0
        || !WaitForConnected(networking, callbackClientConnection, std::chrono::seconds(3))) {
        networking.CloseListenSocket(callbackListenSocket);
        networking.Stop();
        Solstice::Shutdown();
        return Fail(41, "Callback connection failed");
    }
    const char* callbackSeed = "seed";
    Solstice::Networking::Packet callbackSeedPacket(callbackSeed, std::strlen(callbackSeed), 3, Solstice::Networking::Packet::SendMode::Reliable);
    if (!networking.Send(callbackClientConnection, callbackSeedPacket)) {
        networking.CloseConnection(callbackClientConnection);
        networking.CloseListenSocket(callbackListenSocket);
        networking.Stop();
        Solstice::Shutdown();
        return Fail(42, "Callback seed send failed");
    }
    uint64_t callbackServerConnection = 0;
    Solstice::Networking::Packet callbackInbound;
    for (int i = 0; i < 600 && callbackServerConnection == 0; ++i) {
        networking.Poll();
        if (networking.ReceiveAny(callbackServerConnection, callbackInbound)) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (callbackServerConnection == 0) {
        networking.CloseConnection(callbackClientConnection);
        networking.CloseListenSocket(callbackListenSocket);
        networking.Stop();
        Solstice::Shutdown();
        return Fail(43, "Callback server connection discovery failed");
    }

    int callbackCount = 0;
    int callbackChannel = -1;
    Solstice::Networking::Packet::SendMode callbackMode = Solstice::Networking::Packet::SendMode::Reliable;
    networking.SetReceiveCallback([&](uint64_t connection, const Solstice::Networking::Packet& packet) {
        if (connection == callbackClientConnection) {
            ++callbackCount;
            callbackChannel = packet.Channel();
            callbackMode = packet.Mode();
        }
    });

    const char* callbackPayload = "callback";
    Solstice::Networking::Packet callbackPacket(
        callbackPayload,
        std::strlen(callbackPayload),
        4,
        Solstice::Networking::Packet::SendMode::Unreliable);
    callbackPacket.SetSendOptions(
        Solstice::Networking::Packet::UnreliableFlag
        | Solstice::Networking::Packet::NoNagleFlag);
    if (!networking.Send(callbackServerConnection, callbackPacket)) {
        networking.ClearReceiveCallback();
        networking.CloseConnection(callbackServerConnection);
        networking.CloseConnection(callbackClientConnection);
        networking.CloseListenSocket(callbackListenSocket);
        networking.Stop();
        Solstice::Shutdown();
        return Fail(44, "Callback payload send failed");
    }
    for (int i = 0; i < 600 && callbackCount == 0; ++i) {
        networking.Poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    networking.ClearReceiveCallback();
    if (callbackCount == 0 || callbackChannel != 4 || callbackMode != Solstice::Networking::Packet::SendMode::Unreliable) {
        networking.CloseConnection(callbackServerConnection);
        networking.CloseConnection(callbackClientConnection);
        networking.CloseListenSocket(callbackListenSocket);
        networking.Stop();
        Solstice::Shutdown();
        return Fail(45, "Receive callback did not capture expected packet metadata");
    }
    networking.CloseConnection(callbackServerConnection);
    networking.CloseConnection(callbackClientConnection);
    networking.CloseListenSocket(callbackListenSocket);

    networking.Stop();
    Solstice::Shutdown();

    std::printf("[NetworkingTest] PASS\n");
    return 0;
}
