#pragma once

#include <unordered_map>
#include <utility>

#include <Entity/EntityId.hxx>

namespace Solstice::ECS {
struct IComponentStorage {
    virtual ~IComponentStorage() = default;
    virtual void Remove(EntityId e) = 0;
    virtual bool Has(EntityId e) const = 0;
};

template<class T>
struct ComponentStorage : IComponentStorage {
    std::unordered_map<EntityId, T> Data;

    template<class... A>
    T& Add(EntityId e, A&&... a) {
        auto [it, inserted] = Data.emplace(e, T{std::forward<A>(a)...});
        if (!inserted) it->second = T{std::forward<A>(a)...};
        return it->second;
    }

    void Remove(EntityId e) override { Data.erase(e); }

    bool Has(EntityId e) const override { return Data.find(e) != Data.end(); }

    T& Get(EntityId e) { return Data.at(e); }
    const T& Get(EntityId e) const { return Data.at(e); }
};
}
