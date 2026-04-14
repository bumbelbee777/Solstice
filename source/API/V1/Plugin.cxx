#include "SolsticeAPI/V1/Plugin.h"
#include "Solstice.hxx"
#include <Plugin/PluginManager.hxx>
#include <Plugin/PluginMessageBus.hxx>

#include <cstring>
#include <string>

extern "C" {

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_PluginLoad(const char* PathUtf8, uint32_t* OutId) {
    if (!PathUtf8 || !OutId) {
        return SolsticeV1_ResultFailure;
    }
    *OutId = SOLSTICE_V1_PLUGIN_INVALID_ID;
    if (!Solstice::Initialized.load(std::memory_order_acquire)) {
        return SolsticeV1_ResultFailure;
    }
    std::string err;
    const std::uint32_t id = Solstice::Plugin::PluginManager::Instance().LoadNativeModule(PathUtf8, &err);
    if (id == 0) {
        return SolsticeV1_ResultFailure;
    }
    *OutId = id;
    return SolsticeV1_ResultSuccess;
}

SOLSTICE_V1_API void SolsticeV1_PluginUnload(uint32_t Id) {
    if (Id == SOLSTICE_V1_PLUGIN_INVALID_ID) {
        return;
    }
    Solstice::Plugin::PluginManager::Instance().UnloadNativeModule(Id);
}

SOLSTICE_V1_API void SolsticeV1_PluginUnloadAll(void) {
    Solstice::Plugin::PluginManager::Instance().UnloadAll();
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_PluginGetSymbol(uint32_t Id, const char* SymbolName, void** OutAddress) {
    if (!SymbolName || !OutAddress) {
        return SolsticeV1_ResultFailure;
    }
    *OutAddress = nullptr;
    if (Id == SOLSTICE_V1_PLUGIN_INVALID_ID) {
        return SolsticeV1_ResultFailure;
    }
    void* p = Solstice::Plugin::PluginManager::Instance().GetSymbol(Id, SymbolName);
    if (!p) {
        return SolsticeV1_ResultFailure;
    }
    *OutAddress = p;
    return SolsticeV1_ResultSuccess;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_PluginGetPath(
    uint32_t Id, char* OutBuffer, uint32_t BufferSize, uint32_t* OutWritten) {
    if (!OutWritten) {
        return SolsticeV1_ResultFailure;
    }
    *OutWritten = 0;
    if (Id == SOLSTICE_V1_PLUGIN_INVALID_ID) {
        return SolsticeV1_ResultFailure;
    }
    std::string path;
    if (!Solstice::Plugin::PluginManager::Instance().GetPath(Id, &path)) {
        return SolsticeV1_ResultFailure;
    }
    const size_t needWithNull = path.size() + 1u;
    *OutWritten = static_cast<uint32_t>(needWithNull);
    if (!OutBuffer || BufferSize < needWithNull) {
        return SolsticeV1_ResultFailure;
    }
    std::memcpy(OutBuffer, path.c_str(), needWithNull);
    return SolsticeV1_ResultSuccess;
}

SOLSTICE_V1_API uint32_t SolsticeV1_PluginGetLoadedCount(void) {
    return static_cast<uint32_t>(Solstice::Plugin::PluginManager::Instance().GetLoadedCount());
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_PluginBeginHotReload(uint32_t Id) {
    if (Id == SOLSTICE_V1_PLUGIN_INVALID_ID) {
        return SolsticeV1_ResultFailure;
    }
    std::string err;
    if (Solstice::Plugin::PluginManager::Instance().BeginHotReloadSession(Id, &err)) {
        return SolsticeV1_ResultSuccess;
    }
    return SolsticeV1_ResultFailure;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_PluginCompleteHotReload(uint32_t Id, const char* ReplacementPathUtf8) {
    if (Id == SOLSTICE_V1_PLUGIN_INVALID_ID || !ReplacementPathUtf8) {
        return SolsticeV1_ResultFailure;
    }
    std::string err;
    if (Solstice::Plugin::PluginManager::Instance().CompleteHotReloadSession(Id, ReplacementPathUtf8, &err)) {
        return SolsticeV1_ResultSuccess;
    }
    return SolsticeV1_ResultFailure;
}

SOLSTICE_V1_API void SolsticeV1_PluginAbortHotReload(uint32_t Id) {
    Solstice::Plugin::PluginManager::Instance().AbortHotReloadSession(Id);
}

SOLSTICE_V1_API SolsticeV1_Bool SolsticeV1_PluginIsHotReloadPending(uint32_t Id) {
    return Solstice::Plugin::PluginManager::Instance().IsHotReloadPending(Id) ? SolsticeV1_True : SolsticeV1_False;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_PluginBusPublish(
    uint32_t SenderPluginId,
    const char* ChannelUtf8,
    const char* MimeTypeUtf8,
    const uint8_t* Payload,
    uint32_t PayloadSize) {
    if (!ChannelUtf8 || !MimeTypeUtf8) {
        return SolsticeV1_ResultFailure;
    }
    if (!Solstice::Initialized.load(std::memory_order_acquire)) {
        return SolsticeV1_ResultFailure;
    }
    Solstice::Plugin::PluginMessageBus::Instance().Publish(
        SenderPluginId,
        ChannelUtf8,
        MimeTypeUtf8,
        Payload,
        static_cast<std::size_t>(PayloadSize));
    return SolsticeV1_ResultSuccess;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_PluginBusSubscribe(
    const char* ChannelPatternUtf8,
    SolsticeV1_PluginMessageCallback Callback,
    void* UserData,
    uint64_t* OutSubscriptionId) {
    if (!ChannelPatternUtf8 || !Callback || !OutSubscriptionId) {
        return SolsticeV1_ResultFailure;
    }
    if (!Solstice::Initialized.load(std::memory_order_acquire)) {
        return SolsticeV1_ResultFailure;
    }
    *OutSubscriptionId = 0;
    const std::uint64_t sid = Solstice::Plugin::PluginMessageBus::Instance().Subscribe(ChannelPatternUtf8, [Callback, UserData](const Solstice::Plugin::PluginMessage& m) {
        Callback(
            m.SenderPluginId,
            m.Channel.c_str(),
            m.MimeType.c_str(),
            m.Payload.empty() ? nullptr : m.Payload.data(),
            static_cast<uint32_t>(m.Payload.size()),
            UserData);
    });
    if (sid == 0) {
        return SolsticeV1_ResultFailure;
    }
    *OutSubscriptionId = sid;
    return SolsticeV1_ResultSuccess;
}

SOLSTICE_V1_API void SolsticeV1_PluginBusUnsubscribe(uint64_t SubscriptionId) {
    Solstice::Plugin::PluginMessageBus::Instance().Unsubscribe(SubscriptionId);
}

SOLSTICE_V1_API void SolsticeV1_PluginBusClear(void) {
    Solstice::Plugin::PluginMessageBus::Instance().Clear();
}

} // extern "C"
