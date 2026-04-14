#include <Parallax/ParallaxScene.hxx>

#include <Smf/SmfTypes.hxx>
#include <Smf/SmfWire.hxx>

#include <cstring>
#include <fstream>
#include <zstd.h>

namespace Solstice::Parallax {

namespace {

static_assert(static_cast<uint8_t>(AttributeType::TransitionBlendMode) ==
              static_cast<uint8_t>(Solstice::Smf::SmfAttributeType::TransitionBlendMode));

Solstice::Smf::SmfAttributeType ToSmf(AttributeType t) {
    return static_cast<Solstice::Smf::SmfAttributeType>(static_cast<uint8_t>(t));
}

Solstice::Smf::SmfValue ToSmfValue(AttributeType t, const AttributeValue& v) {
    using namespace Solstice::Smf;
    switch (t) {
    case AttributeType::Bool:
        return std::get_if<bool>(&v) && *std::get_if<bool>(&v);
    case AttributeType::Int32:
        return std::get_if<int32_t>(&v) ? *std::get_if<int32_t>(&v) : 0;
    case AttributeType::Int64:
        return std::get_if<int64_t>(&v) ? *std::get_if<int64_t>(&v) : int64_t{0};
    case AttributeType::Float:
        return std::get_if<float>(&v) ? *std::get_if<float>(&v) : 0.f;
    case AttributeType::Double:
        return std::get_if<double>(&v) ? *std::get_if<double>(&v) : 0.0;
    case AttributeType::Vec2: {
        Math::Vec2 a = std::get_if<Math::Vec2>(&v) ? *std::get_if<Math::Vec2>(&v) : Math::Vec2{};
        return SmfVec2{a.x, a.y};
    }
    case AttributeType::Vec3: {
        Math::Vec3 a = std::get_if<Math::Vec3>(&v) ? *std::get_if<Math::Vec3>(&v) : Math::Vec3{};
        return SmfVec3{a.x, a.y, a.z};
    }
    case AttributeType::Vec4:
    case AttributeType::ColorRGBA: {
        Math::Vec4 a = std::get_if<Math::Vec4>(&v) ? *std::get_if<Math::Vec4>(&v) : Math::Vec4{};
        return SmfVec4{a.x, a.y, a.z, a.w};
    }
    case AttributeType::Quaternion: {
        Math::Quaternion q = std::get_if<Math::Quaternion>(&v) ? *std::get_if<Math::Quaternion>(&v) : Math::Quaternion{};
        return SmfQuaternion{q.x, q.y, q.z, q.w};
    }
    case AttributeType::Matrix4: {
        Math::Matrix4 m = std::get_if<Math::Matrix4>(&v) ? *std::get_if<Math::Matrix4>(&v) : Math::Matrix4::Identity();
        SmfMatrix4 s{};
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                s.m[static_cast<size_t>(r)][static_cast<size_t>(c)] = m.M[r][c];
            }
        }
        return s;
    }
    case AttributeType::String:
        return std::get_if<std::string>(&v) ? *std::get_if<std::string>(&v) : std::string{};
    case AttributeType::AssetHash:
    case AttributeType::ArzachelSeed:
        return std::get_if<uint64_t>(&v) ? *std::get_if<uint64_t>(&v) : 0ull;
    case AttributeType::BlobOpaque:
    case AttributeType::SkeletonPose:
        return std::get_if<std::vector<std::byte>>(&v) ? *std::get_if<std::vector<std::byte>>(&v) : std::vector<std::byte>{};
    case AttributeType::ElementRef: {
        ElementIndex er = PARALLAX_INVALID_INDEX;
        if (const auto* ei = std::get_if<ElementIndex>(&v)) {
            er = *ei;
        }
        return static_cast<uint32_t>(er);
    }
    case AttributeType::EasingType:
        return static_cast<uint8_t>(std::get_if<EasingType>(&v) ? static_cast<int>(*std::get_if<EasingType>(&v)) : 0);
    case AttributeType::TransitionBlendMode:
        return static_cast<uint8_t>(std::get_if<BlendMode>(&v) ? static_cast<int>(*std::get_if<BlendMode>(&v)) : 0);
    default:
        return std::monostate{};
    }
}

