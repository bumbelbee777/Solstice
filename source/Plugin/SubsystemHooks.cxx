#include <Plugin/SubsystemHooks.hxx>

#include <algorithm>
#include <utility>

namespace Solstice::Plugin {

SubsystemHooks& SubsystemHooks::Instance() {
    static SubsystemHooks s_Instance;
    return s_Instance;
}

SubsystemHooks::HookHandle SubsystemHooks::Register(SubsystemHookKind kind, HookFn fn) {
    if (kind >= SubsystemHookKind::Count || !fn) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(m_Mutex);
    const HookHandle id = m_NextId++;
    auto& bucket = m_Buckets[static_cast<std::size_t>(kind)];
    Entry e;
    e.Id = id;
    e.Fn = std::move(fn);
    bucket.push_back(std::move(e));
    return id;
}

void SubsystemHooks::Unregister(HookHandle handle) {
    if (handle == 0) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_Mutex);
    for (auto& bucket : m_Buckets) {
        const auto it = std::remove_if(bucket.begin(), bucket.end(), [handle](const Entry& e) { return e.Id == handle; });
        if (it != bucket.end()) {
            bucket.erase(it, bucket.end());
            return;
        }
    }
}

void SubsystemHooks::Clear() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    for (auto& bucket : m_Buckets) {
        bucket.clear();
    }
}

void SubsystemHooks::Invoke(SubsystemHookKind kind, float deltaTime) {
    if (kind >= SubsystemHookKind::Count) {
        return;
    }
    std::vector<HookFn> copy;
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        const auto& bucket = m_Buckets[static_cast<std::size_t>(kind)];
        copy.reserve(bucket.size());
        for (const Entry& e : bucket) {
            if (e.Fn) {
                copy.push_back(e.Fn);
            }
        }
    }
    for (HookFn& fn : copy) {
        if (fn) {
            fn(deltaTime);
        }
    }
}

} // namespace Solstice::Plugin
