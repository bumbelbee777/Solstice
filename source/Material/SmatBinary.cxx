#include "SmatBinary.hxx"

#include <cstring>
#include <fstream>

namespace Solstice::Core {

static void MaterialToSmatV1(const Material& Src, SmatMaterialV1& Dst) {
    std::memset(&Dst, 0, sizeof(Dst));
    Dst.AlbedoRGBA = Src.AlbedoRGBA;
    Dst.NormalMapIndex = Src.NormalMapIndex;
    Dst.Metallic = Src.Metallic;
    Dst.SpecularPower = Src.SpecularPower;
    Dst.EmissionRGBA = Src.EmissionRGBA;
    Dst.AlbedoTexIndex = Src.AlbedoTexIndex;
    Dst.RoughnessTexIndex = Src.RoughnessTexIndex;
    Dst.Flags = Src.Flags;
    Dst.AlphaMode = Src.AlphaMode;
    Dst.ShadingModel = Src.ShadingModel;
    Dst.LightmapScaleX = Src.LightmapScaleX;
    Dst.LightmapScaleY = Src.LightmapScaleY;
    Dst.LightmapOffsetX = Src.LightmapOffsetX;
    Dst.LightmapOffsetY = Src.LightmapOffsetY;
    Dst.UVScaleX = Src.UVScaleX;
    Dst.UVScaleY = Src.UVScaleY;
    Dst.AlbedoTexIndex2 = Src.AlbedoTexIndex2;
    Dst.AlbedoTexIndex3 = Src.AlbedoTexIndex3;
    Dst.TextureBlendMode = Src.TextureBlendMode;
    Dst.TextureBlendFactor = Src.TextureBlendFactor;
    Dst.Opacity = Src.Opacity;
    Dst._pad = Src._padding[0];
}

static void SmatV1ToMaterial(const SmatMaterialV1& Src, Material& Dst) {
    Dst.AlbedoRGBA = Src.AlbedoRGBA;
    Dst.NormalMapIndex = Src.NormalMapIndex;
    Dst.Metallic = Src.Metallic;
    Dst.SpecularPower = Src.SpecularPower;
    Dst.EmissionRGBA = Src.EmissionRGBA;
    Dst.AlbedoTexIndex = Src.AlbedoTexIndex;
    Dst.RoughnessTexIndex = Src.RoughnessTexIndex;
    Dst.Flags = Src.Flags;
    Dst.AlphaMode = Src.AlphaMode;
    Dst.ShadingModel = Src.ShadingModel;
    Dst.LightmapScaleX = Src.LightmapScaleX;
    Dst.LightmapScaleY = Src.LightmapScaleY;
    Dst.LightmapOffsetX = Src.LightmapOffsetX;
    Dst.LightmapOffsetY = Src.LightmapOffsetY;
    Dst.UVScaleX = Src.UVScaleX;
    Dst.UVScaleY = Src.UVScaleY;
    Dst.AlbedoTexIndex2 = Src.AlbedoTexIndex2;
    Dst.AlbedoTexIndex3 = Src.AlbedoTexIndex3;
    Dst.TextureBlendMode = Src.TextureBlendMode;
    Dst.TextureBlendFactor = Src.TextureBlendFactor;
    Dst.Opacity = Src.Opacity;
    Dst._padding[0] = Src._pad;
    Dst.Extras = nullptr;
}

bool WriteSmat(const std::string& FilePath, const Material& Mat, SmatError* OutError) {
    if (OutError) {
        *OutError = SmatError::None;
    }

    SmatFileHeader Hdr{};
    Hdr.Magic = SMAT_MAGIC;
    Hdr.FormatVersionMajor = SMAT_FORMAT_VERSION_MAJOR;
    Hdr.FormatVersionMinor = SMAT_FORMAT_VERSION_MINOR;
    Hdr.PayloadSize = static_cast<uint32_t>(sizeof(SmatMaterialV1));

    SmatMaterialV1 Payload{};
    MaterialToSmatV1(Mat, Payload);

    std::ofstream File(FilePath, std::ios::binary | std::ios::trunc);
    if (!File.is_open()) {
        if (OutError) {
            *OutError = SmatError::IoOpenFailed;
        }
        return false;
    }

    File.write(reinterpret_cast<const char*>(&Hdr), sizeof(Hdr));
    File.write(reinterpret_cast<const char*>(&Payload), sizeof(Payload));
    if (!File.good()) {
        if (OutError) {
            *OutError = SmatError::IoWriteFailed;
        }
        return false;
    }
    return true;
}

bool ReadSmat(const std::string& FilePath, Material& OutMaterial, SmatError* OutError) {
    if (OutError) {
        *OutError = SmatError::None;
    }

    std::ifstream File(FilePath, std::ios::binary);
    if (!File.is_open()) {
        if (OutError) {
            *OutError = SmatError::IoOpenFailed;
        }
        return false;
    }

    SmatFileHeader Hdr{};
    File.read(reinterpret_cast<char*>(&Hdr), sizeof(Hdr));
    if (File.gcount() != static_cast<std::streamsize>(sizeof(Hdr))) {
        if (OutError) {
            *OutError = SmatError::IoReadFailed;
        }
        return false;
    }

    if (Hdr.Magic != SMAT_MAGIC) {
        if (OutError) {
            *OutError = SmatError::InvalidMagic;
        }
        return false;
    }
    if (Hdr.FormatVersionMajor != SMAT_FORMAT_VERSION_MAJOR) {
        if (OutError) {
            *OutError = SmatError::UnsupportedVersion;
        }
        return false;
    }
    if (Hdr.FormatVersionMinor > SMAT_MAX_SUPPORTED_LOAD_FORMAT_MINOR) {
        if (OutError) {
            *OutError = SmatError::UnsupportedVersion;
        }
        return false;
    }
    if (Hdr.PayloadSize != sizeof(SmatMaterialV1)) {
        if (OutError) {
            *OutError = SmatError::CorruptPayload;
        }
        return false;
    }

    SmatMaterialV1 Payload{};
    File.read(reinterpret_cast<char*>(&Payload), sizeof(Payload));
    if (File.gcount() != static_cast<std::streamsize>(sizeof(Payload))) {
        if (OutError) {
            *OutError = SmatError::IoReadFailed;
        }
        return false;
    }

    SmatV1ToMaterial(Payload, OutMaterial);
    return true;
}

} // namespace Solstice::Core
