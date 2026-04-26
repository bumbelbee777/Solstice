#include <Parallax/ParallaxScene.hxx>

#include <algorithm>
#include <cstring>
#include <fstream>

namespace Solstice::Parallax {

static uint32_t FindSchemaIndex(const ParallaxScene& scene, std::string_view name) {
    const auto& schemas = scene.GetSchemas();
    for (uint32_t i = 0; i < schemas.size(); ++i) {
        if (schemas[i].TypeName == name) {
            return i;
        }
    }
    return PARALLAX_INVALID_INDEX;
}

static std::vector<SchemaDef> BuildBuiltinSchemaTable() {
    std::vector<SchemaDef> schemas;
    auto add = [&](const char* type, std::initializer_list<SchemaAttributeDef> attrs) {
        SchemaDef def;
        def.TypeName = type;
        def.Attributes.assign(attrs.begin(), attrs.end());
        schemas.push_back(std::move(def));
    };

    add("SceneRoot", {{"TickRate", AttributeType::Int32},
                      {"AmbientColor", AttributeType::ColorRGBA},
                      {"AmbientIntensity", AttributeType::Float},
                      {"SkyboxEnabled", AttributeType::Bool},
                      {"SkyboxBrightness", AttributeType::Float},
                      {"SkyboxYawDegrees", AttributeType::Float},
                      {"SkyboxFacePosX", AttributeType::String},
                      {"SkyboxFaceNegX", AttributeType::String},
                      {"SkyboxFacePosY", AttributeType::String},
                      {"SkyboxFaceNegY", AttributeType::String},
                      {"SkyboxFacePosZ", AttributeType::String},
                      {"SkyboxFaceNegZ", AttributeType::String}});
    add("CameraElement", {{"Position", AttributeType::Vec3},
                          {"Target", AttributeType::Vec3},
                          {"FovDegrees", AttributeType::Float},
                          {"Near", AttributeType::Float},
                          {"Far", AttributeType::Float},
                          {"ShakeAsset", AttributeType::AssetHash}});
    add("LightElement", {{"Position", AttributeType::Vec3},
                         {"Color", AttributeType::ColorRGBA},
                         {"Intensity", AttributeType::Float},
                         {"Radius", AttributeType::Float},
                         {"ConeAngle", AttributeType::Float},
                         {"CastShadows", AttributeType::Bool},
                         {"ShadowResolution", AttributeType::Int32}});
    add("ActorElement", {{"MeshAsset", AttributeType::AssetHash},
                         {"AnimationClip", AttributeType::AssetHash},
                         {"Position", AttributeType::Vec3},
                         {"ArzachelRigidBodyDamage", AttributeType::Float},
                         {"LodDistanceHigh", AttributeType::Float},
                         {"LodDistanceLow", AttributeType::Float},
                         {"ArzachelAnimationClipPreset", AttributeType::String},
                         {"ArzachelDestructionAnimPreset", AttributeType::String},
                         {"FacialVariationSeed", AttributeType::Int64},
                         {"EnableProceduralBlink", AttributeType::Bool},
                         {"EnableProceduralSaccade", AttributeType::Bool}});
    add("AudioSourceElement", {{"AudioAsset", AttributeType::AssetHash},
                               {"Volume", AttributeType::Float},
                               {"Pitch", AttributeType::Float}});
    add("MotionGraphicsRootElement", {{"CompositeAlpha", AttributeType::Float}});
    add("MGTextElement", {{"Text", AttributeType::String},
                          {"Position", AttributeType::Vec2},
                          {"Color", AttributeType::ColorRGBA},
                          {"Depth", AttributeType::Float}});
    add("MGSpriteElement", {{"Texture", AttributeType::AssetHash},
                            {"Position", AttributeType::Vec2},
                            {"Size", AttributeType::Vec2},
                            {"Depth", AttributeType::Float}});
    add("SmmParticleEmitterElement",
        {{"Enabled", AttributeType::Bool},
            {"AttachToSceneElement", AttributeType::Bool},
            {"AttachElementIndex", AttributeType::Int32},
            {"SpawnPerSec", AttributeType::Float},
            {"LifetimeSec", AttributeType::Float},
            {"VelMin", AttributeType::Vec3},
            {"VelMax", AttributeType::Vec3},
            {"StartSize", AttributeType::Float},
            {"EndSize", AttributeType::Float},
            {"Gravity", AttributeType::Vec3},
            {"LinearDrag", AttributeType::Float},
            {"MaxParticles", AttributeType::Int32},
            {"ColorStart", AttributeType::ColorRGBA},
            {"ColorEnd", AttributeType::ColorRGBA},
            {"UseImportedSprite", AttributeType::Bool},
            {"SpriteTexture", AttributeType::AssetHash},
            {"SpriteSourcePath", AttributeType::String}});
    add("SmmFluidVolumeElement",
        {{"Enabled", AttributeType::Bool},
            {"EnableMacCormack", AttributeType::Bool},
            {"EnableBoussinesq", AttributeType::Bool},
            {"VolumeVisualizationClip", AttributeType::Bool},
            {"BoundsMin", AttributeType::Vec3},
            {"BoundsMax", AttributeType::Vec3},
            {"ResolutionX", AttributeType::Int32},
            {"ResolutionY", AttributeType::Int32},
            {"ResolutionZ", AttributeType::Int32},
            {"Diffusion", AttributeType::Float},
            {"Viscosity", AttributeType::Float},
            {"ReferenceDensity", AttributeType::Float},
            {"PressureRelaxationIterations", AttributeType::Int32},
            {"BuoyancyStrength", AttributeType::Float},
            {"Prandtl", AttributeType::Float}});
    return schemas;
}

void RegisterBuiltinSchemas(ParallaxScene& scene) {
    auto& schemas = scene.GetSchemas();
    if (!schemas.empty()) {
        return;
    }
    schemas = BuildBuiltinSchemaTable();
}

void MergeBuiltinSchemaAttributes(ParallaxScene& scene) {
    const std::vector<SchemaDef> canonical = BuildBuiltinSchemaTable();
    for (const auto& c : canonical) {
        for (auto& s : scene.GetSchemas()) {
            if (s.TypeName != c.TypeName) {
                continue;
            }
            for (const auto& a : c.Attributes) {
                bool have = false;
                for (const auto& e : s.Attributes) {
                    if (e.Name == a.Name) {
                        have = true;
                        break;
                    }
                }
                if (!have) {
                    s.Attributes.push_back(a);
                }
            }
            break;
        }
    }
}

std::unique_ptr<ParallaxScene> CreateScene(uint32_t ticksPerSecond) {
    auto scene = std::make_unique<ParallaxScene>();
    scene->SetTicksPerSecond(ticksPerSecond);
    RegisterBuiltinSchemas(*scene);
    auto& els = scene->GetElements();
    els.clear();
    els.push_back({});
    els[0].SchemaIndex = 0;
    els[0].Name = "Root";
    els[0].Parent = PARALLAX_INVALID_INDEX;
    els[0].FirstChild = PARALLAX_INVALID_INDEX;
    els[0].NextSibling = PARALLAX_INVALID_INDEX;
    els[0].FirstChannelIndex = PARALLAX_INVALID_INDEX;
    els[0].ChannelCount = 0;
    return scene;
}

ElementIndex AddElement(ParallaxScene& scene, std::string_view schemaType, std::string_view name,
                        ElementIndex parent) {
    uint32_t si = FindSchemaIndex(scene, schemaType);
    if (si == PARALLAX_INVALID_INDEX) {
        return PARALLAX_INVALID_INDEX;
    }

    auto& els = scene.GetElements();
    ElementIndex newIdx = static_cast<ElementIndex>(els.size());
    ParallaxScene::ElementNode node;
    node.SchemaIndex = si;
    node.Name = std::string(name);
    node.Parent = parent;
    node.FirstChild = PARALLAX_INVALID_INDEX;
    node.NextSibling = PARALLAX_INVALID_INDEX;
    node.FirstChannelIndex = PARALLAX_INVALID_INDEX;
    node.ChannelCount = 0;
    els.push_back(std::move(node));

    if (parent != PARALLAX_INVALID_INDEX && parent < els.size()) {
        auto& p = els[parent];
        if (p.FirstChild == PARALLAX_INVALID_INDEX) {
            p.FirstChild = newIdx;
        } else {
            ElementIndex cur = p.FirstChild;
            while (els[cur].NextSibling != PARALLAX_INVALID_INDEX) {
                cur = els[cur].NextSibling;
            }
            els[cur].NextSibling = newIdx;
        }
    }

    return newIdx;
}

void SetAttribute(ParallaxScene& scene, ElementIndex element, std::string_view attribute, const AttributeValue& value) {
    auto& els = scene.GetElements();
    if (element >= els.size()) {
        return;
    }
    els[element].Attributes[std::string(attribute)] = value;
}

AttributeValue GetAttribute(const ParallaxScene& scene, ElementIndex element, std::string_view attribute) {
    const auto& els = scene.GetElements();
    if (element >= els.size()) {
        return std::monostate{};
    }
    const auto& m = els[element].Attributes;
    auto it = m.find(std::string(attribute));
    if (it == m.end()) {
        return std::monostate{};
    }
    return it->second;
}

ElementIndex FindElement(const ParallaxScene& scene, std::string_view name) {
    const auto& els = scene.GetElements();
    for (ElementIndex i = 0; i < els.size(); ++i) {
        if (els[i].Name == name) {
            return i;
        }
    }
    return PARALLAX_INVALID_INDEX;
}

std::string_view GetElementSchema(const ParallaxScene& scene, ElementIndex element) {
    const auto& els = scene.GetElements();
    const auto& schemas = scene.GetSchemas();
    if (element >= els.size()) {
        return {};
    }
    uint32_t si = els[element].SchemaIndex;
    if (si >= schemas.size()) {
        return {};
    }
    return schemas[si].TypeName;
}

void RegisterSchema(ParallaxScene& scene, std::string_view typeName,
                    std::initializer_list<SchemaAttributeDef> attributes) {
    if (FindSchemaIndex(scene, typeName) != PARALLAX_INVALID_INDEX) {
        return;
    }
    SchemaDef def;
    def.TypeName = std::string(typeName);
    def.Attributes.assign(attributes.begin(), attributes.end());
    scene.GetSchemas().push_back(std::move(def));
}

bool HasSchema(const ParallaxScene& scene, std::string_view typeName) {
    return FindSchemaIndex(scene, typeName) != PARALLAX_INVALID_INDEX;
}

ChannelIndex AddChannel(ParallaxScene& scene, ElementIndex element, std::string_view attribute, AttributeType type) {
    auto& els = scene.GetElements();
    if (element >= els.size()) {
        return PARALLAX_INVALID_INDEX;
    }
    ChannelRecord ch;
    ch.Element = element;
    ch.AttributeName = std::string(attribute);
    ch.ValueType = type;
    auto& channels = scene.GetChannels();
    ChannelIndex idx = static_cast<ChannelIndex>(channels.size());
    channels.push_back(std::move(ch));

    auto& en = els[element];
    if (en.ChannelCount == 0) {
        en.FirstChannelIndex = idx;
    }
    en.ChannelCount++;
    return idx;
}

void AddKeyframe(ParallaxScene& scene, ChannelIndex channel, uint64_t timeTicks, const AttributeValue& value,
                 EasingType easing) {
    auto& channels = scene.GetChannels();
    if (channel >= channels.size()) {
        return;
    }
    KeyframeRecord k;
    k.TimeTicks = timeTicks;
    k.Easing = static_cast<uint8_t>(easing);
    k.Flags = 0;
    k.EaseOut = 0xFF;
    k.Interp = static_cast<uint8_t>(KeyframeInterpolation::Eased);
    k.TangentIn = 1.f / 3.f;
    k.TangentOut = 1.f / 3.f;
    k.Value = value;
    auto& kfs = channels[channel].Keyframes;
    kfs.push_back(std::move(k));
    std::sort(kfs.begin(), kfs.end(), [](const KeyframeRecord& a, const KeyframeRecord& b) {
        return a.TimeTicks < b.TimeTicks;
    });
}

void RemoveKeyframe(ParallaxScene& scene, ChannelIndex channel, uint64_t timeTicks) {
    auto& channels = scene.GetChannels();
    if (channel >= channels.size()) {
        return;
    }
    auto& kfs = channels[channel].Keyframes;
    kfs.erase(std::remove_if(kfs.begin(), kfs.end(),
                             [timeTicks](const KeyframeRecord& k) { return k.TimeTicks == timeTicks; }),
              kfs.end());
}

void SetKeyframeEasing(ParallaxScene& scene, ChannelIndex channel, uint64_t timeTicks, EasingType easing) {
    auto& channels = scene.GetChannels();
    if (channel >= channels.size()) {
        return;
    }
    for (auto& k : channels[channel].Keyframes) {
        if (k.TimeTicks == timeTicks) {
            k.Easing = static_cast<uint8_t>(easing);
            return;
        }
    }
}

void SetKeyframeEaseOut(ParallaxScene& scene, ChannelIndex channel, uint64_t timeTicks, uint8_t easeOutOr0xFF) {
    auto& channels = scene.GetChannels();
    if (channel >= channels.size()) {
        return;
    }
    for (auto& k : channels[channel].Keyframes) {
        if (k.TimeTicks == timeTicks) {
            k.EaseOut = easeOutOr0xFF;
            return;
        }
    }
}

void SetKeyframeInterpolation(ParallaxScene& scene, ChannelIndex channel, uint64_t timeTicks, KeyframeInterpolation mode) {
    auto& channels = scene.GetChannels();
    if (channel >= channels.size()) {
        return;
    }
    for (auto& k : channels[channel].Keyframes) {
        if (k.TimeTicks == timeTicks) {
            k.Interp = static_cast<uint8_t>(mode);
            return;
        }
    }
}

void SetKeyframeBezierTangents(ParallaxScene& scene, ChannelIndex channel, uint64_t timeTicks, float tangentOut, float tangentIn) {
    auto& channels = scene.GetChannels();
    if (channel >= channels.size()) {
        return;
    }
    for (auto& k : channels[channel].Keyframes) {
        if (k.TimeTicks == timeTicks) {
            k.TangentOut = tangentOut;
            k.TangentIn = tangentIn;
            return;
        }
    }
}

MGIndex AddMGElement(ParallaxScene& scene, std::string_view schemaType, std::string_view name, MGIndex parent) {
    uint32_t si = FindSchemaIndex(scene, schemaType);
    if (si == PARALLAX_INVALID_INDEX) {
        return PARALLAX_INVALID_INDEX;
    }
    auto& mgs = scene.GetMGElements();
    MGIndex idx = static_cast<MGIndex>(mgs.size());
    MGElementRecord rec;
    rec.SchemaIndex = si;
    rec.Name = std::string(name);
    rec.Parent = parent;
    rec.FirstChild = PARALLAX_INVALID_INDEX;
    rec.NextSibling = PARALLAX_INVALID_INDEX;
    if (parent != PARALLAX_INVALID_INDEX && parent < mgs.size()) {
        auto& p = mgs[parent];
        if (p.FirstChild == PARALLAX_INVALID_INDEX) {
            p.FirstChild = idx;
        } else {
            MGIndex cur = p.FirstChild;
            while (mgs[cur].NextSibling != PARALLAX_INVALID_INDEX) {
                cur = mgs[cur].NextSibling;
            }
            mgs[cur].NextSibling = idx;
        }
    }
    mgs.push_back(std::move(rec));
    return idx;
}

uint32_t AddMGTrack(ParallaxScene& scene, MGIndex element, std::string_view property, AttributeType type,
                    EasingType defaultEasing) {
    auto& mgs = scene.GetMGElements();
    if (element >= mgs.size()) {
        return PARALLAX_INVALID_INDEX;
    }
    MGTrackRecord tr;
    tr.PropertyName = std::string(property);
    tr.ValueType = type;
    tr.EasingType = static_cast<uint8_t>(defaultEasing);
    auto& tracks = scene.GetMGTracks();
    uint32_t ti = static_cast<uint32_t>(tracks.size());
    tracks.push_back(std::move(tr));
    auto& el = mgs[element];
    if (el.TrackCount == 0) {
        el.FirstTrackIndex = ti;
    }
    el.TrackCount++;
    return ti;
}

void AddMGKeyframe(ParallaxScene& scene, uint32_t trackIndex, uint64_t timeTicks, const AttributeValue& value,
                   EasingType easing) {
    auto& tracks = scene.GetMGTracks();
    if (trackIndex >= tracks.size()) {
        return;
    }
    KeyframeRecord k;
    k.TimeTicks = timeTicks;
    k.Easing = static_cast<uint8_t>(easing);
    k.Flags = 0;
    k.EaseOut = 0xFF;
    k.Interp = static_cast<uint8_t>(KeyframeInterpolation::Eased);
    k.TangentIn = 1.f / 3.f;
    k.TangentOut = 1.f / 3.f;
    k.Value = value;
    auto& kfs = tracks[trackIndex].Keyframes;
    kfs.push_back(std::move(k));
    std::sort(kfs.begin(), kfs.end(), [](const KeyframeRecord& a, const KeyframeRecord& b) {
        return a.TimeTicks < b.TimeTicks;
    });
}

void RemoveMGKeyframe(ParallaxScene& scene, uint32_t trackIndex, uint64_t timeTicks) {
    auto& tracks = scene.GetMGTracks();
    if (trackIndex >= tracks.size()) {
        return;
    }
    auto& kfs = tracks[trackIndex].Keyframes;
    kfs.erase(std::remove_if(kfs.begin(), kfs.end(),
                             [timeTicks](const KeyframeRecord& k) { return k.TimeTicks == timeTicks; }),
        kfs.end());
}

void SetMGKeyframeEasing(ParallaxScene& scene, uint32_t trackIndex, uint64_t timeTicks, EasingType easing) {
    auto& tracks = scene.GetMGTracks();
    if (trackIndex >= tracks.size()) {
        return;
    }
    for (auto& k : tracks[trackIndex].Keyframes) {
        if (k.TimeTicks == timeTicks) {
            k.Easing = static_cast<uint8_t>(easing);
            return;
        }
    }
}

void SetMGKeyframeEaseOut(ParallaxScene& scene, uint32_t trackIndex, uint64_t timeTicks, uint8_t easeOutOr0xFF) {
    auto& tracks = scene.GetMGTracks();
    if (trackIndex >= tracks.size()) {
        return;
    }
    for (auto& k : tracks[trackIndex].Keyframes) {
        if (k.TimeTicks == timeTicks) {
            k.EaseOut = easeOutOr0xFF;
            return;
        }
    }
}

void SetMGKeyframeInterpolation(ParallaxScene& scene, uint32_t trackIndex, uint64_t timeTicks, KeyframeInterpolation mode) {
    auto& tracks = scene.GetMGTracks();
    if (trackIndex >= tracks.size()) {
        return;
    }
    for (auto& k : tracks[trackIndex].Keyframes) {
        if (k.TimeTicks == timeTicks) {
            k.Interp = static_cast<uint8_t>(mode);
            return;
        }
    }
}

void SetMGKeyframeBezierTangents(ParallaxScene& scene, uint32_t trackIndex, uint64_t timeTicks, float tangentOut, float tangentIn) {
    auto& tracks = scene.GetMGTracks();
    if (trackIndex >= tracks.size()) {
        return;
    }
    for (auto& k : tracks[trackIndex].Keyframes) {
        if (k.TimeTicks == timeTicks) {
            k.TangentOut = tangentOut;
            k.TangentIn = tangentIn;
            return;
        }
    }
}

void SetMGCompositeMode(ParallaxScene& scene, BlendMode mode) {
    scene.SetMGCompositeMode(mode);
}

void SetMGGlobalAlpha(ParallaxScene& scene, float alpha) {
    scene.SetMGGlobalAlpha(alpha);
}

ChannelIndex GetMGGlobalAlphaChannel(ParallaxScene& scene) {
    return scene.GetMGGlobalAlphaChannelIndex();
}

} // namespace Solstice::Parallax
