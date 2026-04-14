#pragma once

#include <MinGfx/EasingFunction.hxx>
#include <Math/Matrix.hxx>
#include <Math/Quaternion.hxx>
#include <Math/Vector.hxx>
#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace Solstice::Parallax {

class IAssetResolver;

constexpr uint32_t PARALLAX_MAGIC = 0x50524C58u; // 'PRLX'
constexpr uint16_t PARALLAX_FORMAT_VERSION_MAJOR = 1;
constexpr uint16_t PARALLAX_FORMAT_VERSION_MINOR = 1; // v1.1: ZSTD compresses tail only (after 92-byte header)
constexpr uint32_t PARALLAX_INVALID_INDEX = 0xFFFFFFFFu;

enum class ParallaxError {
    None,
    InvalidMagic,
    UnsupportedVersion,
    CorruptHeader,
    CorruptStringTable,
    CorruptElementGraph,
    CorruptChannelData,
    CorruptMotionGraphics,
    AssetResolutionFailed,
    VMVersionMismatch,
    StreamingError,
    OutOfMemory,
};

enum class AttributeType : uint8_t {
    Bool,
    Int32,
    Int64,
    Float,
    Double,
    Vec2,
    Vec3,
    Vec4,
    Quaternion,
    Matrix4,
    String,
    AssetHash,
    BlobOpaque,
    ElementRef,
    ArzachelSeed,
    SkeletonPose,
    EasingType,
    ColorRGBA,
    TransitionBlendMode,
};

enum class BlendMode : uint8_t {
    Over,
    Additive,
    Multiply,
};

using ElementIndex = uint32_t;
using ChannelIndex = uint32_t;
using LayerIndex = uint32_t;
using MGIndex = uint32_t;

using EasingType = MinGfx::EasingType;

using AttributeValue = std::variant<
    std::monostate,
    bool,
    int32_t,
    int64_t,
    float,
    double,
    Math::Vec2,
    Math::Vec3,
    Math::Vec4,
    Math::Quaternion,
    Math::Matrix4,
    std::string,
    uint64_t,
    std::vector<std::byte>,
    ElementIndex,
    EasingType,
    BlendMode>;

struct AssetData {
    std::vector<std::byte> Bytes;
    uint16_t HintType{0};
};

struct KeyframeHeader {
    uint64_t TimeTicks{0};
    uint8_t EasingType{0};
    uint8_t Flags{0};
    uint16_t Reserved{0};
};

#pragma pack(push, 1)

struct FileHeader {
    uint32_t Magic{PARALLAX_MAGIC};
    uint16_t FormatVersionMajor{PARALLAX_FORMAT_VERSION_MAJOR};
    uint16_t FormatVersionMinor{PARALLAX_FORMAT_VERSION_MINOR};
    uint64_t TimelineDurationTicks{0};
    uint32_t TicksPerSecond{6000};
    uint32_t StringTableOffset{0};
    uint32_t StringTableSize{0};
    uint32_t SchemaRegistryOffset{0};
    uint32_t SchemaRegistrySize{0};
    uint32_t ElementGraphOffset{0};
    uint32_t ElementGraphSize{0};
    uint32_t ChannelDataIndexOffset{0};
    uint32_t ChannelDataBlobOffset{0};
    uint32_t ChannelDataBlobSize{0};
    uint32_t MotionGraphicsSectionOffset{0};
    uint32_t MotionGraphicsSectionSize{0};
    uint32_t AudioMixOffset{0};
    uint32_t PhysicsSnapshotOffset{0};
    uint32_t ScriptSnapshotOffset{0};
    uint32_t RenderJobTableOffset{0};
    uint32_t RenderJobCount{0};
    uint32_t PathTableOffset{0};
    uint16_t Flags{0};
    uint8_t ReservedPad[2]{0, 0};
};

// Packed size is 92 bytes (all section offsets + sizes). v1 on-disk layout; keep #pragma pack(1).
static_assert(sizeof(FileHeader) == 92, "FileHeader packed layout");

struct AttributeDescriptorDisk {
    uint32_t NameOffset{0};
    uint8_t Type{0};
    uint8_t Flags{0};
    uint16_t Reserved{0};
};

struct SchemaEntryDisk {
    uint32_t TypeNameOffset{0};
    uint32_t AttributeCount{0};
};

struct ElementHeaderDisk {
    uint32_t SchemaIndex{0};
    uint32_t NameOffset{0};
    uint32_t ParentIndex{0};
    uint32_t FirstChildIndex{0};
    uint32_t NextSiblingIndex{0};
    uint32_t AttributeDataOffset{0};
    uint32_t ChannelCount{0};
    uint32_t FirstChannelIndex{0};
};

