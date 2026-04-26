# Networking Subsystem

Solstice networking is built on GameNetworkingSockets and exposed through:

- C++ APIs in `source/Networking`
- C APIs in `SDK/SolsticeAPI/V1/Networking.h`

The phase-1 consolidated surface is canonical and wrapper-free:

- host-based connect (`ConnectHost`)
- explicit send options (`SendEx` / packet send flags)
- callback push receive support (`SetReceiveCallback`)
- relay control points (`SetRelayPolicy`, `EnableRelayNetworkAccess`)

## Core Types

- `Solstice::Networking::Packet`
  - payload bytes
  - channel id
  - send mode
  - send options flags
- `Solstice::Networking::Socket`
  - low-level transport implementation
- `Solstice::Networking::NetworkingSystem`
  - engine-facing singleton subsystem

## Canonical C++ API

- `bool Start()`
- `void Stop()`
- `bool IsRunning() const`
- `void Poll()`
- `bool ListenIPv4(std::string_view address, uint16_t port, uint64_t& outListenSocket)`
- `bool ListenIPv6(std::string_view address, uint16_t port, uint64_t& outListenSocket)`
- `bool ConnectHost(std::string_view host, uint16_t port, Socket::AddressFamily familyPreference, uint64_t& outConnection)`
- `bool Send(uint64_t connection, const Packet& packet)`
- `bool Flush(uint64_t connection)`
- `bool Receive(uint64_t connection, Packet& outPacket)`
- `bool ReceiveAny(uint64_t& outConnection, Packet& outPacket)`
- `void SetReceiveCallback(ReceiveCallback callback)`
- `void ClearReceiveCallback()`
- `bool SetRelayPolicy(Socket::RelayPolicy policy)`
- `Socket::RelayPolicy GetRelayPolicy() const`
- `void EnableRelayNetworkAccess(bool enable)`
- `Socket::ConnectionState GetConnectionState(uint64_t connection) const`
- `bool GetRemoteAddress(uint64_t connection, std::string& outAddress, uint16_t& outPort) const`

## Canonical C API

- `SolsticeV1_NetworkingStart`
- `SolsticeV1_NetworkingStop`
- `SolsticeV1_NetworkingPoll`
- `SolsticeV1_NetworkingIsRunning`
- `SolsticeV1_NetworkingGetActiveConnectionCount`
- `SolsticeV1_NetworkingGetListenSocketCount`
- `SolsticeV1_NetworkingListenIPv4`
- `SolsticeV1_NetworkingListenIPv6`
- `SolsticeV1_NetworkingConnectHost`
- `SolsticeV1_NetworkingCloseConnection`
- `SolsticeV1_NetworkingCloseListenSocket`
- `SolsticeV1_NetworkingSendEx`
- `SolsticeV1_NetworkingFlush`
- `SolsticeV1_NetworkingReceive`
- `SolsticeV1_NetworkingReceiveAny`
- `SolsticeV1_NetworkingGetConnectionState`
- `SolsticeV1_NetworkingGetRemoteAddress`
- `SolsticeV1_NetworkingSetRelayPolicy`
- `SolsticeV1_NetworkingGetRelayPolicy`
- `SolsticeV1_NetworkingEnableRelayNetworkAccess`
- `SolsticeV1_NetworkingSetReceiveCallback`

## Send Flags

`Packet` and C API use these flags:

- `Reliable`
- `Unreliable`
- `NoNagle`
- `NoDelay`

Example C API send:

```c
SolsticeV1_NetworkingSendEx(
    connection,
    data,
    dataSize,
    SolsticeV1_NetworkingSendFlagReliable | SolsticeV1_NetworkingSendFlagNoDelay,
    channel);
```

## Callback Push Model

- Receive callbacks are dispatched from `Poll()`.
- Pull-based receive (`Receive` / `ReceiveAny`) is still supported.
- Choose one primary consumption mode per connection to avoid double-consumption confusion.

## Relay/NAT Controls

- Relay control points are exposed via policy APIs.
- Backend capability can vary; unsupported controls are logged and treated as compatible no-op behavior rather than fatal errors.

## Lifecycle and Integration

- `Solstice::Initialize()` starts networking.
- `Solstice::Shutdown()` stops networking.
- `SolsticeV1_CoreShutdown()` stops networking via C API.

`Poll()` must run regularly to process connection callbacks and drive receive/callback dispatch.

### Game loop and ECS integration

- **`GameBase`** ([`source/Game/App/GameBase.cxx`](../source/Game/App/GameBase.cxx)) calls `NetworkingSystem::Poll()` once per frame after window event polling when networking is running, so callbacks and receive dispatch stay current during gameplay.
- **Multiplayer presets and ECS** live under [`source/Game/Networking/`](../source/Game/Networking/): `MultiplayerPresets` (session tuning, channels, message helpers) and `NetworkSessionCoordinator` (maps connections to ECS entities, installs `SetReceiveCallback`, registers Simulation-phase systems for heartbeat, app-level ping RTT, and timeouts). ECS component types for peers, teams, parties, and lobby state are in [`source/Entity/NetworkMultiplayer.hxx`](../source/Entity/NetworkMultiplayer.hxx). Use at most one `NetworkSessionCoordinator` per process (it owns the global receive callback slot).

## Tests

- `tests/NetworkingTest.cxx`
  - IPv4 + IPv6 loopback
  - DNS host connect path
  - callback push dispatch
  - send-option and reliability metadata checks
- `tests/CAPISmokeTest.cxx`
  - canonical C API flow (`ConnectHost`, `SendEx`, relay controls, callback bridge)
- `tests/ECSRegistryTest.cxx`
  - ECS registry modernization coverage

## Current Constraints

- Family preference in `ConnectHost` is strict (`IPv4` and `IPv6` preferences reject opposite-family addresses).
- Callback push latency is tied to `Poll()` cadence.
- Transport remains message-oriented (not stream-oriented).