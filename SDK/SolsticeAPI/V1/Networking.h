#pragma once

#include "Common.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t SolsticeV1_ConnectionHandle;
typedef uint64_t SolsticeV1_ListenSocketHandle;

typedef enum SolsticeV1_NetworkingConnectionState {
    SolsticeV1_NetworkingConnectionStateNone = 0,
    SolsticeV1_NetworkingConnectionStateConnecting = 1,
    SolsticeV1_NetworkingConnectionStateConnected = 2,
    SolsticeV1_NetworkingConnectionStateClosedByPeer = 3,
    SolsticeV1_NetworkingConnectionStateProblemDetectedLocally = 4,
    SolsticeV1_NetworkingConnectionStateFinWait = 5,
    SolsticeV1_NetworkingConnectionStateLinger = 6,
    SolsticeV1_NetworkingConnectionStateDead = 7,
} SolsticeV1_NetworkingConnectionState;

typedef enum SolsticeV1_NetworkingAddressFamily {
    SolsticeV1_NetworkingAddressFamilyAny = 0,
    SolsticeV1_NetworkingAddressFamilyIPv4 = 1,
    SolsticeV1_NetworkingAddressFamilyIPv6 = 2,
} SolsticeV1_NetworkingAddressFamily;

typedef enum SolsticeV1_NetworkingRelayPolicy {
    SolsticeV1_NetworkingRelayPolicyDefault = 0,
    SolsticeV1_NetworkingRelayPolicyPreferRelay = 1,
    SolsticeV1_NetworkingRelayPolicyForceRelayOnly = 2,
    SolsticeV1_NetworkingRelayPolicyDisableRelay = 3,
} SolsticeV1_NetworkingRelayPolicy;

typedef enum SolsticeV1_NetworkingSendFlags {
    SolsticeV1_NetworkingSendFlagNone = 0,
    SolsticeV1_NetworkingSendFlagReliable = 1 << 0,
    SolsticeV1_NetworkingSendFlagUnreliable = 1 << 1,
    SolsticeV1_NetworkingSendFlagNoNagle = 1 << 2,
    SolsticeV1_NetworkingSendFlagNoDelay = 1 << 3,
} SolsticeV1_NetworkingSendFlags;

typedef void (*SolsticeV1_NetworkingReceiveCallback)(
    SolsticeV1_ConnectionHandle Connection,
    const void* Data,
    size_t Size,
    int32_t Channel,
    SolsticeV1_Bool Reliable,
    void* UserData);

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NetworkingStart(void);
SOLSTICE_V1_API void SolsticeV1_NetworkingStop(void);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NetworkingPoll(void);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NetworkingIsRunning(SolsticeV1_Bool* OutRunning);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NetworkingGetActiveConnectionCount(uint32_t* OutCount);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NetworkingGetListenSocketCount(uint32_t* OutCount);

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NetworkingListenIPv4(
    const char* Address,
    uint16_t Port,
    SolsticeV1_ListenSocketHandle* OutListenSocket);

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NetworkingListenIPv6(
    const char* Address,
    uint16_t Port,
    SolsticeV1_ListenSocketHandle* OutListenSocket);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NetworkingConnectHost(
    const char* Host,
    uint16_t Port,
    SolsticeV1_NetworkingAddressFamily FamilyPreference,
    SolsticeV1_ConnectionHandle* OutConnection);

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NetworkingCloseConnection(SolsticeV1_ConnectionHandle Connection);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NetworkingCloseListenSocket(SolsticeV1_ListenSocketHandle ListenSocket);

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NetworkingSendEx(
    SolsticeV1_ConnectionHandle Connection,
    const void* Data,
    size_t Size,
    uint32_t SendFlags,
    int32_t Channel);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NetworkingFlush(SolsticeV1_ConnectionHandle Connection);

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NetworkingReceive(
    SolsticeV1_ConnectionHandle Connection,
    void* OutBuffer,
    size_t BufferSize,
    size_t* OutBytesReceived,
    int32_t* OutChannel,
    SolsticeV1_Bool* OutReliable);

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NetworkingReceiveAny(
    SolsticeV1_ConnectionHandle* OutConnection,
    void* OutBuffer,
    size_t BufferSize,
    size_t* OutBytesReceived,
    int32_t* OutChannel,
    SolsticeV1_Bool* OutReliable);

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NetworkingGetConnectionState(
    SolsticeV1_ConnectionHandle Connection,
    SolsticeV1_NetworkingConnectionState* OutState);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NetworkingGetRemoteAddress(
    SolsticeV1_ConnectionHandle Connection,
    char* OutAddressBuffer,
    size_t AddressBufferSize,
    uint16_t* OutPort);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NetworkingSetRelayPolicy(SolsticeV1_NetworkingRelayPolicy Policy);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NetworkingGetRelayPolicy(SolsticeV1_NetworkingRelayPolicy* OutPolicy);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NetworkingEnableRelayNetworkAccess(SolsticeV1_Bool Enable);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NetworkingSetReceiveCallback(
    SolsticeV1_NetworkingReceiveCallback Callback,
    void* UserData);

#ifdef __cplusplus
}
#endif
