#pragma once

#include <Smf/SmfMap.hxx>
#include <Smf/SmfTypes.hxx>

#include <variant>

namespace Jackhammer::Particles {

/// Authoring-time particle emitter marker (`origin` / `position` Vec3). Engine/runtime behavior is not wired yet.
inline constexpr const char* kEmitterClassName = "ParticleEmitter";

template <typename Fn>
void ForEachParticleEmitterOrigin(const Solstice::Smf::SmfMap& map, Fn&& fn) {
    using Solstice::Smf::SmfAttributeType;
    using Solstice::Smf::SmfVec3;
    for (const auto& e : map.Entities) {
        if (e.ClassName != kEmitterClassName) {
            continue;
        }
        for (const auto& pr : e.Properties) {
            if ((pr.Key == "origin" || pr.Key == "position") && pr.Type == SmfAttributeType::Vec3) {
                if (auto* v = std::get_if<SmfVec3>(&pr.Value)) {
                    fn(*v);
                    break;
                }
            }
        }
    }
}

} // namespace Jackhammer::Particles
