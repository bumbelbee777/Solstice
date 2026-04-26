#include "MaterialSerializer.hxx"
#include "../Material/SmatBinary.hxx"
#include "../Core/Serialization/Base64.hxx"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cctype>

namespace Solstice::Arzachel {

namespace {

bool FilePathExtensionEquals(const std::string& FilePath, const char* LowerExtWithDot) {
    std::filesystem::path P(FilePath);
    std::string Ext = P.extension().string();
    for (char& C : Ext) {
        C = static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
    }
    return Ext == LowerExtWithDot;
}

} // namespace

Core::JSONValue MaterialSerializer::Serialize(const ProceduralMaterial& Material) {
    Core::JSONObject Obj;
    Obj["Albedo"] = SerializeTexture(Material.Albedo);
    Obj["Normal"] = SerializeTexture(Material.Normal);
    Obj["Roughness"] = SerializeTexture(Material.Roughness);
    Obj["Metallic"] = SerializeTexture(Material.Metallic);
    return Core::JSONValue(std::move(Obj));
}

ProceduralMaterial MaterialSerializer::Deserialize(const Core::JSONValue& JSON) {
    ProceduralMaterial Mat;
    if (JSON.HasKey("Albedo")) Mat.Albedo = DeserializeTexture(JSON["Albedo"]);
    if (JSON.HasKey("Normal")) Mat.Normal = DeserializeTexture(JSON["Normal"]);
    if (JSON.HasKey("Roughness")) Mat.Roughness = DeserializeTexture(JSON["Roughness"]);
    if (JSON.HasKey("Metallic")) Mat.Metallic = DeserializeTexture(JSON["Metallic"]);
    return Mat;
}

Core::JSONValue MaterialSerializer::SerializeTexture(const TextureData& Texture) {
    Core::JSONObject Obj;
    Obj["Width"] = static_cast<double>(Texture.Width);
    Obj["Height"] = static_cast<double>(Texture.Height);
    Obj["Pixels"] = Core::Base64::Encode(Texture.Pixels);
    return Core::JSONValue(std::move(Obj));
}

TextureData MaterialSerializer::DeserializeTexture(const Core::JSONValue& JSON) {
    TextureData Tex;
    Tex.Width = static_cast<uint32_t>(JSON["Width"].AsDouble());
    Tex.Height = static_cast<uint32_t>(JSON["Height"].AsDouble());
    Tex.Pixels = Core::Base64::Decode(JSON["Pixels"].AsString());
    return Tex;
}

bool MaterialSerializer::SaveToFile(const std::string& FilePath, const ProceduralMaterial& Material) {
    std::ofstream File(FilePath);
    if (!File.is_open()) return false;

    Core::JSONValue JSON = Serialize(Material);
    File << JSON.Stringify(true);
    return true;
}

bool MaterialSerializer::LoadFromFile(const std::string& FilePath, ProceduralMaterial& OutMaterial) {
    std::ifstream File(FilePath);
    if (!File.is_open()) return false;

    std::stringstream Buffer;
    Buffer << File.rdbuf();

    try {
        Core::JSONValue JSON = Core::JSONParser::Parse(Buffer.str());
        OutMaterial = Deserialize(JSON);
        return true;
    } catch (...) {
        return false;
    }
}

// Core::Material serialization
Core::JSONValue MaterialSerializer::Serialize(const Core::Material& Material) {
    return Core::Material::Serialize(Material);
}

Core::Material MaterialSerializer::DeserializeMaterial(const Core::JSONValue& JSON) {
    return Core::Material::Deserialize(JSON);
}

bool MaterialSerializer::SaveToFile(const std::string& FilePath, const Core::Material& Material) {
    try {
        // Ensure directory exists
        std::filesystem::path Path(FilePath);
        if (Path.has_parent_path()) {
            std::filesystem::create_directories(Path.parent_path());
        }

        if (FilePathExtensionEquals(FilePath, ".smat")) {
            Core::SmatError Err = Core::SmatError::None;
            return Core::WriteSmat(FilePath, Material, &Err);
        }

        std::ofstream File(FilePath);
        if (!File.is_open()) {
            return false;
        }

        Core::JSONValue JSON = Serialize(Material);
        std::string JSONString = JSON.Stringify(true);
        File << JSONString;

        // Check if write was successful
        if (!File.good()) {
            return false;
        }

        File.close();
        return true;
    } catch (const std::exception& e) {
        // Log error if possible (but we can't use SIMPLE_LOG here as it might not be available)
        (void)e;
        return false;
    } catch (...) {
        return false;
    }
}

bool MaterialSerializer::LoadFromFile(const std::string& FilePath, Core::Material& OutMaterial) {
    if (FilePathExtensionEquals(FilePath, ".smat")) {
        Core::SmatError Err = Core::SmatError::None;
        return Core::ReadSmat(FilePath, OutMaterial, &Err);
    }

    std::ifstream File(FilePath);
    if (!File.is_open()) return false;

    std::stringstream Buffer;
    Buffer << File.rdbuf();

    try {
        Core::JSONValue JSON = Core::JSONParser::Parse(Buffer.str());
        OutMaterial = DeserializeMaterial(JSON);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace Solstice::Arzachel