struct ChannelIndexEntryDisk {
    uint32_t ElementIndex{0};
    uint32_t AttributeNameOffset{0};
    uint8_t ValueType{0};
    uint8_t CompressionType{0};
    uint16_t LayerIndex{0};
    uint64_t DataOffset{0};
    uint32_t KeyframeCount{0};
    uint32_t ChunkCount{0};
};

struct MotionGraphicsSectionHeaderDisk {
    uint32_t ElementCount{0};
    uint32_t TrackCount{0};
    uint32_t CompositeMode{0};
    float GlobalAlpha{1.0f};
    uint32_t GlobalAlphaChannelIndex{PARALLAX_INVALID_INDEX};
    uint32_t Reserved[3]{0, 0, 0};
};

struct MGElementEntryDisk {
    uint32_t SchemaIndex{0};
    uint32_t NameOffset{0};
    uint32_t ParentMGIndex{0};
    uint32_t FirstChildMGIndex{0};
    uint32_t NextSiblingMGIndex{0};
    uint32_t AttributeDataOffset{0};
    uint32_t FirstTrackIndex{0};
    uint32_t TrackCount{0};
};

struct MGTrackEntryDisk {
    uint32_t PropertyNameOffset{0};
    uint8_t ValueType{0};
    uint8_t EasingType{0};
    uint8_t CompressionType{0};
    uint8_t Flags{0};
    uint32_t KeyframeCount{0};
    uint64_t DataOffset{0};
};

struct PhysicsSnapshotHeaderDisk {
    uint32_t SnapshotCount{0};
    uint32_t IntervalTicks{0};
};

struct ScriptSnapshotHeaderDisk {
    uint32_t SnapshotCount{0};
    uint32_t IntervalTicks{0};
    uint16_t VMVersionMajor{0};
    uint16_t VMVersionMinor{0};
};

struct PathTableEntryDisk {
    uint32_t PathOffset{0};
    uint64_t AssetHash{0};
};

#pragma pack(pop)

enum class ChannelCompression : uint8_t {
    None = 0,
    DeltaFloat = 1,
    DeltaQuat = 2,
    FixedPoint = 3,
};

enum class OutputFormat : uint32_t {
    PNGSequence = 0,
    EXRSequence = 1,
    RawFrames = 2,
};

struct RenderJobParams {
    uint32_t Width{1920};
    uint32_t Height{1080};
    uint64_t StartTick{0};
    uint64_t EndTick{0};
    uint32_t FrameRate{60};
    OutputFormat Format{OutputFormat::PNGSequence};
    std::filesystem::path OutputPath;
    bool OfflineMode{true};
    bool CompositeMG{true};
    /** When non-null, MGSpriteElement textures are resolved for CPU rasterization. */
    IAssetResolver* AssetResolver{nullptr};
};

struct ElementTransform {
    ElementIndex Element{PARALLAX_INVALID_INDEX};
    Math::Vec3 Position{0, 0, 0};
    Math::Quaternion Rotation{1, 0, 0, 0};
    Math::Vec3 Scale{1, 1, 1};
};

struct LightState {
    ElementIndex Element{PARALLAX_INVALID_INDEX};
    Math::Vec3 Position{0, 0, 0};
    Math::Vec4 Color{1, 1, 1, 1};
    float Intensity{1.0f};
};

struct AudioSourceState {
    ElementIndex Element{PARALLAX_INVALID_INDEX};
    float Volume{1.0f};
    float Pitch{1.0f};
};

struct MGDisplayList {
    struct Entry {
        std::string SchemaType;
        std::unordered_map<std::string, AttributeValue> Attributes;
        BlendMode Blend{BlendMode::Over};
        float Alpha{1.0f};
        std::vector<Entry> Children;
    };
    std::vector<Entry> Entries;
    BlendMode CompositeMode{BlendMode::Over};
    float GlobalAlpha{1.0f};
};

struct SceneEvaluationResult {
    std::vector<ElementTransform> ElementTransforms;
    std::vector<LightState> LightStates;
    std::vector<AudioSourceState> AudioStates;
    MGDisplayList MotionGraphics;
    std::unordered_map<std::string, AttributeValue> ScriptOutputs;
};

using RenderProgressCallback = std::function<void(uint32_t frame, uint32_t totalFrames, float percentComplete)>;

} // namespace Solstice::Parallax
