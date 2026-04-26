#pragma once

#include "SmfMap.hxx"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Solstice::Smf {

/// Returns entity index in `map.Entities`, or nullopt if not found.
inline std::optional<std::size_t> FindEntityIndex(const SmfMap& map, std::string_view name) {
    for (std::size_t i = 0; i < map.Entities.size(); ++i) {
        if (map.Entities[i].Name == name) {
            return i;
        }
    }
    return std::nullopt;
}

/// Returns property index in `entity.Properties`, or nullopt if not found.
inline std::optional<std::size_t> FindPropertyIndex(const SmfEntity& entity, std::string_view key) {
    for (std::size_t i = 0; i < entity.Properties.size(); ++i) {
        if (entity.Properties[i].Key == key) {
            return i;
        }
    }
    return std::nullopt;
}

struct SmfMapValidationMessage {
    enum class Severity { Warning, Error };
    Severity Level{Severity::Warning};
    std::string Text;
};

/// Editor-oriented checks (in-memory map; does not replace codec validation).
inline void ValidateMapStructure(const SmfMap& map, std::vector<SmfMapValidationMessage>& outMessages) {
    outMessages.clear();

    for (std::size_t i = 0; i < map.Entities.size(); ++i) {
        const auto& e = map.Entities[i];
        if (e.ClassName.empty()) {
            SmfMapValidationMessage m;
            m.Level = SmfMapValidationMessage::Severity::Warning;
            m.Text = "Entity [" + std::to_string(i) + "] '" + (e.Name.empty() ? std::string("(unnamed)") : e.Name)
                + "' has empty ClassName.";
            outMessages.push_back(std::move(m));
        }
        for (std::size_t j = i + 1; j < map.Entities.size(); ++j) {
            if (!map.Entities[i].Name.empty() && map.Entities[i].Name == map.Entities[j].Name) {
                SmfMapValidationMessage m;
                m.Level = SmfMapValidationMessage::Severity::Error;
                m.Text = "Duplicate entity name '" + map.Entities[i].Name + "' at indices " + std::to_string(i) + " and "
                    + std::to_string(j) + ".";
                outMessages.push_back(std::move(m));
            }
        }
        for (std::size_t pi = 0; pi < e.Properties.size(); ++pi) {
            for (std::size_t pj = pi + 1; pj < e.Properties.size(); ++pj) {
                if (!e.Properties[pi].Key.empty() && e.Properties[pi].Key == e.Properties[pj].Key) {
                    SmfMapValidationMessage m;
                    m.Level = SmfMapValidationMessage::Severity::Warning;
                    m.Text = "Entity '" + (e.Name.empty() ? std::string("(unnamed)") : e.Name) + "': duplicate property key '"
                        + e.Properties[pi].Key + "'.";
                    outMessages.push_back(std::move(m));
                }
            }
        }
    }

    for (std::size_t i = 0; i < map.PathTable.size(); ++i) {
        const auto& pathStr = map.PathTable[i].first;
        if (pathStr.empty()) {
            SmfMapValidationMessage m;
            m.Level = SmfMapValidationMessage::Severity::Warning;
            m.Text = "Path table entry [" + std::to_string(i) + "] has an empty path string.";
            outMessages.push_back(std::move(m));
        }
        for (std::size_t j = i + 1; j < map.PathTable.size(); ++j) {
            if (!pathStr.empty() && pathStr == map.PathTable[j].first) {
                SmfMapValidationMessage m;
                m.Level = SmfMapValidationMessage::Severity::Error;
                m.Text = "Duplicate path table entry '" + pathStr + "' at indices " + std::to_string(i) + " and "
                    + std::to_string(j) + ".";
                outMessages.push_back(std::move(m));
            }
        }
    }

    if (map.Bsp.has_value()) {
        const auto& b = *map.Bsp;
        if (b.RootIndex >= b.Nodes.size() && !b.Nodes.empty()) {
            SmfMapValidationMessage m;
            m.Level = SmfMapValidationMessage::Severity::Warning;
            m.Text = "BSP: RootIndex out of range.";
            outMessages.push_back(std::move(m));
        }
        for (std::size_t i = 0; i < b.Nodes.size(); ++i) {
            const auto& n = b.Nodes[i];
            auto checkChild = [&](int32_t c, const char* tag) {
                if (c >= 0 && static_cast<std::size_t>(c) >= b.Nodes.size()) {
                    SmfMapValidationMessage m;
                    m.Level = SmfMapValidationMessage::Severity::Error;
                    m.Text = "BSP: node " + std::to_string(i) + " " + tag + " index invalid.";
                    outMessages.push_back(std::move(m));
                }
            };
            checkChild(n.FrontChild, "front");
            checkChild(n.BackChild, "back");
            if (n.SlabValid) {
                if (n.SlabMin.x > n.SlabMax.x || n.SlabMin.y > n.SlabMax.y || n.SlabMin.z > n.SlabMax.z) {
                    SmfMapValidationMessage m;
                    m.Level = SmfMapValidationMessage::Severity::Warning;
                    m.Text = "BSP: node " + std::to_string(i)
                        + " slab bounds are inverted (min > max on an axis); editor will still draw using sorted AABB.";
                    outMessages.push_back(std::move(m));
                }
            }
        }
    }

    if (map.Octree.has_value()) {
        const auto& o = *map.Octree;
        if (!o.Nodes.empty() && o.RootIndex >= o.Nodes.size()) {
            SmfMapValidationMessage m;
            m.Level = SmfMapValidationMessage::Severity::Warning;
            m.Text = "Octree: RootIndex out of range.";
            outMessages.push_back(std::move(m));
        }
        for (std::size_t i = 0; i < o.Nodes.size(); ++i) {
            const auto& n = o.Nodes[i];
            for (int k = 0; k < 8; ++k) {
                const int32_t c = n.Children[static_cast<size_t>(k)];
                if (c >= 0 && static_cast<std::size_t>(c) >= o.Nodes.size()) {
                    SmfMapValidationMessage m;
                    m.Level = SmfMapValidationMessage::Severity::Error;
                    m.Text = "Octree: node " + std::to_string(i) + " child " + std::to_string(k) + " index invalid.";
                    outMessages.push_back(std::move(m));
                }
            }
        }
    }

    for (std::size_t i = 0; i < map.AcousticZones.size(); ++i) {
        const auto& z = map.AcousticZones[i];
        for (std::size_t j = i + 1; j < map.AcousticZones.size(); ++j) {
            if (!z.Name.empty() && z.Name == map.AcousticZones[j].Name) {
                SmfMapValidationMessage m;
                m.Level = SmfMapValidationMessage::Severity::Error;
                m.Text = "Duplicate acoustic zone name '" + z.Name + "' at indices " + std::to_string(i) + " and "
                    + std::to_string(j) + ".";
                outMessages.push_back(std::move(m));
            }
        }
    }

    for (std::size_t i = 0; i < map.AuthoringLights.size(); ++i) {
        const auto& L = map.AuthoringLights[i];
        for (std::size_t j = i + 1; j < map.AuthoringLights.size(); ++j) {
            if (!L.Name.empty() && L.Name == map.AuthoringLights[j].Name) {
                SmfMapValidationMessage m;
                m.Level = SmfMapValidationMessage::Severity::Error;
                m.Text = "Duplicate authoring light name '" + L.Name + "' at indices " + std::to_string(i) + " and "
                    + std::to_string(j) + ".";
                outMessages.push_back(std::move(m));
            }
        }
    }

    for (std::size_t i = 0; i < map.FluidVolumes.size(); ++i) {
        const auto& f = map.FluidVolumes[i];
        for (std::size_t j = i + 1; j < map.FluidVolumes.size(); ++j) {
            if (!f.Name.empty() && f.Name == map.FluidVolumes[j].Name) {
                SmfMapValidationMessage m;
                m.Level = SmfMapValidationMessage::Severity::Error;
                m.Text = "Duplicate fluid volume name '" + f.Name + "' at indices " + std::to_string(i) + " and "
                    + std::to_string(j) + ".";
                outMessages.push_back(std::move(m));
            }
        }
        if (f.Enabled) {
            const int64_t cells = static_cast<int64_t>(f.ResolutionX) * static_cast<int64_t>(f.ResolutionY)
                * static_cast<int64_t>(f.ResolutionZ);
            if (cells > kSmfFluidInteriorCellBudget) {
                SmfMapValidationMessage m;
                m.Level = SmfMapValidationMessage::Severity::Warning;
                m.Text = "Fluid '" + (f.Name.empty() ? std::string("(unnamed)") : f.Name) + "': resolution product "
                    + std::to_string(cells) + " exceeds budget " + std::to_string(kSmfFluidInteriorCellBudget)
                    + "; runtime will clamp for NSSolver stability.";
                outMessages.push_back(std::move(m));
            }
            if (f.ResolutionX < kSmfFluidResolutionMin || f.ResolutionY < kSmfFluidResolutionMin
                || f.ResolutionZ < kSmfFluidResolutionMin || f.ResolutionX > kSmfFluidResolutionMax
                || f.ResolutionY > kSmfFluidResolutionMax || f.ResolutionZ > kSmfFluidResolutionMax) {
                SmfMapValidationMessage m;
                m.Level = SmfMapValidationMessage::Severity::Warning;
                m.Text = "Fluid '" + (f.Name.empty() ? std::string("(unnamed)") : f.Name)
                    + "': resolution per axis should stay in [" + std::to_string(kSmfFluidResolutionMin) + ", "
                    + std::to_string(kSmfFluidResolutionMax) + "] for stable authoring.";
                outMessages.push_back(std::move(m));
            }
        }
    }
}

} // namespace Solstice::Smf
