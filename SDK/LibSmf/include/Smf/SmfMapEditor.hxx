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
}

} // namespace Solstice::Smf