bool FromSmfValue(AttributeType t, const Solstice::Smf::SmfValue& sv, AttributeValue& outVal) {
    using namespace Solstice::Smf;
    switch (t) {
    case AttributeType::Bool:
        outVal = static_cast<bool>(std::get_if<bool>(&sv) && *std::get_if<bool>(&sv));
        break;
    case AttributeType::Int32:
        outVal = static_cast<int32_t>(std::get_if<int32_t>(&sv) ? *std::get_if<int32_t>(&sv) : 0);
        break;
    case AttributeType::Int64:
        outVal = std::get_if<int64_t>(&sv) ? *std::get_if<int64_t>(&sv) : int64_t{0};
        break;
    case AttributeType::Float:
        outVal = std::get_if<float>(&sv) ? *std::get_if<float>(&sv) : 0.f;
        break;
    case AttributeType::Double:
        outVal = std::get_if<double>(&sv) ? *std::get_if<double>(&sv) : 0.0;
        break;
    case AttributeType::Vec2: {
        auto* a = std::get_if<SmfVec2>(&sv);
        outVal = a ? Math::Vec2(a->x, a->y) : Math::Vec2{};
        break;
    }
    case AttributeType::Vec3: {
        auto* a = std::get_if<SmfVec3>(&sv);
        outVal = a ? Math::Vec3(a->x, a->y, a->z) : Math::Vec3{};
        break;
    }
    case AttributeType::Vec4:
    case AttributeType::ColorRGBA: {
        auto* a = std::get_if<SmfVec4>(&sv);
        outVal = a ? Math::Vec4(a->x, a->y, a->z, a->w) : Math::Vec4{};
        break;
    }
    case AttributeType::Quaternion: {
        auto* q = std::get_if<SmfQuaternion>(&sv);
        outVal = q ? Math::Quaternion(q->x, q->y, q->z, q->w) : Math::Quaternion{};
        break;
    }
    case AttributeType::Matrix4: {
        auto* sm = std::get_if<SmfMatrix4>(&sv);
        Math::Matrix4 m = Math::Matrix4::Identity();
        if (sm) {
            for (int r = 0; r < 4; ++r) {
                for (int c = 0; c < 4; ++c) {
                    m.M[r][c] = sm->m[static_cast<size_t>(r)][static_cast<size_t>(c)];
                }
            }
        }
        outVal = m;
        break;
    }
    case AttributeType::String:
        outVal = std::get_if<std::string>(&sv) ? *std::get_if<std::string>(&sv) : std::string{};
        break;
    case AttributeType::AssetHash:
    case AttributeType::ArzachelSeed:
        outVal = std::get_if<uint64_t>(&sv) ? *std::get_if<uint64_t>(&sv) : 0ull;
        break;
    case AttributeType::BlobOpaque:
    case AttributeType::SkeletonPose:
        outVal = std::get_if<std::vector<std::byte>>(&sv) ? *std::get_if<std::vector<std::byte>>(&sv) : std::vector<std::byte>{};
        break;
    case AttributeType::ElementRef:
        outVal = static_cast<ElementIndex>(std::get_if<uint32_t>(&sv) ? *std::get_if<uint32_t>(&sv)
                                                                      : Solstice::Smf::SMF_INVALID_INDEX);
        break;
    case AttributeType::EasingType:
        outVal = static_cast<EasingType>(std::get_if<uint8_t>(&sv) ? *std::get_if<uint8_t>(&sv) : 0);
        break;
    case AttributeType::TransitionBlendMode:
        outVal = static_cast<BlendMode>(std::get_if<uint8_t>(&sv) ? *std::get_if<uint8_t>(&sv) : 0);
        break;
    default:
        outVal = std::monostate{};
        break;
    }
    return true;
}

void WriteAttributeValue(std::vector<std::byte>& out, AttributeType t, const AttributeValue& v) {
    Solstice::Smf::Wire::WriteAttributeValue(out, ToSmf(t), ToSmfValue(t, v));
}

bool ReadAttributeValue(const std::byte*& p, const std::byte* end, AttributeType t, AttributeValue& outVal) {
    Solstice::Smf::SmfValue sv{};
    if (!Solstice::Smf::Wire::ReadAttributeValue(p, end, ToSmf(t), sv)) {
        return false;
    }
    return FromSmfValue(t, sv, outVal);
}

