#pragma once

#include <Solstice.hxx>
#include <Core/System/Async.hxx>
#include <Core/System/Allocator.hxx>
#include <unordered_map>
#include <list>
#include <functional>
#include <memory>
#include <future>
#include <cstddef>
#include <cstring>

namespace Solstice::Core {

// LRU Cache implementation
template<typename TKey, typename TValue>
class Cache {
public:
    explicit Cache(size_t MaxSize) : m_MaxSize(MaxSize) {}

    bool Get(const TKey& Key, TValue& OutValue) {
        LockGuard guard(m_Lock);
        auto it = m_Map.find(Key);
        if (it == m_Map.end()) {
            return false;
        }

        // Move to front (most recently used)
        m_List.splice(m_List.begin(), m_List, it->second);
        OutValue = it->second->second;
        return true;
    }

    void Put(const TKey& Key, const TValue& Value) {
        LockGuard guard(m_Lock);
        auto it = m_Map.find(Key);
        if (it != m_Map.end()) {
            // Update existing
            it->second->second = Value;
            m_List.splice(m_List.begin(), m_List, it->second);
            return;
        }

        // Add new entry
        if (m_List.size() >= m_MaxSize) {
            // Evict least recently used
            auto last = m_List.back();
            m_Map.erase(last.first);
            m_List.pop_back();
        }

        m_List.emplace_front(Key, Value);
        m_Map[Key] = m_List.begin();
    }

    void Remove(const TKey& Key) {
        LockGuard guard(m_Lock);
        auto it = m_Map.find(Key);
        if (it != m_Map.end()) {
            m_List.erase(it->second);
            m_Map.erase(it);
        }
    }

    void Clear() {
        LockGuard guard(m_Lock);
        m_Map.clear();
        m_List.clear();
    }

    size_t Size() const {
        LockGuard guard(m_Lock);
        return m_Map.size();
    }

private:
    using ListType = std::list<std::pair<TKey, TValue>>;
    using MapType = std::unordered_map<TKey, typename ListType::iterator>;

    mutable Spinlock m_Lock;
    size_t m_MaxSize;
    ListType m_List;
    MapType m_Map;
};

// Content-addressable storage with hash-based deduplication
template<typename T>
class DeduplicationCache {
public:
    using HashType = size_t;

    struct Entry {
        T Data;
        size_t RefCount;
        HashType Hash;
    };

    HashType ComputeHash(const T& Data) const {
        // Simple hash for byte data
        constexpr HashType HASH_MULTIPLIER = 31;
        HashType hash = 0;
        const std::byte* bytes = reinterpret_cast<const std::byte*>(&Data);
        size_t size = sizeof(T);
        for (size_t i = 0; i < size; ++i) {
            hash = hash * HASH_MULTIPLIER + static_cast<HashType>(bytes[i]);
        }
        return hash;
    }

    HashType ComputeHash(const void* Data, size_t Size) const {
        constexpr HashType HASH_MULTIPLIER = 31;
        HashType hash = 0;
        const std::byte* bytes = static_cast<const std::byte*>(Data);
        for (size_t i = 0; i < Size; ++i) {
            hash = hash * HASH_MULTIPLIER + static_cast<HashType>(bytes[i]);
        }
        return hash;
    }

    HashType Insert(const T& Data) {
        LockGuard guard(m_Lock);
        HashType hash = ComputeHash(Data);
        auto it = m_Map.find(hash);
        if (it != m_Map.end()) {
            // Compare data to ensure it's actually the same
            if (std::memcmp(&it->second.Data, &Data, sizeof(T)) == 0) {
                ++it->second.RefCount;
                return hash;
            }
        }

        // Insert new entry
        Entry entry;
        entry.Data = Data;
        entry.RefCount = 1;
        entry.Hash = hash;
        m_Map[hash] = entry;
        return hash;
    }

    HashType Insert(const void* Data, size_t Size) {
        LockGuard guard(m_Lock);
        HashType hash = ComputeHash(Data, Size);
        auto it = m_Map.find(hash);
        if (it != m_Map.end()) {
            // Compare data
            if (std::memcmp(&it->second.Data, Data, Size) == 0) {
                ++it->second.RefCount;
                return hash;
            }
        }

        // For variable-size data, we'd need a different storage mechanism
        // For now, this is a simplified version
        return hash;
    }

    bool Get(HashType Hash, T& OutData) const {
        LockGuard guard(m_Lock);
        auto it = m_Map.find(Hash);
        if (it != m_Map.end()) {
            OutData = it->second.Data;
            return true;
        }
        return false;
    }

    void Release(HashType Hash) {
        LockGuard guard(m_Lock);
        auto it = m_Map.find(Hash);
        if (it != m_Map.end()) {
            --it->second.RefCount;
            if (it->second.RefCount == 0) {
                m_Map.erase(it);
            }
        }
    }

    void Clear() {
        LockGuard guard(m_Lock);
        m_Map.clear();
    }

    size_t Size() const {
        LockGuard guard(m_Lock);
        return m_Map.size();
    }

private:
    mutable Spinlock m_Lock;
    std::unordered_map<HashType, Entry> m_Map;
};

// Async prefetcher with prediction hints
template<typename TKey, typename TValue>
class Prefetcher {
public:
    using LoadFunction = std::function<TValue(const TKey&)>;
    using PredictFunction = std::function<std::vector<TKey>(const TKey&)>;

    Prefetcher(LoadFunction LoadFn, PredictFunction PredictFn = nullptr)
        : m_LoadFn(std::move(LoadFn)), m_PredictFn(std::move(PredictFn)) {}

    void Prefetch(const TKey& Key) {
        LockGuard guard(m_Lock);
        if (m_Pending.find(Key) != m_Pending.end()) {
            return; // Already prefetching
        }

        auto future = Solstice::Core::JobSystem::Instance().SubmitAsync([this, Key]() {
            TValue value = m_LoadFn(Key);
            LockGuard guard2(m_Lock);
            m_Cache[Key] = value;
            m_Pending.erase(Key);
        });

        m_Pending[Key] = std::move(future);
    }

    void PrefetchWithPrediction(const TKey& Key) {
        Prefetch(Key);
        if (m_PredictFn) {
            auto predicted = m_PredictFn(Key);
            for (const auto& predKey : predicted) {
                Prefetch(predKey);
            }
        }
    }

    bool Get(const TKey& Key, TValue& OutValue) {
        LockGuard guard(m_Lock);
        auto it = m_Cache.find(Key);
        if (it != m_Cache.end()) {
            OutValue = it->second;
            return true;
        }
        return false;
    }

    void Clear() {
        LockGuard guard(m_Lock);
        m_Cache.clear();
        m_Pending.clear();
    }

private:
    mutable Spinlock m_Lock;
    LoadFunction m_LoadFn;
    PredictFunction m_PredictFn;
    std::unordered_map<TKey, TValue> m_Cache;
    std::unordered_map<TKey, std::future<void>> m_Pending;
};

} // namespace Solstice::Core
