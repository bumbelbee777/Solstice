#pragma once

#include <cstdint>

namespace Solstice::Core {

/// Magic: 'S' 'M' 'A' 'T' (little-endian)
constexpr uint32_t SMAT_MAGIC = 0x54414D53u;

constexpr uint16_t SMAT_FORMAT_VERSION_MAJOR = 1;
/// Format 1.0: packed non-pointer `Material` fields; no `MaterialExtras` on disk.
constexpr uint16_t SMAT_FORMAT_VERSION_MINOR = 0;
constexpr uint16_t SMAT_MAX_SUPPORTED_LOAD_FORMAT_MINOR = 0;

enum class SmatError : int32_t {
    None = 0,
    InvalidMagic = 1,
    UnsupportedVersion = 2,
    CorruptPayload = 3,
    IoOpenFailed = 4,
    IoReadFailed = 5,
    IoWriteFailed = 6
};

struct SmatFileHeader {
    uint32_t Magic{SMAT_MAGIC};
    uint16_t FormatVersionMajor{SMAT_FORMAT_VERSION_MAJOR};
    uint16_t FormatVersionMinor{SMAT_FORMAT_VERSION_MINOR};
    uint32_t Flags{0};
    /// Byte length of the material record that follows (v1: sizeof(SmatMaterialV1)).
    uint32_t PayloadSize{0};
};

/// Packed v1 material blob (all fields from `Core::Material` except `Extras` pointer).
#pragma pack(push, 1)
struct SmatMaterialV1 {
    uint32_t AlbedoRGBA{};
    uint16_t NormalMapIndex{};
    uint8_t Metallic{};
    uint8_t SpecularPower{};
    uint32_t EmissionRGBA{};
    uint16_t AlbedoTexIndex{};
    uint16_t RoughnessTexIndex{};
    uint16_t Flags{};
    uint8_t AlphaMode{};
    uint8_t ShadingModel{};
    uint16_t LightmapScaleX{};
    uint16_t LightmapScaleY{};
    uint16_t LightmapOffsetX{};
    uint16_t LightmapOffsetY{};
    uint16_t UVScaleX{};
    uint16_t UVScaleY{};
    uint16_t AlbedoTexIndex2{};
    uint16_t AlbedoTexIndex3{};
    uint8_t TextureBlendMode{};
    uint8_t TextureBlendFactor{};
    uint8_t Opacity{};
    uint8_t _pad{};
};
#pragma pack(pop)

static_assert(sizeof(SmatMaterialV1) == 40, "SmatMaterialV1 size drift");

} // namespace Solstice::Core