AttributeType AttrTypeForKey(const ParallaxScene& scene, const ParallaxScene::ElementNode& el, const std::string& key) {
    if (el.SchemaIndex >= scene.GetSchemas().size()) {
        return AttributeType::Float;
    }
    for (const auto& ad : scene.GetSchemas()[el.SchemaIndex].Attributes) {
        if (ad.Name == key) {
            return ad.Type;
        }
    }
    return AttributeType::Float;
}

} // namespace

bool SaveSceneToBytes(const ParallaxScene& scene, std::vector<std::byte>& out, bool compressWhole, ParallaxError* err) {
    (void)err;
    std::unordered_map<std::string, uint32_t, std::hash<std::string>, std::equal_to<>> strOff;
    std::vector<std::string> poolOrder;

    auto intern = [&](const std::string& s) -> uint32_t {
        auto it = strOff.find(s);
        if (it != strOff.end()) {
            return it->second;
        }
        uint32_t off = 0;
        for (const auto& e : poolOrder) {
            off += static_cast<uint32_t>(e.size()) + 1;
        }
        strOff[s] = off;
        poolOrder.push_back(s);
        return off;
    };

    for (const auto& sc : scene.GetSchemas()) {
        intern(sc.TypeName);
        for (const auto& a : sc.Attributes) {
            intern(a.Name);
        }
    }
    for (const auto& el : scene.GetElements()) {
        intern(el.Name);
        for (const auto& kv : el.Attributes) {
            intern(kv.first);
        }
    }
    for (const auto& ch : scene.GetChannels()) {
        intern(ch.AttributeName);
    }
    for (const auto& mg : scene.GetMGElements()) {
        intern(mg.Name);
        for (const auto& kv : mg.Attributes) {
            intern(kv.first);
        }
    }
    for (const auto& tr : scene.GetMGTracks()) {
        intern(tr.PropertyName);
    }
    for (const auto& kv : scene.GetPathTable()) {
        intern(kv.first);
    }

    std::vector<std::byte> stringTable;
    for (const auto& s : poolOrder) {
        for (char c : s) {
            stringTable.push_back(static_cast<std::byte>(static_cast<unsigned char>(c)));
        }
        stringTable.push_back(std::byte{0});
    }

    std::vector<std::byte> schemaBlob;
    Solstice::Smf::Wire::AppendU32(schemaBlob, static_cast<uint32_t>(scene.GetSchemas().size()));
    for (const auto& sc : scene.GetSchemas()) {
        SchemaEntryDisk se{};
        se.TypeNameOffset = intern(sc.TypeName);
        se.AttributeCount = static_cast<uint32_t>(sc.Attributes.size());
        schemaBlob.insert(schemaBlob.end(), reinterpret_cast<const std::byte*>(&se),
                          reinterpret_cast<const std::byte*>(&se) + sizeof(se));
        for (const auto& ad : sc.Attributes) {
            AttributeDescriptorDisk dd{};
            dd.NameOffset = intern(ad.Name);
            dd.Type = static_cast<uint8_t>(ad.Type);
            dd.Flags = ad.Flags;
            schemaBlob.insert(schemaBlob.end(), reinterpret_cast<const std::byte*>(&dd),
                              reinterpret_cast<const std::byte*>(&dd) + sizeof(dd));
        }
    }

    std::vector<std::byte> attrBlob;
    std::vector<uint32_t> elemAttrOffsets(scene.GetElements().size(), 0);
    for (size_t ei = 0; ei < scene.GetElements().size(); ++ei) {
        elemAttrOffsets[ei] = static_cast<uint32_t>(attrBlob.size());
        const auto& el = scene.GetElements()[ei];
        Solstice::Smf::Wire::AppendU32(attrBlob, static_cast<uint32_t>(el.Attributes.size()));
        for (const auto& kv : el.Attributes) {
            AttributeType at = AttrTypeForKey(scene, el, kv.first);
            Solstice::Smf::Wire::AppendU32(attrBlob, intern(kv.first));
            attrBlob.push_back(static_cast<std::byte>(static_cast<uint8_t>(at)));
            attrBlob.push_back(std::byte{0});
            attrBlob.push_back(std::byte{0});
            attrBlob.push_back(std::byte{0});
            WriteAttributeValue(attrBlob, at, kv.second);
        }
    }

    std::vector<std::byte> elemBlob;
    Solstice::Smf::Wire::AppendU32(elemBlob, static_cast<uint32_t>(scene.GetElements().size()));
    for (size_t ei = 0; ei < scene.GetElements().size(); ++ei) {
        const auto& el = scene.GetElements()[ei];
        ElementHeaderDisk eh{};
        eh.SchemaIndex = el.SchemaIndex;
        eh.NameOffset = intern(el.Name);
        eh.ParentIndex = el.Parent;
        eh.FirstChildIndex = el.FirstChild;
        eh.NextSiblingIndex = el.NextSibling;
        eh.AttributeDataOffset = elemAttrOffsets[ei];
        eh.ChannelCount = el.ChannelCount;
        eh.FirstChannelIndex = el.FirstChannelIndex;
        elemBlob.insert(elemBlob.end(), reinterpret_cast<const std::byte*>(&eh), reinterpret_cast<const std::byte*>(&eh) + sizeof(eh));
    }

    std::vector<std::byte> channelBlob;
    std::vector<std::byte> channelIndexBlob;
    Solstice::Smf::Wire::AppendU32(channelIndexBlob, static_cast<uint32_t>(scene.GetChannels().size()));
    uint64_t dataCursor = 0;
    for (const auto& ch : scene.GetChannels()) {
        uint64_t start = channelBlob.size();
        for (const auto& kf : ch.Keyframes) {
            KeyframeHeader kh{};
            kh.TimeTicks = kf.TimeTicks;
            kh.EasingType = kf.Easing;
            kh.Flags = kf.Flags;
            channelBlob.insert(channelBlob.end(), reinterpret_cast<const std::byte*>(&kh),
                               reinterpret_cast<const std::byte*>(&kh) + sizeof(kh));
            WriteAttributeValue(channelBlob, ch.ValueType, kf.Value);
        }

        ChannelIndexEntryDisk ce{};
        ce.ElementIndex = ch.Element;
        ce.AttributeNameOffset = intern(ch.AttributeName);
        ce.ValueType = static_cast<uint8_t>(ch.ValueType);
        ce.CompressionType = 0;
        ce.LayerIndex = static_cast<uint16_t>(ch.Layer == PARALLAX_INVALID_INDEX ? 0xFFFFu : ch.Layer);
        ce.DataOffset = dataCursor;
        ce.KeyframeCount = static_cast<uint32_t>(ch.Keyframes.size());
        ce.ChunkCount = 1;
        dataCursor += static_cast<uint64_t>(channelBlob.size() - start);

        channelIndexBlob.insert(channelIndexBlob.end(), reinterpret_cast<const std::byte*>(&ce),
                                reinterpret_cast<const std::byte*>(&ce) + sizeof(ce));
    }

    std::vector<std::byte> mgBlob;
    MotionGraphicsSectionHeaderDisk mgh{};
    mgh.ElementCount = static_cast<uint32_t>(scene.GetMGElements().size());
    mgh.TrackCount = static_cast<uint32_t>(scene.GetMGTracks().size());
    mgh.CompositeMode = static_cast<uint32_t>(scene.GetMGCompositeMode());
    mgh.GlobalAlpha = scene.GetMGGlobalAlpha();
    mgh.GlobalAlphaChannelIndex = scene.GetMGGlobalAlphaChannelIndex();
    mgBlob.insert(mgBlob.end(), reinterpret_cast<const std::byte*>(&mgh), reinterpret_cast<const std::byte*>(&mgh) + sizeof(mgh));
    for (const auto& mg : scene.GetMGElements()) {
        MGElementEntryDisk me{};
        me.SchemaIndex = mg.SchemaIndex;
        me.NameOffset = intern(mg.Name);
        me.ParentMGIndex = mg.Parent;
        me.FirstChildMGIndex = mg.FirstChild;
        me.NextSiblingMGIndex = mg.NextSibling;
        me.AttributeDataOffset = 0;
        me.FirstTrackIndex = mg.FirstTrackIndex;
        me.TrackCount = mg.TrackCount;
        mgBlob.insert(mgBlob.end(), reinterpret_cast<const std::byte*>(&me), reinterpret_cast<const std::byte*>(&me) + sizeof(me));
    }
    for (const auto& tr : scene.GetMGTracks()) {
        MGTrackEntryDisk te{};
        te.PropertyNameOffset = intern(tr.PropertyName);
        te.ValueType = static_cast<uint8_t>(tr.ValueType);
        te.EasingType = tr.EasingType;
        te.CompressionType = static_cast<uint8_t>(tr.Compression);
        te.Flags = tr.Flags;
        te.KeyframeCount = static_cast<uint32_t>(tr.Keyframes.size());
        te.DataOffset = 0;
        mgBlob.insert(mgBlob.end(), reinterpret_cast<const std::byte*>(&te), reinterpret_cast<const std::byte*>(&te) + sizeof(te));
    }

    std::vector<std::byte> pathBlob;
    Solstice::Smf::Wire::AppendU32(pathBlob, static_cast<uint32_t>(scene.GetPathTable().size()));
    for (const auto& kv : scene.GetPathTable()) {
        PathTableEntryDisk pe{};
        pe.PathOffset = intern(kv.first);
        pe.AssetHash = kv.second;
        pathBlob.insert(pathBlob.end(), reinterpret_cast<const std::byte*>(&pe), reinterpret_cast<const std::byte*>(&pe) + sizeof(pe));
    }

    std::vector<std::byte> elementGraph;
    elementGraph.insert(elementGraph.end(), elemBlob.begin(), elemBlob.end());
    elementGraph.insert(elementGraph.end(), attrBlob.begin(), attrBlob.end());

    std::vector<std::byte> file;
    file.resize(sizeof(FileHeader));

    auto append = [&](const std::vector<std::byte>& sec) -> uint32_t {
        uint32_t o = static_cast<uint32_t>(file.size());
        file.insert(file.end(), sec.begin(), sec.end());
        return o;
    };

    FileHeader fh{};
    fh.Magic = PARALLAX_MAGIC;
    fh.FormatVersionMajor = PARALLAX_FORMAT_VERSION_MAJOR;
    fh.FormatVersionMinor = PARALLAX_FORMAT_VERSION_MINOR;
    fh.TimelineDurationTicks = scene.GetTimelineDurationTicks();
    fh.TicksPerSecond = scene.GetTicksPerSecond();
    fh.StringTableOffset = append(stringTable);
    fh.StringTableSize = static_cast<uint32_t>(stringTable.size());
    fh.SchemaRegistryOffset = append(schemaBlob);
    fh.SchemaRegistrySize = static_cast<uint32_t>(schemaBlob.size());
    fh.ElementGraphOffset = append(elementGraph);
    fh.ElementGraphSize = static_cast<uint32_t>(elementGraph.size());
    fh.ChannelDataIndexOffset = append(channelIndexBlob);
    fh.ChannelDataBlobOffset = append(channelBlob);
    fh.ChannelDataBlobSize = static_cast<uint32_t>(channelBlob.size());
    fh.MotionGraphicsSectionOffset = append(mgBlob);
    fh.MotionGraphicsSectionSize = static_cast<uint32_t>(mgBlob.size());
    fh.AudioMixOffset = static_cast<uint32_t>(file.size());
    fh.PhysicsSnapshotOffset = fh.AudioMixOffset;
    fh.ScriptSnapshotOffset = fh.AudioMixOffset;
    fh.RenderJobTableOffset = fh.AudioMixOffset;
    fh.RenderJobCount = 0;
    fh.PathTableOffset = append(pathBlob);
    fh.Flags = compressWhole ? 1u : 0u;

    std::memcpy(file.data(), &fh, sizeof(fh));

    if (compressWhole) {
        const size_t hdrSize = sizeof(FileHeader);
        const size_t tailSize = file.size() - hdrSize;
        size_t maxDst = ZSTD_compressBound(tailSize);
        std::vector<std::byte> packed(hdrSize + maxDst);
        std::memcpy(packed.data(), file.data(), hdrSize);
        size_t z = ZSTD_compress(packed.data() + hdrSize, maxDst, file.data() + hdrSize, tailSize, 3);
        if (ZSTD_isError(z)) {
            if (err) {
                *err = ParallaxError::OutOfMemory;
            }
            return false;
        }
        packed.resize(hdrSize + z);
        out = std::move(packed);
    } else {
        out = std::move(file);
    }
    return true;
}

