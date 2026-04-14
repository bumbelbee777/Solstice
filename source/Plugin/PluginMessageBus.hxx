#pragma once

#include <Solstice.hxx>

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace Solstice::Plugin {

/**
 * Structured inter-plugin (and host) messaging. Thread-safe pub/sub on logical channels.
 * Payload is opaque bytes; use MimeType (e.g. application/json) for interpretation.
 */
struct SOLSTICE_API PluginMessage {
    /** 0 = engine / host. */
    std::uint32_t SenderPluginId{0};
    std::string Channel;
    /** e.g. application/json, text/plain, application/octet-stream */
    std::string MimeType;
    std::vector<std::uint8_t> Payload;

    static PluginMessage MakeJson(std::uint32_t senderId, std::string_view channel, std::string_view jsonUtf8);
};

class SOLSTICE_API PluginMessageBus {
public:
    using MessageHandler = std::function<void(const PluginMessage&)>;

    static PluginMessageBus& Instance();

    PluginMessageBus(const PluginMessageBus&) = delete;
    PluginMessageBus& operator=(const PluginMessageBus&) = delete;

    /**
     * Subscribe to a channel. If `pattern` ends with '*', it is a prefix match (excluding the '*').
     * Returns subscription id, or 0 on failure.
     */
    std::uint64_t Subscribe(std::string pattern, MessageHandler handler);

    void Unsubscribe(std::uint64_t subscriptionId);

    /** Deliver to matching subscribers (copy-on-invoke; safe if handlers re-enter). */
    void Publish(const PluginMessage& message);

    void Publish(
        std::uint32_t senderPluginId,
        std::string_view channel,
        std::string_view mimeType,
        const std::uint8_t* payload,
        std::size_t payloadSize);

    void Clear();

private:
    PluginMessageBus() = default;

    struct Subscription {
        std::uint64_t Id{0};
        std::string Pattern;
        bool Prefix{false};
        MessageHandler Handler;
    };

    static bool Match(const std::string& pattern, bool prefix, const std::string& channel);

    mutable std::mutex m_Mutex;
    std::uint64_t m_NextId{1};
    std::vector<Subscription> m_Subs;
};

} // namespace Solstice::Plugin
