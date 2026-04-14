#pragma once

#include <unordered_map>
#include <unordered_set>
#include <typeindex>
#include <memory>
#include <utility>
#include <tuple>

#include <Entity/EntityId.hxx>
#include <Entity/ComponentStorage.hxx>

namespace Solstice::ECS {
class Registry {
public:
    template<class... T>
    struct Exclude {
    };

    EntityId Create() {
        EntityId id = m_NextId++;
        m_Alive.insert(id);
        return id;
    }

    void Destroy(EntityId e) {
        if (!Valid(e)) return;
        for (auto& [_, store] : m_Stores) store->Remove(e);
        m_Alive.erase(e);
    }

    bool Valid(EntityId e) const { return m_Alive.find(e) != m_Alive.end(); }

    template<class T, class... A>
    T& Add(EntityId e, A&&... a) {
        auto& s = Ensure<T>();
        return s.Add(e, std::forward<A>(a)...);
    }

    template<class T>
    bool Has(EntityId e) const {
        const auto it = m_Stores.find(std::type_index(typeid(T)));
        if (it == m_Stores.end()) return false;
        return static_cast<const ComponentStorage<T>*>(it->second.get())->Has(e);
    }

    template<class T>
    T& Get(EntityId e) {
        return Ensure<T>().Get(e);
    }

    template<class T>
    const T& Get(EntityId e) const {
        return GetStore<T>()->Get(e);
    }

    template<class T>
    T* TryGet(EntityId e) {
        auto* s = GetStore<T>();
        return s ? s->TryGet(e) : nullptr;
    }

    template<class T>
    const T* TryGet(EntityId e) const {
        auto* s = GetStore<T>();
        return s ? s->TryGet(e) : nullptr;
    }

    template<class T>
    void Remove(EntityId e) {
        auto* s = GetStore<T>();
        if (s) s->Remove(e);
    }

    template<class... T, class F>
    void ForEach(F&& fn) {
        static_assert(sizeof...(T) > 0, "ForEach requires at least one component type");
        using First = std::tuple_element_t<0, std::tuple<T...>>;
        auto* primaryStore = GetStore<First>();
        if (!primaryStore) return;

        for (const auto& [e, _] : primaryStore->Entries()) {
            if (!Valid(e) || !HasAll<T...>(e)) continue;
            fn(e, Get<T>(e)...);
        }
    }

    template<class... Include, class... Excluded, class F>
    void ForEachFiltered(Exclude<Excluded...>, F&& fn) {
        static_assert(sizeof...(Include) > 0, "ForEachFiltered requires at least one include component");
        using First = std::tuple_element_t<0, std::tuple<Include...>>;
        auto* primaryStore = GetStore<First>();
        if (!primaryStore) return;

        for (const auto& [e, _] : primaryStore->Entries()) {
            if (!Valid(e) || !HasAll<Include...>(e) || HasAny<Excluded...>(e)) continue;
            fn(e, Get<Include>(e)...);
        }
    }

    template<class... T>
    bool HasAll(EntityId e) const {
        if constexpr (sizeof...(T) == 0) {
            return true;
        }
        return (Has<T>(e) && ...);
    }

    template<class... T>
    bool HasAny(EntityId e) const {
        if constexpr (sizeof...(T) == 0) {
            return false;
        }
        return (Has<T>(e) || ...);
    }

private:
    template<class T>
    ComponentStorage<T>& Ensure() {
        auto key = std::type_index(typeid(T));
        auto it = m_Stores.find(key);
        if (it == m_Stores.end()) {
            it = m_Stores.emplace(key, std::make_unique<ComponentStorage<T>>()).first;
        }
        return *static_cast<ComponentStorage<T>*>(it->second.get());
    }

    template<class T>
    const ComponentStorage<T>* GetStore() const {
        auto it = m_Stores.find(std::type_index(typeid(T)));
        if (it == m_Stores.end()) return nullptr;
        return static_cast<const ComponentStorage<T>*>(it->second.get());
    }

    template<class T>
    ComponentStorage<T>* GetStore() {
        auto it = m_Stores.find(std::type_index(typeid(T)));
        if (it == m_Stores.end()) return nullptr;
        return static_cast<ComponentStorage<T>*>(it->second.get());
    }

    std::unordered_map<std::type_index, std::unique_ptr<IComponentStorage>> m_Stores;
    std::unordered_set<EntityId> m_Alive;
    EntityId m_NextId{1};
};
}
