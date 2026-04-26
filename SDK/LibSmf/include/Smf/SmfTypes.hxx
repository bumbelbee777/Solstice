#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace Solstice::Smf {

constexpr uint32_t SMF_MAGIC = 0x00464D53u; // 'S' 'M' 'F' '\0' little-endian
constexpr uint16_t SMF_FORMAT_VERSION_MAJOR = 1;
/// Declared format **1.0** (active development; not a production stability guarantee).
constexpr uint16_t SMF_FORMAT_VERSION_MINOR = 0;
/// Load accepts on-disk minors through this value (older dev builds may still have minor 2–3).
constexpr uint16_t SMF_MAX_SUPPORTED_LOAD_FORMAT_MINOR = 3;
constexpr uint32_t SMF_INVALID_INDEX = 0xFFFFFFFFu;

enum class SmfError {
    None,
    InvalidMagic,
    UnsupportedVersion,
    CorruptHeader,
    CorruptSection,
    CorruptStringTable,
    CorruptEntitySection,
    OutOfMemory,
    IoOpenFailed,
    IoReadFailed,
    IoWriteFailed,
};

// Numeric values match Parallax AttributeType for shared tooling (see ParallaxTypes.hxx).
enum class SmfAttributeType : uint8_t {
    Bool = 0,
    Int32 = 1,
    Int64 = 2,
    Float = 3,
    Double = 4,
    Vec2 = 5,
    Vec3 = 6,
    Vec4 = 7,
    Quaternion = 8,
    Matrix4 = 9,
    String = 10,
    AssetHash = 11,
    BlobOpaque = 12,
    ElementRef = 13,
    ArzachelSeed = 14,
    SkeletonPose = 15,
    EasingType = 16,
    ColorRGBA = 17,
    TransitionBlendMode = 18,
};

struct SmfVec2 {
    float x{};
    float y{};
};
struct SmfVec3 {
    float x{};
    float y{};
    float z{};
};
struct SmfVec4 {
    float x{};
    float y{};
    float z{};
    float w{};
};
struct SmfQuaternion {
    float x{};
    float y{};
    float z{};
    float w{};
};
struct SmfMatrix4 {
    std::array<std::array<float, 4>, 4> m{}; // row-major M[row][col]
};

using SmfValue = std::variant<
    std::monostate,
    bool,
    int32_t,
    int64_t,
    float,
    double,
    SmfVec2,
    SmfVec3,
    SmfVec4,
    SmfQuaternion,
    SmfMatrix4,
    std::string,
    uint64_t,
    std::vector<std::byte>,
    uint32_t,
    uint8_t>;

#pragma pack(push, 1)
struct SmfFileHeader {
    uint32_t Magic{SMF_MAGIC};
    uint16_t FormatVersionMajor{SMF_FORMAT_VERSION_MAJOR};
    uint16_t FormatVersionMinor{SMF_FORMAT_VERSION_MINOR};
    uint32_t Flags{0};
    uint32_t Reserved{0};
    uint32_t StringTableOffset{0};
    uint32_t StringTableSize{0};
    uint32_t GeometryOffset{0};
    uint32_t GeometrySize{0};
    uint32_t BspOffset{0};
    uint32_t BspSize{0};
    uint32_t EntityOffset{0};
    uint32_t EntitySize{0};
    uint32_t SectorOffset{0};
    uint32_t SectorSize{0};
    uint32_t PhysicsOffset{0};
    uint32_t PhysicsSize{0};
    uint32_t ScriptOffset{0};
    uint32_t ScriptSize{0};
    uint32_t TriggerOffset{0};
    uint32_t TriggerSize{0};
    uint32_t PathTableOffset{0};
    uint32_t PathTableSize{0};
    /// Optional ``SMAL`` gameplay extras blob (acoustic zones + authoring lights); zero when using legacy 88-byte on-disk headers only.
    uint32_t ExtrasOffset{0};
    uint32_t ExtrasSize{0};
};
#pragma pack(pop)

#pragma pack(push, 1)
struct SmfPathTableEntryDisk {
    uint32_t PathOffset{0};
    uint64_t AssetHash{0};
};
#pragma pack(pop)

} // namespace Solstice::Smf
