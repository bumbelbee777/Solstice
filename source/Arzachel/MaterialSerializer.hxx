#pragma once

#include "../Solstice.hxx"
#include "ProceduralTexture.hxx"
#include "../Core/JSON.hxx"
#include "../Core/Material.hxx"
#include <string>

namespace Solstice::Arzachel {

class SOLSTICE_API MaterialSerializer {
public:
    // ProceduralMaterial serialization (backward compatibility)
    static Core::JSONValue Serialize(const ProceduralMaterial& Material);
    static ProceduralMaterial Deserialize(const Core::JSONValue& JSON);

    static Core::JSONValue SerializeTexture(const TextureData& Texture);
    static TextureData DeserializeTexture(const Core::JSONValue& JSON);

    static bool SaveToFile(const std::string& FilePath, const ProceduralMaterial& Material);
    static bool LoadFromFile(const std::string& FilePath, ProceduralMaterial& OutMaterial);

    // Core::Material serialization
    static Core::JSONValue Serialize(const Core::Material& Material);
    static Core::Material DeserializeMaterial(const Core::JSONValue& JSON);

    static bool SaveToFile(const std::string& FilePath, const Core::Material& Material);
    static bool LoadFromFile(const std::string& FilePath, Core::Material& OutMaterial);
};

} // namespace Solstice::Arzachel