bool LoadSceneFromBytes(ParallaxScene& scene, std::span<const std::byte> data, ParallaxError* err) {
    const std::byte* base = data.data();
    size_t len = data.size();
    std::vector<std::byte> owned;

    if (len < sizeof(FileHeader)) {
        if (err) {
            *err = ParallaxError::CorruptHeader;
        }
        return false;
    }

    FileHeader fh{};
    std::memcpy(&fh, base, sizeof(fh));
    if (fh.Magic != PARALLAX_MAGIC) {
        if (err) {
            *err = ParallaxError::InvalidMagic;
        }
        return false;
    }
    if (fh.FormatVersionMajor != PARALLAX_FORMAT_VERSION_MAJOR) {
        if (err) {
            *err = ParallaxError::UnsupportedVersion;
        }
        return false;
    }
    if (fh.FormatVersionMinor > PARALLAX_FORMAT_VERSION_MINOR) {
        if (err) {
            *err = ParallaxError::UnsupportedVersion;
        }
        return false;
    }

    if (fh.Flags & 1u) {
        const size_t hdrSize = sizeof(FileHeader);
        if (len <= hdrSize) {
            if (err) {
                *err = ParallaxError::CorruptHeader;
            }
            return false;
        }
        const std::byte* comp = base + hdrSize;
        const size_t compLen = len - hdrSize;
        unsigned long long dec = ZSTD_getFrameContentSize(comp, compLen);
        if (dec == ZSTD_CONTENTSIZE_ERROR || dec == ZSTD_CONTENTSIZE_UNKNOWN) {
            if (err) {
                *err = ParallaxError::CorruptHeader;
            }
            return false;
        }
        std::vector<std::byte> tail(static_cast<size_t>(dec));
        size_t r = ZSTD_decompress(tail.data(), tail.size(), comp, compLen);
        if (ZSTD_isError(r) || r != tail.size()) {
            if (err) {
                *err = ParallaxError::CorruptHeader;
            }
            return false;
        }
        owned.resize(hdrSize + tail.size());
        std::memcpy(owned.data(), base, hdrSize);
        std::memcpy(owned.data() + hdrSize, tail.data(), tail.size());
        {
            FileHeader* patch = reinterpret_cast<FileHeader*>(owned.data());
            patch->Flags = static_cast<uint16_t>(patch->Flags & ~1u);
        }
        base = owned.data();
        len = owned.size();
    }

    auto inRange = [&](uint32_t off, uint32_t sz) -> bool {
        return static_cast<uint64_t>(off) + static_cast<uint64_t>(sz) <= len;
    };

    std::memcpy(&fh, base, sizeof(fh));

    ParallaxScene loaded = ParallaxScene{};
    RegisterBuiltinSchemas(loaded);
    loaded.GetSchemas().clear();
    loaded.SetTicksPerSecond(fh.TicksPerSecond);
    loaded.SetTimelineDurationTicks(fh.TimelineDurationTicks);

    if (!inRange(fh.StringTableOffset, fh.StringTableSize) || !inRange(fh.SchemaRegistryOffset, fh.SchemaRegistrySize) ||
        !inRange(fh.ElementGraphOffset, fh.ElementGraphSize) || !inRange(fh.ChannelDataIndexOffset, 4) ||
        !inRange(fh.ChannelDataBlobOffset, fh.ChannelDataBlobSize) || !inRange(fh.MotionGraphicsSectionOffset, fh.MotionGraphicsSectionSize)) {
        if (err) {
            *err = ParallaxError::CorruptHeader;
        }
        return false;
    }

    const char* strBase = reinterpret_cast<const char*>(base + fh.StringTableOffset);
    auto strAt = [&](uint32_t o) -> std::string {
        if (o >= fh.StringTableSize) {
            return {};
        }
        const char* p = strBase + o;
        const char* q = p;
        size_t max = fh.StringTableSize - o;
        size_t n = 0;
        while (n < max && *q) {
            ++q;
            ++n;
        }
        return std::string(p, q);
    };

    const std::byte* pSchema = base + fh.SchemaRegistryOffset;
    const std::byte* endSchema = pSchema + fh.SchemaRegistrySize;
    uint32_t schemaCount = Solstice::Smf::Wire::ReadU32(pSchema);
    pSchema += 4;
    for (uint32_t si = 0; si < schemaCount && pSchema < endSchema; ++si) {
        SchemaEntryDisk se{};
        if (static_cast<size_t>(endSchema - pSchema) < sizeof(se)) {
            break;
        }
        std::memcpy(&se, pSchema, sizeof(se));
        pSchema += sizeof(se);
        SchemaDef def;
        def.TypeName = strAt(se.TypeNameOffset);
        for (uint32_t ai = 0; ai < se.AttributeCount && pSchema < endSchema; ++ai) {
            AttributeDescriptorDisk ad{};
            if (static_cast<size_t>(endSchema - pSchema) < sizeof(ad)) {
                break;
            }
            std::memcpy(&ad, pSchema, sizeof(ad));
            pSchema += sizeof(ad);
            SchemaAttributeDef sad;
            sad.Name = strAt(ad.NameOffset);
            sad.Type = static_cast<AttributeType>(ad.Type);
            sad.Flags = ad.Flags;
            def.Attributes.push_back(std::move(sad));
        }
        loaded.GetSchemas().push_back(std::move(def));
    }

    const std::byte* pEG = base + fh.ElementGraphOffset;
    const std::byte* endEG = pEG + fh.ElementGraphSize;
    uint32_t elemCount = Solstice::Smf::Wire::ReadU32(pEG);
    pEG += 4;
    std::vector<uint32_t> attrOffsets;
    attrOffsets.resize(elemCount);
    loaded.GetElements().clear();
    loaded.GetElements().reserve(elemCount);
    for (uint32_t ei = 0; ei < elemCount && pEG + sizeof(ElementHeaderDisk) <= endEG; ++ei) {
        ElementHeaderDisk eh{};
        std::memcpy(&eh, pEG, sizeof(eh));
        pEG += sizeof(eh);
        ParallaxScene::ElementNode node;
        node.SchemaIndex = eh.SchemaIndex;
        node.Name = strAt(eh.NameOffset);
        node.Parent = eh.ParentIndex;
        node.FirstChild = eh.FirstChildIndex;
        node.NextSibling = eh.NextSiblingIndex;
        node.FirstChannelIndex = eh.FirstChannelIndex;
        node.ChannelCount = eh.ChannelCount;
        attrOffsets[ei] = eh.AttributeDataOffset;
        loaded.GetElements().push_back(std::move(node));
    }

    const std::byte* attrSection = base + fh.ElementGraphOffset + 4 + static_cast<size_t>(elemCount) * sizeof(ElementHeaderDisk);
    for (uint32_t ei = 0; ei < elemCount; ++ei) {
        const std::byte* ap = attrSection + attrOffsets[ei];
        const std::byte* aend = endEG;
        if (ap >= aend) {
            continue;
        }
        uint32_t acount = Solstice::Smf::Wire::ReadU32(ap);
        ap += 4;
        for (uint32_t ai = 0; ai < acount && ap < aend; ++ai) {
            uint32_t nameOff = Solstice::Smf::Wire::ReadU32(ap);
            ap += 4;
            AttributeType at = static_cast<AttributeType>(static_cast<uint8_t>(*ap));
            ap += 4;
            std::string key = strAt(nameOff);
            AttributeValue val;
            if (!ReadAttributeValue(ap, aend, at, val)) {
                break;
            }
            loaded.GetElements()[ei].Attributes[std::move(key)] = std::move(val);
        }
    }

    const std::byte* pCI = base + fh.ChannelDataIndexOffset;
    uint32_t chCount = Solstice::Smf::Wire::ReadU32(pCI);
    pCI += 4;
    loaded.GetChannels().clear();
    loaded.GetChannels().resize(chCount);
    for (uint32_t ci = 0; ci < chCount; ++ci) {
        ChannelIndexEntryDisk ce{};
        std::memcpy(&ce, pCI, sizeof(ce));
        pCI += sizeof(ce);
        ChannelRecord cr;
        cr.Element = ce.ElementIndex;
        cr.AttributeName = strAt(ce.AttributeNameOffset);
        cr.ValueType = static_cast<AttributeType>(ce.ValueType);
        cr.Compression = static_cast<ChannelCompression>(ce.CompressionType);
        cr.Layer = ce.LayerIndex == 0xFFFFu ? PARALLAX_INVALID_INDEX : ce.LayerIndex;

        const std::byte* cdata = base + fh.ChannelDataBlobOffset + static_cast<size_t>(ce.DataOffset);
        const std::byte* cend = base + fh.ChannelDataBlobOffset + fh.ChannelDataBlobSize;
        for (uint32_t ki = 0; ki < ce.KeyframeCount && cdata + sizeof(KeyframeHeader) <= cend; ++ki) {
            KeyframeHeader kh{};
            std::memcpy(&kh, cdata, sizeof(kh));
            cdata += sizeof(kh);
            KeyframeRecord kr;
            kr.TimeTicks = kh.TimeTicks;
            kr.Easing = kh.EasingType;
            kr.Flags = kh.Flags;
            if (!ReadAttributeValue(cdata, cend, cr.ValueType, kr.Value)) {
                break;
            }
            cr.Keyframes.push_back(std::move(kr));
        }
        loaded.GetChannels()[ci] = std::move(cr);
    }

    if (fh.PathTableOffset != 0 && inRange(fh.PathTableOffset, 4)) {
        const std::byte* pp = base + fh.PathTableOffset;
        uint32_t ptCount = Solstice::Smf::Wire::ReadU32(pp);
        pp += 4;
        for (uint32_t i = 0; i < ptCount && pp + sizeof(PathTableEntryDisk) <= base + len; ++i) {
            PathTableEntryDisk pe{};
            std::memcpy(&pe, pp, sizeof(pe));
            pp += sizeof(pe);
            loaded.GetPathTable()[strAt(pe.PathOffset)] = pe.AssetHash;
        }
    }

    const std::byte* pMG = base + fh.MotionGraphicsSectionOffset;
    if (fh.MotionGraphicsSectionSize >= sizeof(MotionGraphicsSectionHeaderDisk)) {
        MotionGraphicsSectionHeaderDisk mgh{};
        std::memcpy(&mgh, pMG, sizeof(mgh));
        loaded.SetMGCompositeMode(static_cast<BlendMode>(mgh.CompositeMode));
        loaded.SetMGGlobalAlpha(mgh.GlobalAlpha);
        loaded.SetMGGlobalAlphaChannelIndex(mgh.GlobalAlphaChannelIndex);
    }

    scene = std::move(loaded);
    if (err) {
        *err = ParallaxError::None;
    }
    return true;
}

