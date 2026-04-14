#pragma once

#include "Common.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Invalid id for SolsticeV1_PluginLoad / native module handles. */
#define SOLSTICE_V1_PLUGIN_INVALID_ID 0u

/**
 * Loads a native shared library (DLL, .so, or .dylib) into the process.
 * `PathUtf8` must be UTF-8. Returns SolsticeV1_PluginInvalidId in *OutId on failure.
 * Requires SolsticeV1_CoreInitialize() to have completed successfully.
 */
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_PluginLoad(const char* PathUtf8, uint32_t* OutId);

SOLSTICE_V1_API void SolsticeV1_PluginUnload(uint32_t Id);
SOLSTICE_V1_API void SolsticeV1_PluginUnloadAll(void);

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_PluginGetSymbol(
    uint32_t Id, const char* SymbolName, void** OutAddress);

/**
 * Copies the load path for `Id` into `OutBuffer` (NUL-terminated if space allows).
 * `OutWritten` receives characters written excluding NUL, or required size if buffer too small.
 */
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_PluginGetPath(
    uint32_t Id, char* OutBuffer, uint32_t BufferSize, uint32_t* OutWritten);

SOLSTICE_V1_API uint32_t SolsticeV1_PluginGetLoadedCount(void);

/**
 * Hot-reload: Begin marks a loaded module for replacement; Complete loads the new library and swaps it in place
 * (id unchanged). On Windows the replacement file must differ from the mapped image (copy DLL to a new path if needed).
 */
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_PluginBeginHotReload(uint32_t Id);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_PluginCompleteHotReload(uint32_t Id, const char* ReplacementPathUtf8);
SOLSTICE_V1_API void SolsticeV1_PluginAbortHotReload(uint32_t Id);
SOLSTICE_V1_API SolsticeV1_Bool SolsticeV1_PluginIsHotReloadPending(uint32_t Id);

/** Inter-plugin message bus (structured opaque payloads; use MimeType e.g. application/json). */
typedef void (*SolsticeV1_PluginMessageCallback)(
    uint32_t SenderPluginId,
    const char* ChannelUtf8,
    const char* MimeTypeUtf8,
    const uint8_t* Payload,
    uint32_t PayloadSize,
    void* UserData);

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_PluginBusPublish(
    uint32_t SenderPluginId,
    const char* ChannelUtf8,
    const char* MimeTypeUtf8,
    const uint8_t* Payload,
    uint32_t PayloadSize);

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_PluginBusSubscribe(
    const char* ChannelPatternUtf8,
    SolsticeV1_PluginMessageCallback Callback,
    void* UserData,
    uint64_t* OutSubscriptionId);

SOLSTICE_V1_API void SolsticeV1_PluginBusUnsubscribe(uint64_t SubscriptionId);

SOLSTICE_V1_API void SolsticeV1_PluginBusClear(void);

#ifdef __cplusplus
}
#endif
