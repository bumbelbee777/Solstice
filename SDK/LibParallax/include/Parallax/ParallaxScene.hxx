#pragma once

#include "ParallaxTypes.hxx"
#include "IAssetResolver.hxx"

namespace Solstice {
namespace Physics {
class PhysicsSystem;
}
namespace ECS {
class Registry;
}
namespace Scripting {
class BytecodeVM;
}
namespace Render {
class SoftwareRenderer;
}
} // namespace Solstice
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace Solstice::Parallax {

struct SchemaAttributeDef {
    std::string Name;
    AttributeType Type{AttributeType::Float};
    uint8_t Flags{0};
};

struct SchemaDef {
    std::string TypeName;
    std::vector<SchemaAttributeDef> Attributes;
};

struct KeyframeRecord {
    uint64_t TimeTicks{0};
    uint8_t Easing{0};
    uint8_t Flags{0};
    uint8_t EaseOut{0xFF}; ///< 0xFF: incoming segment to *next* key uses next key’s `Easing` for parametric part.
    uint8_t Interp{0}; ///< `KeyframeInterpolation` (hold / linear / bezier / eased).
    float TangentIn{0.333f}; ///< Bezier: weight on value delta toward previous side (0..2 typical).
    float TangentOut{0.333f}; ///< Bezier: weight toward next side. Ignored for non-Bezier and non-float.
    AttributeValue Value;
};

struct ChannelRecord {
    ElementIndex Element{0};
    std::string AttributeName;
    AttributeType ValueType{AttributeType::Float};
    ChannelCompression Compression{ChannelCompression::None};
    LayerIndex Layer{PARALLAX_INVALID_INDEX};
    std::vector<KeyframeRecord> Keyframes;
};

struct MGElementRecord {
    uint32_t SchemaIndex{0};
    std::string Name;
    MGIndex Parent{PARALLAX_INVALID_INDEX};
    MGIndex FirstChild{PARALLAX_INVALID_INDEX};
    MGIndex NextSibling{PARALLAX_INVALID_INDEX};
    std::unordered_map<std::string, AttributeValue> Attributes;
    uint32_t FirstTrackIndex{0};
    uint32_t TrackCount{0};
};

struct MGTrackRecord {
    std::string PropertyName;
    AttributeType ValueType{AttributeType::Float};
    uint8_t EasingType{0};
    ChannelCompression Compression{ChannelCompression::None};
    uint8_t Flags{0};
    std::vector<KeyframeRecord> Keyframes;
};

class ParallaxScene {
public:
    struct ElementNode {
        uint32_t SchemaIndex{0};
        std::string Name;
        ElementIndex Parent{PARALLAX_INVALID_INDEX};
        ElementIndex FirstChild{PARALLAX_INVALID_INDEX};
        ElementIndex NextSibling{PARALLAX_INVALID_INDEX};
        std::unordered_map<std::string, AttributeValue> Attributes;
        ChannelIndex FirstChannelIndex{PARALLAX_INVALID_INDEX};
        uint32_t ChannelCount{0};
    };

    ParallaxScene() = default;
    ~ParallaxScene() = default;
    ParallaxScene(const ParallaxScene&) = delete;
    ParallaxScene& operator=(const ParallaxScene&) = delete;
    ParallaxScene(ParallaxScene&&) = default;
    ParallaxScene& operator=(ParallaxScene&&) = default;

    uint32_t GetTicksPerSecond() const { return m_TicksPerSecond; }
    void SetTicksPerSecond(uint32_t t) { m_TicksPerSecond = t; }
    uint64_t GetTimelineDurationTicks() const { return m_TimelineDurationTicks; }
    void SetTimelineDurationTicks(uint64_t d) { m_TimelineDurationTicks = d; }

    const std::vector<SchemaDef>& GetSchemas() const { return m_Schemas; }
    std::vector<SchemaDef>& GetSchemas() { return m_Schemas; }

    const std::vector<ChannelRecord>& GetChannels() const { return m_Channels; }
    std::vector<ChannelRecord>& GetChannels() { return m_Channels; }

    const std::unordered_map<std::string, uint64_t>& GetPathTable() const { return m_PathTable; }
    std::unordered_map<std::string, uint64_t>& GetPathTable() { return m_PathTable; }

    BlendMode GetMGCompositeMode() const { return m_MGCompositeMode; }
    void SetMGCompositeMode(BlendMode m) { m_MGCompositeMode = m; }
    float GetMGGlobalAlpha() const { return m_MGGlobalAlpha; }
    void SetMGGlobalAlpha(float a) { m_MGGlobalAlpha = a; }
    ChannelIndex GetMGGlobalAlphaChannelIndex() const { return m_MGGlobalAlphaChannelIndex; }
    void SetMGGlobalAlphaChannelIndex(ChannelIndex i) { m_MGGlobalAlphaChannelIndex = i; }

    const std::vector<MGElementRecord>& GetMGElements() const { return m_MGElements; }
    std::vector<MGElementRecord>& GetMGElements() { return m_MGElements; }
    const std::vector<MGTrackRecord>& GetMGTracks() const { return m_MGTracks; }
    std::vector<MGTrackRecord>& GetMGTracks() { return m_MGTracks; }

    const std::vector<ElementNode>& GetElements() const { return m_Elements; }
    std::vector<ElementNode>& GetElements() { return m_Elements; }

private:
    friend class ParallaxStreamReader;

    uint32_t m_TicksPerSecond{6000};
    uint64_t m_TimelineDurationTicks{0};
    std::vector<SchemaDef> m_Schemas;

    std::vector<ElementNode> m_Elements;

    std::vector<ChannelRecord> m_Channels;

    BlendMode m_MGCompositeMode{BlendMode::Over};
    float m_MGGlobalAlpha{1.0f};
    ChannelIndex m_MGGlobalAlphaChannelIndex{PARALLAX_INVALID_INDEX};
    std::vector<MGElementRecord> m_MGElements;
    std::vector<MGTrackRecord> m_MGTracks;

    std::unordered_map<std::string, uint64_t> m_PathTable;

    uint16_t m_FileFlags{0};
};

void RegisterBuiltinSchemas(ParallaxScene& scene);
/// Merges missing attribute definitions from the current built-in table into schemas loaded from disk (v1.2+ fields on older .prlx).
void MergeBuiltinSchemaAttributes(ParallaxScene& scene);

bool SaveSceneToBytes(const ParallaxScene& scene, std::vector<std::byte>& out, bool compressWhole, ParallaxError* err);
bool LoadSceneFromBytes(ParallaxScene& scene, std::span<const std::byte> data, ParallaxError* err = nullptr);

std::unique_ptr<ParallaxScene> CreateScene(uint32_t ticksPerSecond = 6000);

std::unique_ptr<ParallaxScene> LoadScene(const std::filesystem::path& path, IAssetResolver* resolver = nullptr,
                                         ParallaxError* outError = nullptr);

bool SaveScene(const ParallaxScene& scene, const std::filesystem::path& path, bool compressWhole = false,
               ParallaxError* outError = nullptr);

ElementIndex AddElement(ParallaxScene& scene, std::string_view schemaType, std::string_view name,
                        ElementIndex parent = PARALLAX_INVALID_INDEX);

void SetAttribute(ParallaxScene& scene, ElementIndex element, std::string_view attribute, const AttributeValue& value);

AttributeValue GetAttribute(const ParallaxScene& scene, ElementIndex element, std::string_view attribute);

ElementIndex FindElement(const ParallaxScene& scene, std::string_view name);

std::string_view GetElementSchema(const ParallaxScene& scene, ElementIndex element);

void RegisterSchema(ParallaxScene& scene, std::string_view typeName,
                    std::initializer_list<SchemaAttributeDef> attributes);

bool HasSchema(const ParallaxScene& scene, std::string_view typeName);

ChannelIndex AddChannel(ParallaxScene& scene, ElementIndex element, std::string_view attribute, AttributeType type);

void AddKeyframe(ParallaxScene& scene, ChannelIndex channel, uint64_t timeTicks, const AttributeValue& value,
                 EasingType easing = EasingType::Linear);

void RemoveKeyframe(ParallaxScene& scene, ChannelIndex channel, uint64_t timeTicks);

void SetKeyframeEasing(ParallaxScene& scene, ChannelIndex channel, uint64_t timeTicks, EasingType easing);

void SetKeyframeEaseOut(ParallaxScene& scene, ChannelIndex channel, uint64_t timeTicks, uint8_t easeOutOr0xFF);

void SetKeyframeInterpolation(ParallaxScene& scene, ChannelIndex channel, uint64_t timeTicks, KeyframeInterpolation mode);

void SetKeyframeBezierTangents(ParallaxScene& scene, ChannelIndex channel, uint64_t timeTicks, float tangentOut, float tangentIn);

MGIndex AddMGElement(ParallaxScene& scene, std::string_view schemaType, std::string_view name,
                     MGIndex parent = PARALLAX_INVALID_INDEX);

uint32_t AddMGTrack(ParallaxScene& scene, MGIndex element, std::string_view property, AttributeType type,
                    EasingType defaultEasing = EasingType::EaseInOut);

void AddMGKeyframe(ParallaxScene& scene, uint32_t trackIndex, uint64_t timeTicks, const AttributeValue& value,
                   EasingType easing);

void RemoveMGKeyframe(ParallaxScene& scene, uint32_t trackIndex, uint64_t timeTicks);

void SetMGKeyframeEasing(ParallaxScene& scene, uint32_t trackIndex, uint64_t timeTicks, EasingType easing);

void SetMGKeyframeEaseOut(ParallaxScene& scene, uint32_t trackIndex, uint64_t timeTicks, uint8_t easeOutOr0xFF);

void SetMGKeyframeInterpolation(ParallaxScene& scene, uint32_t trackIndex, uint64_t timeTicks, KeyframeInterpolation mode);

void SetMGKeyframeBezierTangents(ParallaxScene& scene, uint32_t trackIndex, uint64_t timeTicks, float tangentOut, float tangentIn);

void SetMGCompositeMode(ParallaxScene& scene, BlendMode mode);
void SetMGGlobalAlpha(ParallaxScene& scene, float alpha);
ChannelIndex GetMGGlobalAlphaChannel(ParallaxScene& scene);

void EvaluateScene(const ParallaxScene& scene, uint64_t timeTicks, SceneEvaluationResult& outResult);

AttributeValue EvaluateChannel(const ParallaxScene& scene, ChannelIndex channel, uint64_t timeTicks);

MGDisplayList EvaluateMG(const ParallaxScene& scene, uint64_t timeTicks);

void RestorePhysicsSnapshot(const ParallaxScene& scene, uint64_t timeTicks, Solstice::Physics::PhysicsSystem& physics,
                            Solstice::ECS::Registry& registry);

void RestoreScriptSnapshot(const ParallaxScene& scene, uint64_t timeTicks, Solstice::Scripting::BytecodeVM& vm);

void ExecuteRenderJob(const ParallaxScene& scene, const RenderJobParams& params, Solstice::Render::SoftwareRenderer& renderer,
                      RenderProgressCallback progress = nullptr);

} // namespace Solstice::Parallax