std::unique_ptr<ParallaxScene> LoadScene(const std::filesystem::path& path, IAssetResolver* resolver, ParallaxError* outError) {
    (void)resolver;
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        if (outError) {
            *outError = ParallaxError::StreamingError;
        }
        return nullptr;
    }
    f.seekg(0, std::ios::end);
    auto sz = f.tellg();
    f.seekg(0);
    if (sz <= 0) {
        if (outError) {
            *outError = ParallaxError::CorruptHeader;
        }
        return nullptr;
    }
    std::vector<std::byte> buf(static_cast<size_t>(sz));
    f.read(reinterpret_cast<char*>(buf.data()), sz);
    auto scene = std::make_unique<ParallaxScene>();
    if (!LoadSceneFromBytes(*scene, buf, outError)) {
        return nullptr;
    }
    return scene;
}

bool SaveScene(const ParallaxScene& scene, const std::filesystem::path& path, bool compressWhole, ParallaxError* outError) {
    std::vector<std::byte> bytes;
    if (!SaveSceneToBytes(scene, bytes, compressWhole, outError)) {
        return false;
    }
    std::ofstream f(path, std::ios::binary);
    if (!f) {
        if (outError) {
            *outError = ParallaxError::StreamingError;
        }
        return false;
    }
    f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (outError) {
        *outError = ParallaxError::None;
    }
    return true;
}

} // namespace Solstice::Parallax
