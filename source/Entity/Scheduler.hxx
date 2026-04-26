#pragma once

#include <Solstice.hxx>
#include <Entity/System.hxx>
#include <functional>
#include <string>
#include <vector>

namespace Solstice::ECS {

enum class SystemPhase {
    Input = 0,
    Simulation,
    Presentation,
    Late
};

class SOLSTICE_API PhaseScheduler {
public:
    struct Entry {
        std::string Name;
        SystemPhase Phase{SystemPhase::Simulation};
        std::function<void(Registry&, float)> Execute;
    };

    void Clear() {
        m_Entries.clear();
    }

    void Register(SystemPhase phase, const std::string& name, std::function<void(Registry&, float)> fn) {
        if (!fn) {
            return;
        }
        m_Entries.push_back(Entry{name, phase, std::move(fn)});
    }

    void Register(SystemPhase phase, const std::string& name, ISystem& system) {
        Register(phase, name, [&system](Registry& registry, float deltaTime) {
            system.Update(registry, deltaTime);
        });
    }

    void ExecutePhase(SystemPhase phase, Registry& registry, float deltaTime);
    void ExecuteAll(Registry& registry, float deltaTime);

private:
    std::vector<Entry> m_Entries;
};

} // namespace Solstice::ECS
