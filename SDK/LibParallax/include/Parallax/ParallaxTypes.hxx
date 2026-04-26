#pragma once

#include <MinGfx/EasingFunction.hxx>
#include <Math/Matrix.hxx>
#include <Math/Quaternion.hxx>
#include <Math/Vector.hxx>
#include <Skeleton/Skeleton.hxx>
#include <array>
#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <functional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace Solstice::Parallax {

class IAssetResolver;

/// Timeline channel attribute names for `EvaluateScene` facial sampling (additive ActorElement channels).
inline constexpr const char* kChannelFacialMoodName = "FacialMoodName";
inline constexpr const char* kChannelFacialMoodWeight = "FacialMoodWeight";
inline constexpr const char* kChannelFacialVisemeId = "FacialVisemeId";
inline constexpr const char* kChannelFacialVisemeWeight = "FacialVisemeWeight";

constexpr uint32_t PARALLAX_MAGIC = 0x50524C58u; // 'PRLX'
constexpr uint16_t PARALLAX_FORMAT_VERSION_MAJOR = 1;
constexpr uint16_t PARALLAX_FORMAT_VERSION_MINOR = 0;
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

/// Parallax per-keyframe **ease-in** (segment ending at this key) matches historical `Easing` byte.
enum class KeyframeInterpolation : uint8_t {
    Eased = 0, ///< Parametric `Ease` on normalized time, then lerp.
    Hold = 1,  ///< Value stays at previous key until this key’s tick (float/vec3/color).
    Linear = 2, ///< Lerp, ignoring parametric Ease.
    Bezier = 3 ///< 1D value-space cubic; tangents in `KeyframeRecord` (float channels only; vec uses eased lerp as fallback).
};

#pragma pack(push, 1)
struct KeyframeHeader {
    uint64_t TimeTicks{0};
    uint8_t EasingType{0};
    uint8_t Flags{0};
    uint8_t EaseOut{0xFF}; ///< Outgoing ease on the segment *after* this key (0xFF = inherit: use the next key’s EaseIn at runtime).
    uint8_t Interp{0};
};
static_assert(sizeof(KeyframeHeader) == 12, "keyframe header disk layout");
#pragma pack(pop)

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

/** Sort key for root `MGDisplayList::Entries` when compositing (MovieMaker CPU raster + ImGui compositor). */
inline float MGEntryCompositeDepth(const MGDisplayList::Entry& e) {
    auto it = e.Attributes.find("Depth");
    if (it == e.Attributes.end()) {
        return 0.f;
    }
    if (const auto* f = std::get_if<float>(&it->second)) {
        return *f;
    }
    return 0.f;
}

/// Matches `SmmFluidVolumeElement` for viewport overlay / authoring; interior cell count should stay within `kParallaxFluidInteriorCellBudget` when possible.
struct FluidVolumeState {
    ElementIndex Element{PARALLAX_INVALID_INDEX};
    std::string Name;
    bool Enabled{true};
    bool EnableMacCormack{true};
    bool EnableBoussinesq{false};
    bool VolumeVisualizationClip{false};
    Math::Vec3 BoundsMin{};
    Math::Vec3 BoundsMax{1.f, 1.f, 1.f};
    int Nx{32};
    int Ny{32};
    int Nz{32};
    float Diffusion{0.0001f};
    float Viscosity{0.0001f};
    float ReferenceDensity{1000.f};
    int PressureRelaxationIterations{32};
    float BuoyancyStrength{1.f};
    float Prandtl{0.71f};
};

/// Same budget as Jackhammer’s SMF fluid authoring (see `Smf::kSmfFluidInteriorCellBudget` in the map SDK).
inline constexpr int64_t kParallaxFluidInteriorCellBudget = 262144;

/// Parallax `SceneRoot` sky cubemap (matches Jackhammer `.smf` / `SmfSkybox` path semantics: +X, −X, +Y, −Y, +Z, −Z).
struct SkyboxAuthoringState {
    bool Enabled{false};
    float Brightness{1.f};
    /// World Y-up: rotates the skybox around +Y (degrees).
    float YawDegrees{0.f};
    std::array<std::string, 6> FacePaths{};
};

/// Evaluated facial pose for one actor (named bone deltas + logical morph weights). Runtime maps names to rig bones.
struct ActorFacialPose {
    ElementIndex Element{PARALLAX_INVALID_INDEX};
    std::unordered_map<std::string, Solstice::Skeleton::BoneTransform> BoneDeltasByName;
    /// Hash ids align with `Solstice::Arzachel::MorphNameHash` for string keys.
    std::unordered_map<uint32_t, float> MorphWeights;
};

/// Rigid-mesh + Arzachel variation hooks authored on `ActorElement` (runtime may feed `Arzachel::Damaged`, LOD, or clip names).
struct ActorArzachelAuthoring {
    ElementIndex Element{PARALLAX_INVALID_INDEX};
    /// 0 = none; (0,1] drives `Arzachel::Damaged(baseMesh, seed, amount)`-style destruction for rigid props.
    float RigidBodyDamage{0.f};
    /// World-space distance beyond which a coarser representation may be used (0 = engine default / unused).
    float LodDistanceHigh{0.f};
    /// Maximum draw distance for this actor (0 = engine default / unused).
    float LodDistanceLow{0.f};
    /// Free-form animation preset label (pairs with `AnimationClip` / engine runners).
    std::string AnimationClipPreset;
    /// Label for a paired destruction / breakup animation or Arzachel recipe (authoring; engine-defined).
    std::string DestructionAnimPreset;
};

struct SceneEvaluationResult {
    std::vector<ElementTransform> ElementTransforms;
    std::vector<LightState> LightStates;
    std::vector<AudioSourceState> AudioStates;
    /// Populated for `SmmFluidVolumeElement` rows (MovieMaker + runtime visualization).
    std::vector<FluidVolumeState> FluidVolumes;
    /// Filled from `SceneRoot` on element 0 when the schema is `SceneRoot` (SMM + exporters).
    std::optional<SkyboxAuthoringState> EnvironmentSkybox;
    /// One row per `ActorElement` with Arzachel / LOD / preset fields (for tooling and runtime).
    std::vector<ActorArzachelAuthoring> ActorArzachelAuthoring;
    /// One row per `ActorElement` in element order when facial channels are present or proc flags are set.
    std::vector<ActorFacialPose> ActorFacialPoses;
    MGDisplayList MotionGraphics;
    std::unordered_map<std::string, AttributeValue> ScriptOutputs;
};

using RenderProgressCallback = std::function<void(uint32_t frame, uint32_t totalFrames, float percentComplete)>;

} // namespace Solstice::Parallax
