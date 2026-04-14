#include "TestHarness.hxx"
#include "Solstice.hxx"
#include "Networking/NetworkingSystem.hxx"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

namespace {

int Fail(int code, const char* message) {
    std::fprintf(stderr, "[NetworkingStressTest] FAIL (%d): %s\n", code, message);
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
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return false;
}

bool EnvFlag(const char* name) {
    const char* v = std::getenv(name);
    return v && v[0] != '\0' && std::strcmp(v, "0") != 0;
}

int EnvInt(const char* name, int defaultValue) {
    const char* e = std::getenv(name);
    if (!e || e[0] == '\0') {
        return defaultValue;
    }
    char* end = nullptr;
    const long v = std::strtol(e, &end, 10);
    if (end == e || v < 1) {
        return defaultValue;
    }
    return static_cast<int>(v);
}

bool OneLoopbackBurst(
    Solstice::Networking::NetworkingSystem& net,
    uint16_t basePort,
    int packetBurst) {
    uint64_t listenSocket = 0;
    if (!net.ListenIPv4("127.0.0.1", basePort, listenSocket) || listenSocket == 0) {
        return false;
    }
    uint64_t client = 0;
    if (!net.ConnectHost("127.0.0.1", basePort, Solstice::Networking::Socket::AddressFamily::IPv4, client)
        || client == 0) {
        net.CloseListenSocket(listenSocket);
        return false;
    }
    if (!WaitForConnected(net, client, std::chrono::seconds(4))) {
        net.CloseConnection(client);
        net.CloseListenSocket(listenSocket);
        return false;
    }

    for (int p = 0; p < packetBurst; ++p) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "burst-%d-%d", static_cast<int>(basePort), p);
        Solstice::Networking::Packet out(
            buf,
            std::strlen(buf),
            1 + (p % 7),
            Solstice::Networking::Packet::SendMode::Reliable);
        out.SetSendOptions(Solstice::Networking::Packet::ReliableFlag);
        if (!net.Send(client, out)) {
            net.CloseConnection(client);
            net.CloseListenSocket(listenSocket);
            return false;
        }
        net.Flush(client);
        net.Poll();
    }

    uint64_t serverConn = 0;
    int received = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
    while (std::chrono::steady_clock::now() < deadline && received < packetBurst) {
        net.Poll();
        Solstice::Networking::Packet in;
        while (net.ReceiveAny(serverConn, in)) {
            if (serverConn != 0 && !in.Empty()) {
                ++received;
            }
            if (received >= packetBurst) {
                break;
            }
        }
        if (received >= packetBurst) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    net.CloseConnection(client);
    if (serverConn != 0) {
        net.CloseConnection(serverConn);
    }
    net.CloseListenSocket(listenSocket);
    return received == packetBurst;
}

bool RunNetworkingStress() {
    if (!Solstice::Initialize()) {
        SOLSTICE_TEST_FAIL_MSG("Solstice::Initialize");
        return false;
    }
    auto& net = Solstice::Networking::NetworkingSystem::Instance();
    if (!net.Start()) {
        Solstice::Shutdown();
        SOLSTICE_TEST_FAIL_MSG("NetworkingSystem::Start");
        return false;
    }

    const bool torture = EnvFlag("SOLSTICE_STRESS_TORTURE");
    const int cycles = EnvInt("SOLSTICE_NET_STRESS_CYCLES", torture ? 24 : 8);
    const int burst = EnvInt("SOLSTICE_NET_STRESS_PACKETS", torture ? 200 : 40);
    const uint16_t portBase = 38000;

    for (int c = 0; c < cycles; ++c) {
        const uint16_t port = static_cast<uint16_t>(portBase + c);
        SOLSTICE_TEST_ASSERT(OneLoopbackBurst(net, port, burst), "loopback burst cycle");
    }

    net.Stop();
    Solstice::Shutdown();
    SOLSTICE_TEST_PASS("Networking stress");
    return true;
}

} // namespace

int main() {
    if (!RunNetworkingStress()) {
        return 1;
    }
    return SolsticeTestMainResult("NetworkingStressTest");
}
