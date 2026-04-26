#include <Material/SmatBinary.hxx>
#include <Material/Material.hxx>

#include <cassert>
#include <cstdint>
#include <cstring>
#include <filesystem>

using namespace Solstice::Core;
namespace Math = Solstice::Math;

static bool MaterialsEqualNoExtras(const Material& A, const Material& B) {
    return A.AlbedoRGBA == B.AlbedoRGBA && A.NormalMapIndex == B.NormalMapIndex && A.Metallic == B.Metallic &&
           A.SpecularPower == B.SpecularPower && A.EmissionRGBA == B.EmissionRGBA && A.AlbedoTexIndex == B.AlbedoTexIndex &&
           A.RoughnessTexIndex == B.RoughnessTexIndex && A.Flags == B.Flags && A.AlphaMode == B.AlphaMode &&
           A.ShadingModel == B.ShadingModel && A.LightmapScaleX == B.LightmapScaleX && A.LightmapScaleY == B.LightmapScaleY &&
           A.LightmapOffsetX == B.LightmapOffsetX && A.LightmapOffsetY == B.LightmapOffsetY && A.UVScaleX == B.UVScaleX &&
           A.UVScaleY == B.UVScaleY && A.AlbedoTexIndex2 == B.AlbedoTexIndex2 && A.AlbedoTexIndex3 == B.AlbedoTexIndex3 &&
           A.TextureBlendMode == B.TextureBlendMode && A.TextureBlendFactor == B.TextureBlendFactor && A.Opacity == B.Opacity &&
           A._padding[0] == B._padding[0];
}

int main() {
    Material Def;
    Def.SetAlbedoColor(Math::Vec3(0.2f, 0.9f, 0.4f), 0.33f);
    Def.SetEmission(Math::Vec3(0.1f, 0.0f, 0.0f), 0.5f);
    Def.AlbedoTexIndex = 42;
    Def.NormalMapIndex = 7;
    Def.Flags = MaterialFlag_CastsShadows;

    const std::filesystem::path Tmp = std::filesystem::temp_directory_path() / "solstice_smat_roundtrip_test.smat";
    std::string Path = Tmp.string();

    SmatError WErr = SmatError::None;
    assert(WriteSmat(Path, Def, &WErr));
    assert(WErr == SmatError::None);

    Material Loaded;
    SmatError RErr = SmatError::None;
    assert(ReadSmat(Path, Loaded, &RErr));
    assert(RErr == SmatError::None);
    assert(Loaded.Extras == nullptr);
    assert(MaterialsEqualNoExtras(Def, Loaded));

    std::filesystem::remove(Tmp);

    Material Full = Materials::CreateEmissive(Math::Vec3(0.9f, 0.1f, 0.2f), 1.2f);
    Full.Metallic = 200;
    Full.RoughnessTexIndex = 999;
    Full.UVScaleX = 512;
    Full.UVScaleY = 384;
    Full.AlbedoTexIndex2 = 3;
    Full.AlbedoTexIndex3 = 5;
    Full.TextureBlendMode = static_cast<uint8_t>(TextureBlendMode::Overlay);
    Full.SetTextureBlendFactor(0.75f);
    Full.Opacity = 200;

    const std::filesystem::path Tmp2 = std::filesystem::temp_directory_path() / "solstice_smat_full_test.smat";
    std::string Path2 = Tmp2.string();
    assert(WriteSmat(Path2, Full, nullptr));
    Material Loaded2;
    assert(ReadSmat(Path2, Loaded2, nullptr));
    assert(MaterialsEqualNoExtras(Full, Loaded2));
    std::filesystem::remove(Tmp2);

    return 0;
}
