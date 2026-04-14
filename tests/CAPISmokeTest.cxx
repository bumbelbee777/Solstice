/**
 * Solstice V1 C API stress-smoke test.
 * Verifies link-time/runtime export availability and repeatedly exercises all
 * exposed V1 modules with lightweight validation.
 */

#include "SolsticeAPI/V1/SolsticeAPI.h"
#include <Smf/SmfBinary.hxx>
#include <Smf/SmfMap.hxx>
#include <Smf/SmfTypes.hxx>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

static int g_PluginBusHits = 0;

extern "C" void SolsticeSmoke_PluginBusCb(
    uint32_t SenderPluginId,
    const char* ChannelUtf8,
    const char* MimeTypeUtf8,
    const uint8_t* Payload,
    uint32_t PayloadSize,
    void* UserData) {
    (void)SenderPluginId;
    (void)UserData;
    (void)MimeTypeUtf8;
    (void)Payload;
    (void)PayloadSize;
    if (ChannelUtf8 && std::strcmp(ChannelUtf8, "capismoke.plugin.bus") == 0) {
        g_PluginBusHits++;
    }
}

namespace {

constexpr int kStressIterations = 24;
constexpr int kMotionSamples = 64;

int Fail(int code, const char* message) {
    std::fprintf(stderr, "[CAPISmokeTest] FAIL (%d): %s\n", code, message);
    return code;
}

int FailWithDetail(int code, const char* message, const char* detail) {
    if (detail && detail[0] != '\0') {
        std::fprintf(stderr, "[CAPISmokeTest] FAIL (%d): %s | detail: %s\n", code, message, detail);
    } else {
        std::fprintf(stderr, "[CAPISmokeTest] FAIL (%d): %s\n", code, message);
    }
    return code;
}

void LogStage(const char* stage) {
    std::printf("[CAPISmokeTest] Stage: %s\n", stage);
    std::fflush(stdout);
}

bool IsFiniteNonNegative(float value) {
    return value >= 0.0f && value < 1.0e20f;
}

int TestCoreModule() {
    if (SolsticeV1_CoreInitialize() != SolsticeV1_True) {
        return Fail(101, "SolsticeV1_CoreInitialize failed");
    }
    if (SolsticeV1_CoreReinitialize() != SolsticeV1_True) {
        return Fail(102, "SolsticeV1_CoreReinitialize failed");
    }

    const char* version = nullptr;
    SolsticeV1_CoreGetVersionString(&version);
    if (!version || version[0] == '\0') {
        return Fail(103, "SolsticeV1_CoreGetVersionString returned empty string");
    }

    const char* commit = nullptr;
    SolsticeV1_CoreGetBuildCommit(&commit);
    if (!commit || commit[0] == '\0') {
        return Fail(104, "SolsticeV1_CoreGetBuildCommit returned empty string");
    }

    std::printf("[CAPISmokeTest] Core version: %s\n", version);
    std::printf("[CAPISmokeTest] Core commit : %s\n", commit);
    return 0;
}

int TestPhysicsModule() {
    SolsticeV1_PhysicsWorldHandle world = nullptr;
    if (SolsticeV1_PhysicsStart(&world) != SolsticeV1_ResultSuccess) {
        return Fail(120, "SolsticeV1_PhysicsStart failed");
    }
    if (!world) {
        return Fail(121, "SolsticeV1_PhysicsStart returned null world handle");
    }

    // Also validate idempotent start behavior.
    SolsticeV1_PhysicsWorldHandle world2 = nullptr;
    if (SolsticeV1_PhysicsStart(&world2) != SolsticeV1_ResultSuccess) {
        return Fail(122, "Second SolsticeV1_PhysicsStart failed");
    }
    if (!world2) {
        return Fail(123, "Second SolsticeV1_PhysicsStart returned null world handle");
    }

    for (int i = 0; i < kStressIterations; ++i) {
        SolsticeV1_PhysicsUpdate(1.0f / 240.0f);
    }
    return 0;
}

int TestNetworkingModule() {
    if (SolsticeV1_NetworkingStart() != SolsticeV1_ResultSuccess) {
        return Fail(130, "SolsticeV1_NetworkingStart failed");
    }
    SolsticeV1_Bool isRunning = SolsticeV1_False;
    if (SolsticeV1_NetworkingIsRunning(&isRunning) != SolsticeV1_ResultSuccess || isRunning != SolsticeV1_True) {
        SolsticeV1_NetworkingStop();
        return Fail(131, "SolsticeV1_NetworkingIsRunning failed");
    }
    uint32_t listenCount = 0;
    if (SolsticeV1_NetworkingGetListenSocketCount(&listenCount) != SolsticeV1_ResultSuccess || listenCount != 0) {
        SolsticeV1_NetworkingStop();
        return Fail(132, "Initial listen socket count invalid");
    }
    SolsticeV1_NetworkingRelayPolicy relayPolicy = SolsticeV1_NetworkingRelayPolicyDefault;
    if (SolsticeV1_NetworkingGetRelayPolicy(&relayPolicy) != SolsticeV1_ResultSuccess
        || relayPolicy != SolsticeV1_NetworkingRelayPolicyDefault) {
        SolsticeV1_NetworkingStop();
        return Fail(132, "Initial relay policy invalid");
    }
    if (SolsticeV1_NetworkingSetRelayPolicy(SolsticeV1_NetworkingRelayPolicyPreferRelay) != SolsticeV1_ResultSuccess
        || SolsticeV1_NetworkingSetRelayPolicy(SolsticeV1_NetworkingRelayPolicyDefault) != SolsticeV1_ResultSuccess
        || SolsticeV1_NetworkingEnableRelayNetworkAccess(SolsticeV1_True) != SolsticeV1_ResultSuccess) {
        SolsticeV1_NetworkingStop();
        return Fail(132, "Relay policy configuration failed");
    }

    SolsticeV1_ConnectionHandle rejectedConnection = 0;
    if (SolsticeV1_NetworkingConnectHost("::1", 37071, SolsticeV1_NetworkingAddressFamilyIPv4, &rejectedConnection) == SolsticeV1_ResultSuccess) {
        SolsticeV1_NetworkingStop();
        return Fail(133, "ConnectHost unexpectedly accepted IPv6 host for IPv4 preference");
    }
    if (SolsticeV1_NetworkingConnectHost("127.0.0.1", 37071, SolsticeV1_NetworkingAddressFamilyIPv6, &rejectedConnection) == SolsticeV1_ResultSuccess) {
        SolsticeV1_NetworkingStop();
        return Fail(134, "ConnectHost unexpectedly accepted IPv4 host for IPv6 preference");
    }

    using ListenFn = SolsticeV1_ResultCode (*)(const char*, uint16_t, SolsticeV1_ListenSocketHandle*);
    auto runScenario = [&](int failBase, const char* listenAddress, const char* connectHost, uint16_t port, ListenFn listenFn, SolsticeV1_NetworkingAddressFamily family) -> int {
        SolsticeV1_ListenSocketHandle listenSocket = 0;
        if (listenFn(listenAddress, port, &listenSocket) != SolsticeV1_ResultSuccess || listenSocket == 0) {
            return Fail(failBase + 1, "Listen failed");
        }
        if (SolsticeV1_NetworkingGetListenSocketCount(&listenCount) != SolsticeV1_ResultSuccess || listenCount == 0) {
            SolsticeV1_NetworkingCloseListenSocket(listenSocket);
            return Fail(failBase + 2, "Listen socket count did not increment");
        }

        SolsticeV1_ConnectionHandle clientConnection = 0;
        if (SolsticeV1_NetworkingConnectHost(connectHost, port, family, &clientConnection) != SolsticeV1_ResultSuccess || clientConnection == 0) {
            SolsticeV1_NetworkingCloseListenSocket(listenSocket);
            return Fail(failBase + 3, "Connect failed");
        }

        bool connected = false;
        for (int i = 0; i < 600; ++i) {
            SolsticeV1_NetworkingPoll();
            SolsticeV1_NetworkingConnectionState state = SolsticeV1_NetworkingConnectionStateNone;
            SolsticeV1_NetworkingGetConnectionState(clientConnection, &state);
            if (state == SolsticeV1_NetworkingConnectionStateConnected) {
                connected = true;
                break;
            }
            if (state == SolsticeV1_NetworkingConnectionStateProblemDetectedLocally
                || state == SolsticeV1_NetworkingConnectionStateClosedByPeer
                || state == SolsticeV1_NetworkingConnectionStateDead) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        if (!connected) {
            SolsticeV1_NetworkingCloseConnection(clientConnection);
            SolsticeV1_NetworkingCloseListenSocket(listenSocket);
            return Fail(failBase + 4, "Connection did not reach Connected");
        }

        uint32_t activeConnections = 0;
        if (SolsticeV1_NetworkingGetActiveConnectionCount(&activeConnections) != SolsticeV1_ResultSuccess || activeConnections == 0) {
            SolsticeV1_NetworkingCloseConnection(clientConnection);
            SolsticeV1_NetworkingCloseListenSocket(listenSocket);
            return Fail(failBase + 5, "Active connection count did not increment");
        }

        const char* payload = "capi-networking-ping";
        if (SolsticeV1_NetworkingSendEx(
                clientConnection,
                payload,
                std::strlen(payload),
                SolsticeV1_NetworkingSendFlagReliable | SolsticeV1_NetworkingSendFlagNoDelay,
                3) != SolsticeV1_ResultSuccess) {
            SolsticeV1_NetworkingCloseConnection(clientConnection);
            SolsticeV1_NetworkingCloseListenSocket(listenSocket);
            return Fail(failBase + 6, "SolsticeV1_NetworkingSend failed");
        }
        if (SolsticeV1_NetworkingFlush(clientConnection) != SolsticeV1_ResultSuccess) {
            SolsticeV1_NetworkingCloseConnection(clientConnection);
            SolsticeV1_NetworkingCloseListenSocket(listenSocket);
            return Fail(failBase + 7, "SolsticeV1_NetworkingFlush failed");
        }

        SolsticeV1_ConnectionHandle serverConnection = 0;
        char receiveBuffer[256] = {};
        size_t receivedBytes = 0;
        int32_t receivedChannel = -1;
        SolsticeV1_Bool receivedReliable = SolsticeV1_False;
        bool gotServerReceive = false;
        for (int i = 0; i < 600; ++i) {
            SolsticeV1_NetworkingPoll();
            if (SolsticeV1_NetworkingReceiveAny(
                    &serverConnection,
                    receiveBuffer,
                    sizeof(receiveBuffer),
                    &receivedBytes,
                    &receivedChannel,
                    &receivedReliable) == SolsticeV1_ResultSuccess) {
                gotServerReceive = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        if (!gotServerReceive || serverConnection == 0) {
            SolsticeV1_NetworkingCloseConnection(clientConnection);
            SolsticeV1_NetworkingCloseListenSocket(listenSocket);
            return Fail(failBase + 8, "SolsticeV1_NetworkingReceiveAny did not return message");
        }
        if (receivedBytes != std::strlen(payload)
            || std::memcmp(receiveBuffer, payload, receivedBytes) != 0
            || receivedChannel != 3
            || receivedReliable != SolsticeV1_True) {
            SolsticeV1_NetworkingCloseConnection(serverConnection);
            SolsticeV1_NetworkingCloseConnection(clientConnection);
            SolsticeV1_NetworkingCloseListenSocket(listenSocket);
            return Fail(failBase + 9, "ReceiveAny payload/channel/reliable mismatch");
        }

        char remoteAddress[128] = {};
        uint16_t remotePort = 0;
        if (SolsticeV1_NetworkingGetRemoteAddress(clientConnection, remoteAddress, sizeof(remoteAddress), &remotePort) != SolsticeV1_ResultSuccess
            || remoteAddress[0] == '\0'
            || remotePort == 0) {
            SolsticeV1_NetworkingCloseConnection(serverConnection);
            SolsticeV1_NetworkingCloseConnection(clientConnection);
            SolsticeV1_NetworkingCloseListenSocket(listenSocket);
            return Fail(failBase + 10, "SolsticeV1_NetworkingGetRemoteAddress failed");
        }

        const char* ack = "capi-networking-ack";
        if (SolsticeV1_NetworkingSendEx(
                serverConnection,
                ack,
                std::strlen(ack),
                SolsticeV1_NetworkingSendFlagUnreliable | SolsticeV1_NetworkingSendFlagNoNagle,
                5) != SolsticeV1_ResultSuccess) {
            SolsticeV1_NetworkingCloseConnection(serverConnection);
            SolsticeV1_NetworkingCloseConnection(clientConnection);
            SolsticeV1_NetworkingCloseListenSocket(listenSocket);
            return Fail(failBase + 11, "Server response send failed");
        }

        bool gotClientReceive = false;
        for (int i = 0; i < 600; ++i) {
            SolsticeV1_NetworkingPoll();
            if (SolsticeV1_NetworkingReceive(
                    clientConnection,
                    receiveBuffer,
                    sizeof(receiveBuffer),
                    &receivedBytes,
                    &receivedChannel,
                    &receivedReliable) == SolsticeV1_ResultSuccess) {
                gotClientReceive = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        if (!gotClientReceive) {
            SolsticeV1_NetworkingCloseConnection(serverConnection);
            SolsticeV1_NetworkingCloseConnection(clientConnection);
            SolsticeV1_NetworkingCloseListenSocket(listenSocket);
            return Fail(failBase + 12, "Client did not receive server response");
        }
        if (receivedBytes != std::strlen(ack)
            || std::memcmp(receiveBuffer, ack, receivedBytes) != 0
            || receivedChannel != 5
            || receivedReliable != SolsticeV1_False) {
            SolsticeV1_NetworkingCloseConnection(serverConnection);
            SolsticeV1_NetworkingCloseConnection(clientConnection);
            SolsticeV1_NetworkingCloseListenSocket(listenSocket);
            return Fail(failBase + 13, "Client response payload/channel/reliable mismatch");
        }

        SolsticeV1_NetworkingCloseConnection(serverConnection);
        SolsticeV1_NetworkingCloseConnection(clientConnection);
        SolsticeV1_NetworkingCloseListenSocket(listenSocket);
        if (SolsticeV1_NetworkingGetActiveConnectionCount(&activeConnections) != SolsticeV1_ResultSuccess || activeConnections != 0) {
            return Fail(failBase + 14, "Active connection count did not drop to zero");
        }
        return 0;
    };

    int code = runScenario(140, "127.0.0.1", "127.0.0.1", 37071, SolsticeV1_NetworkingListenIPv4, SolsticeV1_NetworkingAddressFamilyIPv4);
    if (code != 0) {
        SolsticeV1_NetworkingStop();
        return code;
    }

    code = runScenario(170, "::1", "::1", 37073, SolsticeV1_NetworkingListenIPv6, SolsticeV1_NetworkingAddressFamilyIPv6);
    if (code != 0) {
        SolsticeV1_NetworkingStop();
        return code;
    }

    SolsticeV1_ListenSocketHandle dnsListenSocket = 0;
    if (SolsticeV1_NetworkingListenIPv4("127.0.0.1", 37075, &dnsListenSocket) != SolsticeV1_ResultSuccess) {
        SolsticeV1_NetworkingStop();
        return Fail(190, "DNS listen setup failed");
    }
    SolsticeV1_ConnectionHandle dnsConnection = 0;
    if (SolsticeV1_NetworkingConnectHost("localhost", 37075, SolsticeV1_NetworkingAddressFamilyIPv4, &dnsConnection) != SolsticeV1_ResultSuccess
        || dnsConnection == 0) {
        SolsticeV1_NetworkingCloseListenSocket(dnsListenSocket);
        SolsticeV1_NetworkingStop();
        return Fail(191, "SolsticeV1_NetworkingConnectHost failed");
    }
    bool dnsConnected = false;
    for (int i = 0; i < 600; ++i) {
        SolsticeV1_NetworkingPoll();
        SolsticeV1_NetworkingConnectionState state = SolsticeV1_NetworkingConnectionStateNone;
        SolsticeV1_NetworkingGetConnectionState(dnsConnection, &state);
        if (state == SolsticeV1_NetworkingConnectionStateConnected) {
            dnsConnected = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (!dnsConnected) {
        SolsticeV1_NetworkingCloseConnection(dnsConnection);
        SolsticeV1_NetworkingCloseListenSocket(dnsListenSocket);
        SolsticeV1_NetworkingStop();
        return Fail(192, "DNS-based connection did not reach Connected");
    }

    SolsticeV1_ConnectionHandle dnsServerConnection = 0;
    const char* dnsPayload = "dns-ping";
    if (SolsticeV1_NetworkingSendEx(
            dnsConnection,
            dnsPayload,
            std::strlen(dnsPayload),
            SolsticeV1_NetworkingSendFlagReliable,
            7) != SolsticeV1_ResultSuccess) {
        SolsticeV1_NetworkingCloseConnection(dnsConnection);
        SolsticeV1_NetworkingCloseListenSocket(dnsListenSocket);
        SolsticeV1_NetworkingStop();
        return Fail(193, "DNS scenario send failed");
    }
    char dnsBuffer[64] = {};
    size_t dnsBytes = 0;
    int32_t dnsChannel = 0;
    SolsticeV1_Bool dnsReliable = SolsticeV1_False;
    bool dnsServerGotMsg = false;
    for (int i = 0; i < 600; ++i) {
        SolsticeV1_NetworkingPoll();
        if (SolsticeV1_NetworkingReceiveAny(&dnsServerConnection, dnsBuffer, sizeof(dnsBuffer), &dnsBytes, &dnsChannel, &dnsReliable) == SolsticeV1_ResultSuccess) {
            dnsServerGotMsg = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (!dnsServerGotMsg || dnsServerConnection == 0) {
        SolsticeV1_NetworkingCloseConnection(dnsConnection);
        SolsticeV1_NetworkingCloseListenSocket(dnsListenSocket);
        SolsticeV1_NetworkingStop();
        return Fail(194, "DNS scenario server receive failed");
    }

    struct CallbackState {
        int count{0};
        int32_t lastChannel{-1};
        SolsticeV1_Bool lastReliable{SolsticeV1_False};
        size_t lastSize{0};
    } callbackState;
    auto callbackFn = [](SolsticeV1_ConnectionHandle, const void*, size_t size, int32_t channel, SolsticeV1_Bool reliable, void* userData) {
        auto* state = static_cast<CallbackState*>(userData);
        if (!state) {
            return;
        }
        ++state->count;
        state->lastChannel = channel;
        state->lastReliable = reliable;
        state->lastSize = size;
    };
    if (SolsticeV1_NetworkingSetReceiveCallback(callbackFn, &callbackState) != SolsticeV1_ResultSuccess) {
        SolsticeV1_NetworkingCloseConnection(dnsServerConnection);
        SolsticeV1_NetworkingCloseConnection(dnsConnection);
        SolsticeV1_NetworkingCloseListenSocket(dnsListenSocket);
        SolsticeV1_NetworkingStop();
        return Fail(195, "Setting receive callback failed");
    }

    const char* callbackPayload = "callback-ping";
    if (SolsticeV1_NetworkingSendEx(
            dnsServerConnection,
            callbackPayload,
            std::strlen(callbackPayload),
            SolsticeV1_NetworkingSendFlagUnreliable | SolsticeV1_NetworkingSendFlagNoNagle,
            9) != SolsticeV1_ResultSuccess) {
        SolsticeV1_NetworkingSetReceiveCallback(nullptr, nullptr);
        SolsticeV1_NetworkingCloseConnection(dnsServerConnection);
        SolsticeV1_NetworkingCloseConnection(dnsConnection);
        SolsticeV1_NetworkingCloseListenSocket(dnsListenSocket);
        SolsticeV1_NetworkingStop();
        return Fail(196, "Callback scenario send failed");
    }
    for (int i = 0; i < 600 && callbackState.count == 0; ++i) {
        SolsticeV1_NetworkingPoll();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (callbackState.count == 0
        || callbackState.lastChannel != 9
        || callbackState.lastReliable != SolsticeV1_False
        || callbackState.lastSize != std::strlen(callbackPayload)) {
        SolsticeV1_NetworkingSetReceiveCallback(nullptr, nullptr);
        SolsticeV1_NetworkingCloseConnection(dnsServerConnection);
        SolsticeV1_NetworkingCloseConnection(dnsConnection);
        SolsticeV1_NetworkingCloseListenSocket(dnsListenSocket);
        SolsticeV1_NetworkingStop();
        return Fail(197, "Receive callback did not receive expected payload metadata");
    }

    SolsticeV1_NetworkingSetReceiveCallback(nullptr, nullptr);
    SolsticeV1_NetworkingCloseConnection(dnsServerConnection);
    SolsticeV1_NetworkingCloseConnection(dnsConnection);
    SolsticeV1_NetworkingCloseListenSocket(dnsListenSocket);

    SolsticeV1_NetworkingStop();
    return 0;
}

int TestAudioModule() {
    if (SolsticeV1_AudioSetMasterVolume(0.5f) != SolsticeV1_ResultSuccess) {
        return Fail(140, "SolsticeV1_AudioSetMasterVolume(0.5) failed");
    }
    if (SolsticeV1_AudioSetMasterVolume(0.0f) != SolsticeV1_ResultSuccess) {
        return Fail(141, "SolsticeV1_AudioSetMasterVolume(0.0) failed");
    }
    if (SolsticeV1_AudioSetMasterVolume(1.0f) != SolsticeV1_ResultSuccess) {
        return Fail(142, "SolsticeV1_AudioSetMasterVolume(1.0) failed");
    }

    for (int i = 0; i < kStressIterations; ++i) {
        if (SolsticeV1_AudioUpdate(1.0f / 120.0f) != SolsticeV1_ResultSuccess) {
            return Fail(143, "SolsticeV1_AudioUpdate failed");
        }
    }

    if (SolsticeV1_AudioSetListener(
            0.0f, 1.8f, 0.0f,
            0.0f, 0.0f, -1.0f,
            0.0f, 1.0f, 0.0f) != SolsticeV1_ResultSuccess) {
        return Fail(144, "SolsticeV1_AudioSetListener failed");
    }
    if (SolsticeV1_AudioSetReverbPreset(1) != SolsticeV1_ResultSuccess) {
        return Fail(145, "SolsticeV1_AudioSetReverbPreset failed");
    }
    SolsticeV1_AudioEmitterHandle emitter = 0;
    if (SolsticeV1_AudioCreateEmitter(nullptr, 0.0f, 0.0f, 0.0f, 20.0f, SolsticeV1_False, &emitter) != SolsticeV1_ResultFailure) {
        return Fail(146, "SolsticeV1_AudioCreateEmitter should fail for null path");
    }
    if (SolsticeV1_AudioSetEmitterVolume(9999999, 0.5f) != SolsticeV1_ResultFailure) {
        return Fail(147, "SolsticeV1_AudioSetEmitterVolume should fail for invalid handle");
    }
    if (SolsticeV1_AudioSetEmitterOcclusion(9999999, 0.5f) != SolsticeV1_ResultFailure) {
        return Fail(148, "SolsticeV1_AudioSetEmitterOcclusion should fail for invalid handle");
    }
    if (SolsticeV1_AudioDestroyEmitter(9999999) != SolsticeV1_ResultFailure) {
        return Fail(149, "SolsticeV1_AudioDestroyEmitter should fail for invalid handle");
    }

    // Stop is expected to be safe even without active music.
    if (SolsticeV1_AudioStopMusic() != SolsticeV1_ResultSuccess) {
        return Fail(150, "SolsticeV1_AudioStopMusic failed");
    }
    return 0;
}

int TestVideoModule() {
    SolsticeV1_VideoDecoder dec = SolsticeV1_VideoDecoderInvalid;
    if (SolsticeV1_VideoDecoderOpen(nullptr, &dec) != SolsticeV1_ResultFailure) {
        return Fail(151, "SolsticeV1_VideoDecoderOpen should fail for null path");
    }
    const SolsticeV1_ResultCode missRc = SolsticeV1_VideoDecoderOpen("nope/solstice_missing_video_file.bin", &dec);
    if (missRc != SolsticeV1_ResultFailure && missRc != SolsticeV1_ResultNotSupported) {
        return Fail(152, "SolsticeV1_VideoDecoderOpen should fail or be not supported for missing file");
    }
    if (dec != SolsticeV1_VideoDecoderInvalid) {
        SolsticeV1_VideoDecoderClose(dec);
        dec = SolsticeV1_VideoDecoderInvalid;
    }
    if (SolsticeV1_VideoDecoderOpen("any", nullptr) != SolsticeV1_ResultFailure) {
        return Fail(153, "SolsticeV1_VideoDecoderOpen should fail for null OutDecoder");
    }
    if (SolsticeV1_VideoDecoderClose(SolsticeV1_VideoDecoderInvalid) != SolsticeV1_ResultSuccess) {
        return Fail(154, "SolsticeV1_VideoDecoderClose invalid handle");
    }
    uint32_t w = 0;
    uint32_t h = 0;
    if (SolsticeV1_VideoDecoderGetDimensions(SolsticeV1_VideoDecoderInvalid, &w, &h) != SolsticeV1_ResultFailure
        && SolsticeV1_VideoDecoderGetDimensions(SolsticeV1_VideoDecoderInvalid, &w, &h) != SolsticeV1_ResultNotSupported) {
        return Fail(155, "SolsticeV1_VideoDecoderGetDimensions invalid decoder");
    }
    return 0;
}

int TestScriptingModule() {
    char compileError[256] = {};
    const char* script = R"(
        @Entry {
            let x = 21;
            let y = 21;
            print("sum", x + y);
        }
    )";
    if (SolsticeV1_ScriptingCompile(script, compileError, sizeof(compileError)) != SolsticeV1_ResultSuccess) {
        return FailWithDetail(160, "SolsticeV1_ScriptingCompile failed", compileError);
    }

    char output[256] = {};
    char execError[256] = {};
    if (SolsticeV1_ScriptingExecute(script, output, sizeof(output), execError, sizeof(execError))
        != SolsticeV1_ResultSuccess) {
        return FailWithDetail(161, "SolsticeV1_ScriptingExecute failed", execError);
    }

    // Stress compile path without repeatedly triggering JIT backend startup logs.
    for (int i = 0; i < 8; ++i) {
        const char* stressCompileScript = R"(
            @Entry {
                let i = 0;
                let acc = 0;
                while (i < 8) {
                    acc = acc + i;
                    i = i + 1;
                }
                print("acc", acc);
            }
        )";
        std::memset(compileError, 0, sizeof(compileError));
        if (SolsticeV1_ScriptingCompile(stressCompileScript, compileError, sizeof(compileError))
            != SolsticeV1_ResultSuccess) {
            return FailWithDetail(162, "Repeated SolsticeV1_ScriptingCompile failed", compileError);
        }
    }
    return 0;
}

int TestNarrativeCutsceneSmfModule() {
    char err[512] = {};

    const char* narrativeOk = R"({"format":"solstice.narrative.v1","startNodeId":"a","nodes":[{"nodeId":"a","text":"x","nextNodeId":"","choices":[]}]})";
    if (SolsticeV1_NarrativeValidateJSON(narrativeOk, err, sizeof(err)) != SolsticeV1_ResultSuccess) {
        return FailWithDetail(170, "SolsticeV1_NarrativeValidateJSON (good doc) failed", err);
    }

    if (SolsticeV1_NarrativeValidateJSON("{", err, sizeof(err)) != SolsticeV1_ResultFailure) {
        return Fail(171, "SolsticeV1_NarrativeValidateJSON should fail on bad JSON");
    }

    const char* cutsceneOk = R"({"id":"t","events":[]})";
    if (SolsticeV1_CutsceneValidateJSON(cutsceneOk, err, sizeof(err)) != SolsticeV1_ResultSuccess) {
        return FailWithDetail(172, "SolsticeV1_CutsceneValidateJSON (good doc) failed", err);
    }
    if (SolsticeV1_CutsceneValidateJSON("[]", err, sizeof(err)) != SolsticeV1_ResultFailure) {
        return Fail(173, "SolsticeV1_CutsceneValidateJSON should fail when root is not an object");
    }

    Solstice::Smf::SmfMap map;
    std::vector<std::byte> smfBytes;
    Solstice::Smf::SmfError smfErr = Solstice::Smf::SmfError::None;
    if (!Solstice::Smf::SaveSmfToBytes(map, smfBytes, &smfErr) || smfErr != Solstice::Smf::SmfError::None) {
        return Fail(174, "SaveSmfToBytes for empty map failed");
    }
    if (SolsticeV1_SmfValidateBinary(smfBytes.data(), smfBytes.size(), err, sizeof(err)) != SolsticeV1_ResultSuccess) {
        return FailWithDetail(175, "SolsticeV1_SmfValidateBinary (good blob) failed", err);
    }
    const std::byte badSmf[4] = {std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}};
    if (SolsticeV1_SmfValidateBinary(badSmf, sizeof(badSmf), err, sizeof(err)) != SolsticeV1_ResultFailure) {
        return Fail(176, "SolsticeV1_SmfValidateBinary should fail on garbage");
    }
    if (SolsticeV1_SmfValidateBinary(nullptr, 0, err, sizeof(err)) != SolsticeV1_ResultFailure) {
        return Fail(177, "SolsticeV1_SmfValidateBinary should fail on null/zero size");
    }
    return 0;
}

int TestFluidModule() {
    SolsticeV1_FluidHandle fluid = nullptr;
    if (SolsticeV1_FluidCreate(
            16, 16, 16,
            1.0f / 16.0f, 1.0f / 16.0f, 1.0f / 16.0f,
            1.0e-4f, 1.0e-4f,
            &fluid) != SolsticeV1_ResultSuccess) {
        return Fail(180, "SolsticeV1_FluidCreate failed");
    }
    if (!fluid) {
        return Fail(181, "SolsticeV1_FluidCreate returned null handle");
    }

    if (SolsticeV1_FluidSetPressureMultigrid(fluid, SolsticeV1_True, 4, 2, 2, 18, 48) != SolsticeV1_ResultSuccess) {
        SolsticeV1_FluidDestroy(fluid);
        return Fail(182, "SolsticeV1_FluidSetPressureMultigrid failed");
    }

    for (int i = 0; i < kStressIterations; ++i) {
        const int x = 2 + (i % 12);
        const int y = 2 + ((i * 3) % 12);
        const int z = 2 + ((i * 5) % 12);
        if (SolsticeV1_FluidAddDensity(fluid, x, y, z, 0.25f + 0.01f * static_cast<float>(i)) != SolsticeV1_ResultSuccess) {
            SolsticeV1_FluidDestroy(fluid);
            return Fail(183, "SolsticeV1_FluidAddDensity failed");
        }
        if (SolsticeV1_FluidAddVelocity(fluid, x, y, z, 0.1f, 0.05f, -0.08f) != SolsticeV1_ResultSuccess) {
            SolsticeV1_FluidDestroy(fluid);
            return Fail(184, "SolsticeV1_FluidAddVelocity failed");
        }
        if (SolsticeV1_FluidStep(fluid, 1.0f / 120.0f) != SolsticeV1_ResultSuccess) {
            SolsticeV1_FluidDestroy(fluid);
            return Fail(185, "SolsticeV1_FluidStep failed");
        }
    }

    float meanDiv = 0.0f;
    float maxDiv = 0.0f;
    if (SolsticeV1_FluidGetDivergenceMetrics(fluid, &meanDiv, &maxDiv) != SolsticeV1_ResultSuccess) {
        SolsticeV1_FluidDestroy(fluid);
        return Fail(186, "SolsticeV1_FluidGetDivergenceMetrics failed");
    }
    if (!IsFiniteNonNegative(meanDiv) || !IsFiniteNonNegative(maxDiv)) {
        SolsticeV1_FluidDestroy(fluid);
        return Fail(187, "Fluid divergence metrics are invalid");
    }

    SolsticeV1_FluidDestroy(fluid);
    return 0;
}

int TestProfilerModule() {
    const char* strictProfilerEnv = std::getenv("CAPISMOKE_STRICT_PROFILER");
    const bool strictProfiler = (strictProfilerEnv && strictProfilerEnv[0] != '\0' && strictProfilerEnv[0] != '0');
    if (!strictProfiler) {
        std::printf("[CAPISmokeTest] Profiler stage skipped (covered by ProfilerTest; set CAPISMOKE_STRICT_PROFILER=1 to force).\n");
        return 0;
    }

    if (SolsticeV1_ProfilerSetEnabled(SolsticeV1_True) != SolsticeV1_ResultSuccess) {
        return Fail(200, "SolsticeV1_ProfilerSetEnabled(true) failed");
    }

    // Use a conservative API subset that is stable across backend/runtime states.
    constexpr int kProfilerIterations = 8;
    for (int i = 0; i < kProfilerIterations; ++i) {
        if (SolsticeV1_ProfilerSetCounter("CAPISmokeCounter", i) != SolsticeV1_ResultSuccess) {
            return Fail(203, "SolsticeV1_ProfilerSetCounter failed");
        }
    }
    int64_t counterValue = -1;
    if (SolsticeV1_ProfilerGetCounter("CAPISmokeCounter", &counterValue) != SolsticeV1_ResultSuccess) {
        return Fail(206, "SolsticeV1_ProfilerGetCounter failed");
    }
    if (counterValue != (kProfilerIterations - 1)) {
        return Fail(207, "Profiler counter mismatch after stress loop");
    }
    if (SolsticeV1_ProfilerSetEnabled(SolsticeV1_False) != SolsticeV1_ResultSuccess) {
        return Fail(208, "SolsticeV1_ProfilerSetEnabled(false) failed");
    }
    return 0;
}

int TestMotionGraphicsModule() {
    for (int i = 0; i < kMotionSamples; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kMotionSamples - 1);
        for (int ease = SolsticeV1_MotionEaseLinear; ease <= SolsticeV1_MotionEaseBezierCurve; ++ease) {
            float out = -1.0f;
            if (SolsticeV1_MotionEase(t, static_cast<SolsticeV1_MotionEasingType>(ease), 1.0f, &out)
                != SolsticeV1_ResultSuccess) {
                return Fail(230, "SolsticeV1_MotionEase failed");
            }
            if (!(out > -1000.0f && out < 1000.0f)) {
                return Fail(231, "SolsticeV1_MotionEase returned out-of-range value");
            }
        }
    }

    for (int i = 0; i < kMotionSamples; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kMotionSamples - 1);
        float out = -1.0f;
        if (SolsticeV1_MotionEaseBezier(t, 0.2f, 0.0f, 0.8f, 1.0f, &out) != SolsticeV1_ResultSuccess) {
            return Fail(232, "SolsticeV1_MotionEaseBezier failed");
        }
        if (!(out >= -0.1f && out <= 1.1f)) {
            return Fail(233, "SolsticeV1_MotionEaseBezier returned out-of-range value");
        }
    }

    float ping = -1.0f;
    if (SolsticeV1_MotionPingPongNormalized(3.1f, 1.0f, &ping) != SolsticeV1_ResultSuccess) {
        return Fail(234, "SolsticeV1_MotionPingPongNormalized failed");
    }
    if (!(ping >= -0.01f && ping <= 1.01f)) {
        return Fail(235, "SolsticeV1_MotionPingPongNormalized out of range");
    }
    float vel = 0.0f;
    float damped = 0.0f;
    if (SolsticeV1_MotionSmoothDampFloat(0.0f, 10.0f, &vel, 0.25f, 1.0f / 60.0f, &damped) != SolsticeV1_ResultSuccess) {
        return Fail(236, "SolsticeV1_MotionSmoothDampFloat failed");
    }

    SolsticeV1_SpritePhysicsWorldHandle spWorld = nullptr;
    if (SolsticeV1_SpritePhysicsCreate(&spWorld) != SolsticeV1_ResultSuccess || spWorld == nullptr) {
        return Fail(237, "SolsticeV1_SpritePhysicsCreate failed");
    }
    SolsticeV1_SpritePhysicsSetGravity(spWorld, 0.0f, 400.0f);
    SolsticeV1_SpritePhysicsSetBounds(spWorld, 0.0f, 0.0f, 1280.0f, 720.0f);
    uint32_t bodyId = 0;
    if (SolsticeV1_SpritePhysicsAddBody(spWorld, 100.0f, 100.0f, 24.0f, 24.0f, 1.0f, SolsticeV1_True, &bodyId)
        != SolsticeV1_ResultSuccess) {
        SolsticeV1_SpritePhysicsDestroy(spWorld);
        return Fail(238, "SolsticeV1_SpritePhysicsAddBody failed");
    }
    SolsticeV1_SpritePhysicsStep(spWorld, 1.0f / 60.0f);
    float cx = 0.0f;
    float cy = 0.0f;
    if (SolsticeV1_SpritePhysicsGetCenter(spWorld, bodyId, &cx, &cy) != SolsticeV1_ResultSuccess) {
        SolsticeV1_SpritePhysicsDestroy(spWorld);
        return Fail(239, "SolsticeV1_SpritePhysicsGetCenter failed");
    }
    SolsticeV1_SpritePhysicsDestroy(spWorld);
    return 0;
}

int TestPluginModule() {
    uint32_t id = 999u;
    if (SolsticeV1_PluginLoad("__solstice_nonexistent_plugin__.dll", &id) == SolsticeV1_ResultSuccess) {
        return Fail(240, "SolsticeV1_PluginLoad should fail for a missing library");
    }
    if (id != SOLSTICE_V1_PLUGIN_INVALID_ID) {
        return Fail(241, "SolsticeV1_PluginLoad should zero OutId on failure");
    }
    if (SolsticeV1_PluginGetLoadedCount() != 0u) {
        return Fail(242, "SolsticeV1_PluginGetLoadedCount unexpected after failed load");
    }
    if (SolsticeV1_PluginBeginHotReload(1u) != SolsticeV1_ResultFailure) {
        return Fail(243, "SolsticeV1_PluginBeginHotReload should fail without a loaded module");
    }
    if (SolsticeV1_PluginCompleteHotReload(1u, "__stub__.dll") != SolsticeV1_ResultFailure) {
        return Fail(244, "SolsticeV1_PluginCompleteHotReload should fail without an active session");
    }
    SolsticeV1_PluginAbortHotReload(1u);
    if (SolsticeV1_PluginIsHotReloadPending(1u) != SolsticeV1_False) {
        return Fail(245, "SolsticeV1_PluginIsHotReloadPending should be false");
    }

    g_PluginBusHits = 0;
    uint64_t busSub = 0;
    if (SolsticeV1_PluginBusSubscribe("capismoke.plugin.bus", &SolsticeSmoke_PluginBusCb, nullptr, &busSub) != SolsticeV1_ResultSuccess
        || busSub == 0) {
        return Fail(246, "SolsticeV1_PluginBusSubscribe failed");
    }
    static const char kBusJson[] = "{}";
    if (SolsticeV1_PluginBusPublish(0, "capismoke.plugin.bus", "application/json", reinterpret_cast<const uint8_t*>(kBusJson), 2u)
        != SolsticeV1_ResultSuccess) {
        SolsticeV1_PluginBusUnsubscribe(busSub);
        return Fail(247, "SolsticeV1_PluginBusPublish failed");
    }
    if (g_PluginBusHits != 1) {
        SolsticeV1_PluginBusUnsubscribe(busSub);
        return Fail(248, "SolsticeV1_PluginBusPublish did not deliver to subscriber");
    }
    SolsticeV1_PluginBusUnsubscribe(busSub);
    SolsticeV1_PluginBusClear();
    return 0;
}

#if defined(_WIN32)
int TestDllExportResolution() {
    HMODULE module = nullptr;
    if (!GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&SolsticeV1_CoreInitialize),
            &module)) {
        return Fail(250, "GetModuleHandleExA failed to resolve Solstice module");
    }

    auto resolvedCoreInit = reinterpret_cast<SolsticeV1_Bool (*)(void)>(
        GetProcAddress(module, "SolsticeV1_CoreInitialize"));
    auto resolvedPhysicsStart = reinterpret_cast<SolsticeV1_ResultCode (*)(SolsticeV1_PhysicsWorldHandle*)>(
        GetProcAddress(module, "SolsticeV1_PhysicsStart"));
    auto resolvedFluidCreate = reinterpret_cast<SolsticeV1_ResultCode (*)(
        int, int, int, float, float, float, float, float, SolsticeV1_FluidHandle*)>(
        GetProcAddress(module, "SolsticeV1_FluidCreate"));
    auto resolvedMotionEase = reinterpret_cast<SolsticeV1_ResultCode (*)(
        float, SolsticeV1_MotionEasingType, float, float*)>(
        GetProcAddress(module, "SolsticeV1_MotionEase"));
    auto resolvedSpritePhysCreate = reinterpret_cast<SolsticeV1_ResultCode (*)(SolsticeV1_SpritePhysicsWorldHandle*)>(
        GetProcAddress(module, "SolsticeV1_SpritePhysicsCreate"));
    auto resolvedPluginLoad = reinterpret_cast<SolsticeV1_ResultCode (*)(const char*, uint32_t*)>(
        GetProcAddress(module, "SolsticeV1_PluginLoad"));
    auto resolvedPluginHotBegin = reinterpret_cast<SolsticeV1_ResultCode (*)(uint32_t)>(
        GetProcAddress(module, "SolsticeV1_PluginBeginHotReload"));
    auto resolvedPluginBusPublish = reinterpret_cast<SolsticeV1_ResultCode (*)(
        uint32_t, const char*, const char*, const uint8_t*, uint32_t)>(
        GetProcAddress(module, "SolsticeV1_PluginBusPublish"));
    auto resolvedNetworkingStart = reinterpret_cast<SolsticeV1_ResultCode (*)(void)>(
        GetProcAddress(module, "SolsticeV1_NetworkingStart"));
    auto resolvedVideoOpen = reinterpret_cast<SolsticeV1_ResultCode (*)(
        const char*, SolsticeV1_VideoDecoder*)>(GetProcAddress(module, "SolsticeV1_VideoDecoderOpen"));

    if (!resolvedCoreInit || !resolvedPhysicsStart || !resolvedFluidCreate || !resolvedMotionEase || !resolvedSpritePhysCreate
        || !resolvedPluginLoad || !resolvedPluginHotBegin || !resolvedPluginBusPublish || !resolvedNetworkingStart
        || !resolvedVideoOpen) {
        return Fail(251, "GetProcAddress failed for one or more V1 exports");
    }
    return 0;
}
#endif

} // namespace

int main() {
    LogStage("Core");
    int code = TestCoreModule();
    if (code != 0) {
        SolsticeV1_CoreShutdown();
        return code;
    }

    LogStage("Physics");
    code = TestPhysicsModule();
    if (code == 0) LogStage("Networking");
    if (code == 0) code = TestNetworkingModule();
    if (code == 0) LogStage("Audio");
    if (code == 0) code = TestAudioModule();
    if (code == 0) LogStage("Video");
    if (code == 0) code = TestVideoModule();
    if (code == 0) LogStage("Scripting");
    if (code == 0) code = TestScriptingModule();
    if (code == 0) LogStage("NarrativeCutsceneSmf");
    if (code == 0) code = TestNarrativeCutsceneSmfModule();
    if (code == 0) LogStage("Fluid");
    if (code == 0) code = TestFluidModule();
    if (code == 0) LogStage("Profiler");
    if (code == 0) code = TestProfilerModule();
    if (code == 0) LogStage("Plugin");
    if (code == 0) code = TestPluginModule();
    const char* strictMotionEnv = std::getenv("CAPISMOKE_STRICT_MOTION");
    const bool strictMotion = (strictMotionEnv && strictMotionEnv[0] != '\0' && strictMotionEnv[0] != '0');
    if (code == 0 && strictMotion) {
        LogStage("MotionGraphics");
        code = TestMotionGraphicsModule();
    } else if (code == 0) {
        std::printf("[CAPISmokeTest] MotionGraphics stress skipped (set CAPISMOKE_STRICT_MOTION=1 to enable).\n");
    }
#if defined(_WIN32)
    const char* strictExportsEnv = std::getenv("CAPISMOKE_STRICT_EXPORTS");
    const bool strictExports = (strictExportsEnv && strictExportsEnv[0] != '\0' && strictExportsEnv[0] != '0');
    if (code == 0 && strictExports) {
        LogStage("Win32ExportResolution");
        code = TestDllExportResolution();
    } else if (code == 0) {
        std::printf("[CAPISmokeTest] Win32 export resolution skipped (set CAPISMOKE_STRICT_EXPORTS=1 to enable).\n");
    }
#endif

    const char* strictFullEnv = std::getenv("CAPISMOKE_STRICT_FULL");
    const bool strictFull = (strictFullEnv && strictFullEnv[0] != '\0' && strictFullEnv[0] != '0');
    if (code == 0 && !strictFull) {
        std::printf("[CAPISmokeTest] PASS - V1 API stress smoke completed.\n");
#if defined(_WIN32)
        ExitProcess(0);
#else
        std::_Exit(EXIT_SUCCESS);
#endif
    }

    LogStage("Shutdown");
    const char* strictShutdownEnv = std::getenv("CAPISMOKE_STRICT_SHUTDOWN");
    const bool strictShutdown = (strictShutdownEnv && strictShutdownEnv[0] != '\0' && strictShutdownEnv[0] != '0');
    if (strictShutdown) {
        // Deterministic strict-mode teardown: stop active subsystems explicitly,
        // then terminate process to avoid backend-dependent late teardown faults.
        SolsticeV1_NetworkingStop();
        SolsticeV1_PhysicsStop();
#if defined(_WIN32)
        ExitProcess(code == 0 ? 0u : static_cast<UINT>(code));
#else
        std::_Exit(code == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
#endif
    } else {
        std::printf("[CAPISmokeTest] Core shutdown skipped (set CAPISMOKE_STRICT_SHUTDOWN=1 to enforce).\n");
    }

    if (code != 0) {
        std::fprintf(stderr, "[CAPISmokeTest] Completed with failures.\n");
        return code;
    }

    std::printf("[CAPISmokeTest] PASS - V1 API stress smoke completed.\n");
#if defined(_WIN32)
    ExitProcess(0);
#else
    std::_Exit(EXIT_SUCCESS);
#endif
}
