#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <vector>

namespace Solstice::Networking {

class Packet {
public:
    enum SendFlags : uint32_t {
        None = 0,
        ReliableFlag = 1 << 0,
        UnreliableFlag = 1 << 1,
        NoNagleFlag = 1 << 2,
        NoDelayFlag = 1 << 3,
    };

    enum class SendMode : uint8_t {
        Reliable = 0,
        Unreliable = 1,
    };

    Packet() = default;
    Packet(const void* data, size_t size, int channel = 0, SendMode mode = SendMode::Reliable)
        : m_Channel(channel), m_Mode(mode) {
        Assign(data, size);
    }

    void Assign(const void* data, size_t size) {
        m_Data.clear();
        if (!data || size == 0) {
            return;
        }
        m_Data.resize(size);
        std::memcpy(m_Data.data(), data, size);
    }

    template <typename T>
    void AppendPod(const T& value) {
        static_assert(std::is_trivially_copyable_v<T>, "AppendPod requires trivially copyable types");
        const auto* bytes = reinterpret_cast<const uint8_t*>(&value);
        m_Data.insert(m_Data.end(), bytes, bytes + sizeof(T));
    }

    template <typename T>
    bool ReadPod(size_t offset, T& outValue) const {
        static_assert(std::is_trivially_copyable_v<T>, "ReadPod requires trivially copyable types");
        if (offset + sizeof(T) > m_Data.size()) {
            return false;
        }
        std::memcpy(&outValue, m_Data.data() + offset, sizeof(T));
        return true;
    }

    const std::vector<uint8_t>& Data() const { return m_Data; }
    std::vector<uint8_t>& Data() { return m_Data; }
    size_t Size() const { return m_Data.size(); }
    bool Empty() const { return m_Data.empty(); }

    int Channel() const { return m_Channel; }
    void SetChannel(int channel) { m_Channel = channel; }

    SendMode Mode() const { return m_Mode; }
    void SetMode(SendMode mode) { m_Mode = mode; }

    uint32_t SendOptions() const { return m_SendOptions; }
    void SetSendOptions(uint32_t sendOptions) { m_SendOptions = sendOptions; }

private:
    std::vector<uint8_t> m_Data;
    int m_Channel{0};
    SendMode m_Mode{SendMode::Reliable};
    uint32_t m_SendOptions{None};
};

} // namespace Solstice::Networking
