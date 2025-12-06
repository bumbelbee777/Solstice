#pragma once

#include <unordered_map>
#include <unordered_set>
#include <typeindex>
#include <memory>
#include <utility>

#include <Entity/EntityId.hxx>
#include <Entity/ComponentStorage.hxx>

namespace Solstice::ECS {
class Registry {
public:
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
    void Remove(EntityId e) {
        Ensure<T>().Remove(e);
    }

    template<class T, class F>
    void ForEach(F&& fn) {
        auto* s = GetStore<T>();
        if (!s) return;
        for (auto& [e, c] : s->Data) if (Valid(e)) fn(e, c);
    }

    template<class T1, class T2, class F>
    void ForEach(F&& fn) {
        auto* s1 = GetStore<T1>();
        auto* s2 = GetStore<T2>();
        if (!s1 || !s2) return;
        if (s1->Data.size() < s2->Data.size()) {
            for (auto& [e, c1] : s1->Data) if (Valid(e) && s2->Has(e)) fn(e, c1, s2->Get(e));
        } else {
            for (auto& [e, c2] : s2->Data) if (Valid(e) && s1->Has(e)) fn(e, s1->Get(e), c2);
        }
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
