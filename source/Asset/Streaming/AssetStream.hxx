#pragma once

#include <Solstice.hxx>
#include <Core/Platform/Cache.hxx>
#include <Core/System/Async.hxx>
#include <string>
#include <memory>
#include <unordered_map>
#include <future>
#include <functional>
#include <vector>
#include <cstdint>

namespace Solstice::Core {

// Asset types
enum class AssetType {
    Texture,
    Material,
    Audio,
    Model
};

// Opaque asset handle
struct AssetHandle {
    uint64_t ID;
    AssetType Type;

    AssetHandle() : ID(0), Type(AssetType::Texture) {}
    AssetHandle(uint64_t Id, AssetType T) : ID(Id), Type(T) {}

    bool operator==(const AssetHandle& Other) const {
        return ID == Other.ID && Type == Other.Type;
    }

    bool IsValid() const {
        return ID != 0;
    }
};

// Base asset data
struct AssetData {
    AssetType Type;
    std::string Path;
    size_t Size;
    std::vector<std::byte> Data;

    AssetData(AssetType T, const std::string& P) : Type(T), Path(P), Size(0) {}
    virtual ~AssetData() = default;
};

// Asset loading function type for streamed payloads.
using AssetDataLoader = std::function<std::shared_ptr<AssetData>(const std::string&)>;

// Asset streamer for async loading
class SOLSTICE_API AssetStreamer {
public:
    AssetStreamer() = default;

    void SetLoader(AssetType Type, AssetDataLoader Loader) {
        m_Loaders[Type] = std::move(Loader);
    }

    AssetHandle RequestAsset(AssetType Type, const std::string& Path) {
        // Check cache first
        std::string cacheKey = GetCacheKey(Type, Path);
        std::shared_ptr<AssetData> cached;
        if (m_Cache.Get(cacheKey, cached)) {
            // Return existing handle
            AssetHandle handle = GetOrCreateHandle(Type, Path);
            return handle;
        }

        // Create handle
        AssetHandle handle = GetOrCreateHandle(Type, Path);

        // Queue for async loading
        LockGuard guard(m_Lock);
        if (m_PendingLoads.find(handle.ID) == m_PendingLoads.end()) {
            auto future = Solstice::Core::JobSystem::Instance().SubmitAsync([this, Type, Path, handle]() {
                LoadAsset(Type, Path, handle);
            });
            m_PendingLoads[handle.ID] = std::move(future);
        }

        return handle;
    }

    bool GetAsset(AssetHandle Handle, std::shared_ptr<AssetData>& OutAsset) {
        if (!Handle.IsValid()) {
            return false;
        }

        // Check if loading is complete
        LockGuard guard(m_Lock);
        auto pendingIt = m_PendingLoads.find(Handle.ID);
        if (pendingIt != m_PendingLoads.end()) {
            // Wait for loading to complete
            try {
                pendingIt->second.wait();
            } catch (...) {
                return false;
            }
            m_PendingLoads.erase(pendingIt);
        }

        // Get from cache
        std::string path = GetPathFromHandle(Handle);
        if (path.empty()) {
            return false;
        }

        std::string cacheKey = GetCacheKey(Handle.Type, path);
        return m_Cache.Get(cacheKey, OutAsset);
    }

    void PrefetchAsset(AssetType Type, const std::string& Path) {
        RequestAsset(Type, Path);
    }

    void UnloadAsset(AssetHandle Handle) {
        if (!Handle.IsValid()) {
            return;
        }

        LockGuard guard(m_Lock);
        std::string path = GetPathFromHandle(Handle);
        if (!path.empty()) {
            std::string cacheKey = GetCacheKey(Handle.Type, path);
            m_Cache.Remove(cacheKey);
        }

        m_Handles.erase(Handle.ID);
        m_PendingLoads.erase(Handle.ID);
    }

    void Clear() {
        LockGuard guard(m_Lock);
        m_Cache.Clear();
        m_Handles.clear();
        m_PendingLoads.clear();
        m_NextHandleID = 1;
    }

    size_t GetCacheSize() const {
        return m_Cache.Size();
    }

    void SetCacheSize(size_t MaxSize) {
        // Cache size is set at construction, but we can recreate
        // For now, this is a placeholder
    }

private:
    void LoadAsset(AssetType Type, const std::string& Path, AssetHandle Handle) {
        auto loaderIt = m_Loaders.find(Type);
        if (loaderIt == m_Loaders.end()) {
            return; // No loader registered
        }

        std::shared_ptr<AssetData> asset = loaderIt->second(Path);
        if (asset) {
            // Store in cache
            std::string cacheKey = GetCacheKey(Type, Path);
            m_Cache.Put(cacheKey, asset);
        }
    }

    AssetHandle GetOrCreateHandle(AssetType Type, const std::string& Path) {
        LockGuard guard(m_Lock);

        // Check if handle already exists for this path
        for (const auto& [id, path] : m_Handles) {
            if (path == Path) {
                return AssetHandle(id, Type);
            }
        }

        // Create new handle
        uint64_t id = m_NextHandleID++;
        m_Handles[id] = Path;
        return AssetHandle(id, Type);
    }

    std::string GetPathFromHandle(AssetHandle Handle) const {
        LockGuard guard(m_Lock);
        auto it = m_Handles.find(Handle.ID);
        if (it != m_Handles.end()) {
            return it->second;
        }
        return "";
    }

    std::string GetCacheKey(AssetType Type, const std::string& Path) const {
        return std::to_string(static_cast<int>(Type)) + ":" + Path;
    }

    mutable Spinlock m_Lock;
    Cache<std::string, std::shared_ptr<AssetData>> m_Cache{1000}; // Max 1000 cached assets
    std::unordered_map<AssetType, AssetDataLoader> m_Loaders;
    std::unordered_map<uint64_t, std::string> m_Handles;
    std::unordered_map<uint64_t, std::future<void>> m_PendingLoads;
    uint64_t m_NextHandleID = 1;
};

} // namespace Solstice::Core

