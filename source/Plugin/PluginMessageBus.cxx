#include <Plugin/PluginMessageBus.hxx>

#include <algorithm>
#include <cstring>
#include <utility>

namespace Solstice::Plugin {

PluginMessage PluginMessage::MakeJson(std::uint32_t senderId, std::string_view channel, std::string_view jsonUtf8) {
    PluginMessage m;
    m.SenderPluginId = senderId;
    m.Channel = std::string(channel);
    m.MimeType = "application/json";
    m.Payload.assign(jsonUtf8.begin(), jsonUtf8.end());
    return m;
}

PluginMessageBus& PluginMessageBus::Instance() {
    static PluginMessageBus s_Bus;
    return s_Bus;
}

bool PluginMessageBus::Match(const std::string& pattern, bool prefix, const std::string& channel) {
    if (prefix) {
        if (pattern.empty()) {
            return true;
        }
        if (channel.size() < pattern.size()) {
            return false;
        }
        return std::memcmp(channel.data(), pattern.data(), pattern.size()) == 0;
    }
    return pattern == channel;
}

std::uint64_t PluginMessageBus::Subscribe(std::string pattern, MessageHandler handler) {
    if (pattern.empty() || !handler) {
        return 0;
    }
    bool prefix = false;
    if (pattern.size() >= 2 && pattern.back() == '*') {
        prefix = true;
        pattern.pop_back();
    }

    std::lock_guard<std::mutex> lock(m_Mutex);
    const std::uint64_t id = m_NextId++;
    Subscription s;
    s.Id = id;
    s.Pattern = std::move(pattern);
    s.Prefix = prefix;
    s.Handler = std::move(handler);
    m_Subs.push_back(std::move(s));
    return id;
}

void PluginMessageBus::Unsubscribe(std::uint64_t subscriptionId) {
    if (subscriptionId == 0) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_Mutex);
    const auto it = std::remove_if(m_Subs.begin(), m_Subs.end(), [subscriptionId](const Subscription& s) { return s.Id == subscriptionId; });
    m_Subs.erase(it, m_Subs.end());
}

void PluginMessageBus::Publish(const PluginMessage& message) {
    std::vector<MessageHandler> copy;
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        copy.reserve(m_Subs.size());
        for (const Subscription& s : m_Subs) {
            if (Match(s.Pattern, s.Prefix, message.Channel) && s.Handler) {
                copy.push_back(s.Handler);
            }
        }
    }
    for (MessageHandler& fn : copy) {
        fn(message);
    }
}

void PluginMessageBus::Publish(
    std::uint32_t senderPluginId,
    std::string_view channel,
    std::string_view mimeType,
    const std::uint8_t* payload,
    std::size_t payloadSize) {
    PluginMessage m;
    m.SenderPluginId = senderPluginId;
    m.Channel = std::string(channel);
    m.MimeType = std::string(mimeType);
    if (payload && payloadSize > 0) {
        m.Payload.assign(payload, payload + payloadSize);
    }
    Publish(m);
}

void PluginMessageBus::Clear() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Subs.clear();
}

} // namespace Solstice::Plugin
