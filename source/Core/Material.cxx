#include "Material.hxx"
#include "JSON.hxx"
#include <memory>
#include <stdexcept>

namespace Solstice::Core {

JSONValue Material::Serialize(const Material& Mat) {
    using JSONObject = Core::JSONObject;
    JSONObject Obj;

    // Serialize all material fields
    Obj["AlbedoRGBA"] = static_cast<double>(Mat.AlbedoRGBA);
    Obj["NormalMapIndex"] = static_cast<double>(Mat.NormalMapIndex);
    Obj["Metallic"] = static_cast<double>(Mat.Metallic);
    Obj["SpecularPower"] = static_cast<double>(Mat.SpecularPower);
    Obj["EmissionRGBA"] = static_cast<double>(Mat.EmissionRGBA);
    Obj["AlbedoTexIndex"] = static_cast<double>(Mat.AlbedoTexIndex);
    Obj["RoughnessTexIndex"] = static_cast<double>(Mat.RoughnessTexIndex);
    Obj["Flags"] = static_cast<double>(Mat.Flags);
    Obj["AlphaMode"] = static_cast<double>(Mat.AlphaMode);
    Obj["ShadingModel"] = static_cast<double>(Mat.ShadingModel);
    Obj["LightmapScaleX"] = static_cast<double>(Mat.LightmapScaleX);
    Obj["LightmapScaleY"] = static_cast<double>(Mat.LightmapScaleY);
    Obj["LightmapOffsetX"] = static_cast<double>(Mat.LightmapOffsetX);
    Obj["LightmapOffsetY"] = static_cast<double>(Mat.LightmapOffsetY);
    Obj["UVScaleX"] = static_cast<double>(Mat.UVScaleX);
    Obj["UVScaleY"] = static_cast<double>(Mat.UVScaleY);
    Obj["AlbedoTexIndex2"] = static_cast<double>(Mat.AlbedoTexIndex2);
    Obj["AlbedoTexIndex3"] = static_cast<double>(Mat.AlbedoTexIndex3);
    Obj["TextureBlendMode"] = static_cast<double>(Mat.TextureBlendMode);
    Obj["TextureBlendFactor"] = static_cast<double>(Mat.TextureBlendFactor);

    // Serialize MaterialExtras if present
    if (Mat.Extras != nullptr) {
        using JSONObject = Core::JSONObject;
        JSONObject ExtrasObj;
        ExtrasObj["Buoyancy"] = Mat.Extras->Buoyancy;
        ExtrasObj["Flammability"] = Mat.Extras->Flammability;
        ExtrasObj["Conductiveness"] = Mat.Extras->Conductiveness;
        ExtrasObj["Density"] = Mat.Extras->Density;
        Obj["Extras"] = JSONValue(std::move(ExtrasObj));
    }

    return JSONValue(std::move(Obj));
}

Material Material::Deserialize(const JSONValue& JSON) {
    using JSONObject = Core::JSONObject;
    Material Mat;

    if (!JSON.IsObject()) {
        return Mat; // Return default material if JSON is invalid
    }

    const JSONObject& Obj = JSON.AsObject();

    // Deserialize all material fields
    if (Obj.find("AlbedoRGBA") != Obj.end()) {
        Mat.AlbedoRGBA = static_cast<uint32_t>(Obj.at("AlbedoRGBA").AsDouble());
    }
    if (Obj.find("NormalMapIndex") != Obj.end()) {
        Mat.NormalMapIndex = static_cast<uint16_t>(Obj.at("NormalMapIndex").AsDouble());
    }
    if (Obj.find("Metallic") != Obj.end()) {
        Mat.Metallic = static_cast<uint8_t>(Obj.at("Metallic").AsDouble());
    }
    if (Obj.find("SpecularPower") != Obj.end()) {
        Mat.SpecularPower = static_cast<uint8_t>(Obj.at("SpecularPower").AsDouble());
    }
    if (Obj.find("EmissionRGBA") != Obj.end()) {
        Mat.EmissionRGBA = static_cast<uint32_t>(Obj.at("EmissionRGBA").AsDouble());
    }
    if (Obj.find("AlbedoTexIndex") != Obj.end()) {
        Mat.AlbedoTexIndex = static_cast<uint16_t>(Obj.at("AlbedoTexIndex").AsDouble());
    }
    if (Obj.find("RoughnessTexIndex") != Obj.end()) {
        Mat.RoughnessTexIndex = static_cast<uint16_t>(Obj.at("RoughnessTexIndex").AsDouble());
    }
    if (Obj.find("Flags") != Obj.end()) {
        Mat.Flags = static_cast<uint16_t>(Obj.at("Flags").AsDouble());
    }
    if (Obj.find("AlphaMode") != Obj.end()) {
        Mat.AlphaMode = static_cast<uint8_t>(Obj.at("AlphaMode").AsDouble());
    }
    if (Obj.find("ShadingModel") != Obj.end()) {
        Mat.ShadingModel = static_cast<uint8_t>(Obj.at("ShadingModel").AsDouble());
    }
    if (Obj.find("LightmapScaleX") != Obj.end()) {
        Mat.LightmapScaleX = static_cast<uint16_t>(Obj.at("LightmapScaleX").AsDouble());
    }
    if (Obj.find("LightmapScaleY") != Obj.end()) {
        Mat.LightmapScaleY = static_cast<uint16_t>(Obj.at("LightmapScaleY").AsDouble());
    }
    if (Obj.find("LightmapOffsetX") != Obj.end()) {
        Mat.LightmapOffsetX = static_cast<uint16_t>(Obj.at("LightmapOffsetX").AsDouble());
    }
    if (Obj.find("LightmapOffsetY") != Obj.end()) {
        Mat.LightmapOffsetY = static_cast<uint16_t>(Obj.at("LightmapOffsetY").AsDouble());
    }
    if (Obj.find("UVScaleX") != Obj.end()) {
        Mat.UVScaleX = static_cast<uint16_t>(Obj.at("UVScaleX").AsDouble());
    }
    if (Obj.find("UVScaleY") != Obj.end()) {
        Mat.UVScaleY = static_cast<uint16_t>(Obj.at("UVScaleY").AsDouble());
    }
    if (Obj.find("AlbedoTexIndex2") != Obj.end()) {
        Mat.AlbedoTexIndex2 = static_cast<uint16_t>(Obj.at("AlbedoTexIndex2").AsDouble());
    }
    if (Obj.find("AlbedoTexIndex3") != Obj.end()) {
        Mat.AlbedoTexIndex3 = static_cast<uint16_t>(Obj.at("AlbedoTexIndex3").AsDouble());
    }
    if (Obj.find("TextureBlendMode") != Obj.end()) {
        Mat.TextureBlendMode = static_cast<uint8_t>(Obj.at("TextureBlendMode").AsDouble());
    }
    if (Obj.find("TextureBlendFactor") != Obj.end()) {
        Mat.TextureBlendFactor = static_cast<uint8_t>(Obj.at("TextureBlendFactor").AsDouble());
    }

    // Deserialize MaterialExtras if present
    if (Obj.find("Extras") != Obj.end() && Obj.at("Extras").IsObject()) {
        using JSONObject = Core::JSONObject;
        Mat.Extras = new MaterialExtras();
        const JSONObject& ExtrasObj = Obj.at("Extras").AsObject();

        if (ExtrasObj.find("Buoyancy") != ExtrasObj.end()) {
            Mat.Extras->Buoyancy = static_cast<float>(ExtrasObj.at("Buoyancy").AsDouble());
        }
        if (ExtrasObj.find("Flammability") != ExtrasObj.end()) {
            Mat.Extras->Flammability = static_cast<float>(ExtrasObj.at("Flammability").AsDouble());
        }
        if (ExtrasObj.find("Conductiveness") != ExtrasObj.end()) {
            Mat.Extras->Conductiveness = static_cast<float>(ExtrasObj.at("Conductiveness").AsDouble());
        }
        if (ExtrasObj.find("Density") != ExtrasObj.end()) {
            Mat.Extras->Density = static_cast<float>(ExtrasObj.at("Density").AsDouble());
        }
    }

    return Mat;
}

JSONValue Material::ToJSON() const {
    return Serialize(*this);
}

bool Material::FromJSON(const JSONValue& JSON) {
    try {
        *this = Deserialize(JSON);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace Solstice::Core
