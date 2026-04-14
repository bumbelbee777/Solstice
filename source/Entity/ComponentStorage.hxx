#pragma once

#include <unordered_map>
#include <utility>
#include <stdexcept>

#include <Entity/EntityId.hxx>

namespace Solstice::ECS {
struct IComponentStorage {
    virtual ~IComponentStorage() = default;
    virtual void Remove(EntityId e) = 0;
    virtual bool Has(EntityId e) const = 0;
};

template<class T>
struct ComponentStorage : IComponentStorage {
    template<class... A>
    T& Add(EntityId e, A&&... a) {
        auto [it, inserted] = m_Data.emplace(e, T{std::forward<A>(a)...});
        if (!inserted) it->second = T{std::forward<A>(a)...};
        return it->second;
    }

    void Remove(EntityId e) override { m_Data.erase(e); }

    bool Has(EntityId e) const override { return m_Data.find(e) != m_Data.end(); }

    T* TryGet(EntityId e) {
        auto it = m_Data.find(e);
        if (it == m_Data.end()) return nullptr;
        return &it->second;
    }

    const T* TryGet(EntityId e) const {
        auto it = m_Data.find(e);
        if (it == m_Data.end()) return nullptr;
        return &it->second;
    }

    T& Get(EntityId e) {
        auto* ptr = TryGet(e);
        if (!ptr) throw std::out_of_range("Component not found");
        return *ptr;
    }

    const T& Get(EntityId e) const {
        const auto* ptr = TryGet(e);
        if (!ptr) throw std::out_of_range("Component not found");
        return *ptr;
    }

    const std::unordered_map<EntityId, T>& Entries() const { return m_Data; }
    std::unordered_map<EntityId, T>& Entries() { return m_Data; }
    size_t Size() const { return m_Data.size(); }

private:
    std::unordered_map<EntityId, T> m_Data;
};
}
