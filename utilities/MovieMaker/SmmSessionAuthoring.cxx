#include "SmmSessionAuthoring.hxx"
#include "SmmFileOps.hxx"
#include "SmmLipsyncMorpheme.hxx"

#include <Arzachel/TextLipSync.hxx>
#include <Parallax/ParallaxScene.hxx>
#include <Parallax/ParallaxTypes.hxx>

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <variant>

namespace Smm::Authoring {
namespace {

static std::string ToUpperAscii(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return s;
}

static void SkipWs(std::string_view& sv) {
    while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t')) {
        sv.remove_prefix(1);
    }
}

static bool ParseUInt64Hex(std::string_view sv, uint64_t& out) {
    SkipWs(sv);
    if (sv.size() >= 2 && (sv[0] == '0') && (sv[1] == 'x' || sv[1] == 'X')) {
        sv.remove_prefix(2);
    }
    out = 0;
    for (char c : sv) {
        if (c >= '0' && c <= '9') {
            out = (out * 16ull) + static_cast<uint64_t>(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            out = (out * 16ull) + static_cast<uint64_t>(10 + c - 'a');
        } else if (c >= 'A' && c <= 'F') {
            out = (out * 16ull) + static_cast<uint64_t>(10 + c - 'A');
        } else {
            break;
        }
    }
    return true;
}

static int SplitTab(const std::string& line, std::vector<std::string>& outCols, size_t minCols) {
    outCols.clear();
    std::string cur;
    for (char c : line) {
        if (c == '\t') {
            outCols.push_back(std::move(cur));
            cur.clear();
        } else {
            cur += c;
        }
    }
    outCols.push_back(std::move(cur));
    if (outCols.size() < minCols) {
        return -1;
    }
    return static_cast<int>(outCols.size());
}

static void ParseAttrPairs(
    const std::string& pairs, std::function<void(const std::string&, const std::string&)> emit) {
    size_t p = 0;
    while (p < pairs.size()) {
        size_t sc = pairs.find(';', p);
        const std::string part = (sc == std::string::npos) ? pairs.substr(p) : pairs.substr(p, sc - p);
        p = (sc == std::string::npos) ? pairs.size() : sc + 1;
        const size_t eq = part.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        std::string k = part.substr(0, eq);
        std::string v = part.substr(eq + 1);
        while (!k.empty() && (k.front() == ' ' || k.front() == '\t')) {
            k.erase(k.begin());
        }
        if (!k.empty()) {
            emit(k, v);
        }
    }
}

static Solstice::Parallax::AttributeValue StringToValueForKey(std::string_view key, const std::string& value, std::string& err) {
    err.clear();
    try {
        if (key == "FovDegrees" || key == "Near" || key == "Far" || key == "Radius" || key == "Intensity" || key == "Pitch" ||
            key == "Volume" || key == "CompositeAlpha" || key == "ArzachelRigidBodyDamage" || key == "LodDistanceHigh" ||
            key == "LodDistanceLow" || key == "Depth" || key == "SkyboxYawDegrees" || key == "SkyboxBrightness") {
            return Solstice::Parallax::AttributeValue{std::stof(value)};
        }
        if (key == "TickRate" || key == "ShadowResolution" || key == "AttachElementIndex") {
            return Solstice::Parallax::AttributeValue{static_cast<int32_t>(std::stoi(value))};
        }
        if (key == "Position" || key == "Target" || key == "VelMin" || key == "VelMax" || key == "Gravity") {
            float x = 0, y = 0, z = 0;
            if (std::sscanf(value.c_str(), "%f,%f,%f", &x, &y, &z) == 3) {
                return Solstice::Parallax::AttributeValue{Solstice::Math::Vec3(x, y, z)};
            }
            if (std::sscanf(value.c_str(), "%f,%f", &x, &y) == 2) {
                return Solstice::Parallax::AttributeValue{Solstice::Math::Vec2(x, y)};
            }
            err = "Position/Target: use x,y (MG) or x,y,z.";
            return Solstice::Parallax::AttributeValue{std::monostate{}};
        }
        if (key == "Color") {
            float x = 0, y = 0, z = 0, w = 1;
            if (std::sscanf(value.c_str(), "%f,%f,%f,%f", &x, &y, &z, &w) >= 3) {
                return Solstice::Parallax::AttributeValue{Solstice::Math::Vec4(x, y, z, w)};
            }
        }
        if (key == "Text" || key == "ArzachelAnimationClipPreset" || key == "ArzachelDestructionAnimPreset" ||
            key == "AttributeName" || key == "ModelPath") {
            return Solstice::Parallax::AttributeValue{value};
        }
        if (key == "MeshAsset" || key == "AnimationClip" || key == "AudioAsset" || key == "Texture" || key == "ShakeAsset") {
            uint64_t h = 0;
            (void)ParseUInt64Hex(value, h);
            return Solstice::Parallax::AttributeValue{h};
        }
        if (key == "CastShadows" || key == "SkyboxEnabled") {
            if (value == "1" || ToUpperAscii(value) == "TRUE") {
                return Solstice::Parallax::AttributeValue{true};
            }
            return Solstice::Parallax::AttributeValue{false};
        }
        if (key == "AmbientColor") {
            float r = 0, g = 0, b = 0, a = 0;
            if (std::sscanf(value.c_str(), "%f,%f,%f,%f", &r, &g, &b, &a) == 4) {
                return Solstice::Parallax::AttributeValue{Solstice::Math::Vec4(r, g, b, a)};
            }
        }
    } catch (const std::exception& ex) {
        err = ex.what();
        return Solstice::Parallax::AttributeValue{std::monostate{}};
    }
    return Solstice::Parallax::AttributeValue{value};
}

static uint32_t FindMgTrackForProperty(
    const Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::MGIndex el, const char* property) {
    if (el >= scene.GetMGElements().size()) {
        return 0xFFFFFFFFu;
    }
    const auto& mge = scene.GetMGElements()[el];
    const uint32_t t0 = mge.FirstTrackIndex;
    const uint32_t n = mge.TrackCount;
    if (n == 0) {
        return 0xFFFFFFFFu;
    }
    const auto& trs = scene.GetMGTracks();
    for (uint32_t i = 0; i < n; ++i) {
        if (t0 + i >= trs.size()) {
            break;
        }
        if (trs[t0 + i].PropertyName == property) {
            return t0 + i;
        }
    }
    return 0xFFFFFFFFu;
}

} // namespace

std::filesystem::path AuthoringSidecarPathForProject(const std::filesystem::path& smmJsonPath) {
    const std::string stem = smmJsonPath.stem().string();
    return smmJsonPath.parent_path() / (stem + ".authoring.tsv");
}

std::vector<std::string> ExtractMorphemeLikeTokens(const std::string& text) {
    std::vector<std::string> out;
    std::string cur;
    for (unsigned char ch : text) {
        if (std::isalnum(ch) != 0 || ch == static_cast<unsigned char>('-') || ch == static_cast<unsigned char>('\'')) {
            cur += static_cast<char>(ch);
        } else {
            if (!cur.empty()) {
                out.push_back(std::move(cur));
                cur.clear();
            }
        }
    }
    if (!cur.empty()) {
        out.push_back(std::move(cur));
    }
    return out;
}

float MouthOpenHeuristic2002(std::string_view w) {
    // Naive: vowel density → mouth openness. Good enough to schedule placeholder shots.
    const std::string u = std::string(w);
    const std::string s = ToUpperAscii(u);
    int vowels = 0;
    for (char c : s) {
        if (c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U' || c == 'Y') {
            ++vowels;
        }
    }
    int len = 0;
    for (char c : s) {
        if (c != ' ') {
            ++len;
        }
    }
    if (len <= 0) {
        return 0.1f;
    }
    const float t = (static_cast<float>(vowels) + 1.f) / (static_cast<float>(len) + 4.f);
    return (std::clamp)(t * 1.1f, 0.12f, 0.95f);
}

void BuildLipsyncPlaceholderSamples(const LipsyncLineStub& line, uint32_t ticksPerSecond, std::vector<LipsyncSample>& out) {
    std::string warn;
    Smm::LipsyncMorpheme::BuildLipsyncSamples(line, ticksPerSecond, out, warn);
    (void)warn;
}

namespace {

Solstice::Parallax::ChannelIndex FindParallaxChannelForElement(
    const Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::ElementIndex el, const char* attributeName) {
    const auto& channels = scene.GetChannels();
    for (Solstice::Parallax::ChannelIndex ci = 0; ci < channels.size(); ++ci) {
        if (channels[ci].Element == el && channels[ci].AttributeName == attributeName) {
            return ci;
        }
    }
    return Solstice::Parallax::PARALLAX_INVALID_INDEX;
}

} // namespace

bool TryApplyLipsyncVisemeToActor(const LipsyncLineStub& line, Solstice::Parallax::ParallaxScene& scene,
    Solstice::Parallax::ElementIndex actorElement, std::string& errOut) {
    errOut.clear();
    if (actorElement >= scene.GetElements().size()) {
        errOut = "Invalid actor element index.";
        return false;
    }
    if (Solstice::Parallax::GetElementSchema(scene, actorElement) != "ActorElement") {
        errOut = "Viseme lipsync applies to ActorElement only.";
        return false;
    }
    const uint32_t tps = (std::max)(scene.GetTicksPerSecond(), 1u);
    std::vector<Solstice::Arzachel::VisemeKeyframeTick> keys;
    std::string lwarn;
    Solstice::Arzachel::BuildVisemeKeyframesFromEnglishText(
        line.Text, tps, line.StartTick, line.EndTick, line.MaxKeyframes, keys, lwarn);
    if (keys.empty()) {
        errOut = lwarn.empty() ? "No viseme keyframes generated." : lwarn;
        return false;
    }

    Solstice::Parallax::ChannelIndex chId =
        FindParallaxChannelForElement(scene, actorElement, Solstice::Parallax::kChannelFacialVisemeId);
    if (chId == Solstice::Parallax::PARALLAX_INVALID_INDEX) {
        chId = Solstice::Parallax::AddChannel(
            scene, actorElement, Solstice::Parallax::kChannelFacialVisemeId, Solstice::Parallax::AttributeType::String);
    }
    Solstice::Parallax::ChannelIndex chWt =
        FindParallaxChannelForElement(scene, actorElement, Solstice::Parallax::kChannelFacialVisemeWeight);
    if (chWt == Solstice::Parallax::PARALLAX_INVALID_INDEX) {
        chWt = Solstice::Parallax::AddChannel(
            scene, actorElement, Solstice::Parallax::kChannelFacialVisemeWeight, Solstice::Parallax::AttributeType::Float);
    }
    if (chId == Solstice::Parallax::PARALLAX_INVALID_INDEX || chWt == Solstice::Parallax::PARALLAX_INVALID_INDEX) {
        errOut = "Failed to add facial viseme channels.";
        return false;
    }

    for (const Solstice::Arzachel::VisemeKeyframeTick& k : keys) {
        const float st = std::clamp(k.Strength * line.Strength, 0.f, 4.f);
        Solstice::Parallax::RemoveKeyframe(scene, chId, k.TimeTicks);
        Solstice::Parallax::RemoveKeyframe(scene, chWt, k.TimeTicks);
        Solstice::Parallax::AddKeyframe(scene, chId, k.TimeTicks, Solstice::Parallax::AttributeValue{k.VisemeId},
            Solstice::Parallax::EasingType::Linear);
        Solstice::Parallax::AddKeyframe(scene, chWt, k.TimeTicks, Solstice::Parallax::AttributeValue{st},
            Solstice::Parallax::EasingType::Linear);
        Solstice::Parallax::SetKeyframeInterpolation(
            scene, chId, k.TimeTicks, Solstice::Parallax::KeyframeInterpolation::Hold);
    }
    errOut = lwarn;
    return true;
}

bool TryApplyLipsyncPlaceholderToMgText(const LipsyncLineStub& line, Solstice::Parallax::ParallaxScene& scene,
    Solstice::Parallax::MGIndex mgTextIndex, std::string& errOut) {
    errOut.clear();
    if (mgTextIndex >= scene.GetMGElements().size()) {
        errOut = "Invalid MG text index.";
        return false;
    }
    const auto& mgel = scene.GetMGElements()[mgTextIndex];
    std::string_view st;
    if (mgel.SchemaIndex < scene.GetSchemas().size()) {
        st = scene.GetSchemas()[mgel.SchemaIndex].TypeName;
    }
    if (st != "MGTextElement") {
        errOut = "Lipsync placeholder applies to MGTextElement (selected MG index).";
        return false;
    }
    const uint32_t tps = (std::max)(scene.GetTicksPerSecond(), 1u);
    std::vector<LipsyncSample> samples;
    std::string lwarn;
    Smm::LipsyncMorpheme::BuildLipsyncSamples(line, tps, samples, lwarn);
    if (samples.empty()) {
        errOut = lwarn.empty() ? "No lipsync samples to apply." : lwarn;
        return false;
    }
    uint32_t trackIndex = FindMgTrackForProperty(scene, mgTextIndex, "Depth");
    if (trackIndex == 0xFFFFFFFFu) {
        trackIndex = Solstice::Parallax::AddMGTrack(
            scene, mgTextIndex, "Depth", Solstice::Parallax::AttributeType::Float, Solstice::Parallax::EasingType::Linear);
    }
    if (trackIndex == 0xFFFFFFFFu || trackIndex >= scene.GetMGTracks().size()) {
        errOut = "Failed to get Depth track.";
        return false;
    }
    float baseDepth = 0.f;
    {
        const auto it = mgel.Attributes.find("Depth");
        if (it != mgel.Attributes.end()) {
            if (const auto* f = std::get_if<float>(&it->second)) {
                baseDepth = *f;
            }
        }
    }
    for (const LipsyncSample& s : samples) {
        const float wiggle = 0.08f * s.MouthOpen; // “breathing” draw order, like early PS2 talk systems.
        (void)Solstice::Parallax::RemoveMGKeyframe(scene, trackIndex, s.Tick);
        Solstice::Parallax::AddMGKeyframe(
            scene, trackIndex, s.Tick, Solstice::Parallax::AttributeValue{baseDepth + wiggle},
            Solstice::Parallax::EasingType::Linear);
    }
    errOut = lwarn; ///< Non-fatal notes (e.g. caps); empty if none.
    return true;
}

bool InstantiatePrefab(const PrefabEntry& prefab, Solstice::Parallax::ParallaxScene& scene, std::string& errOut,
    int* outElementIndex, Solstice::Parallax::MGIndex* outMGIndex) {
    errOut.clear();
    if (outElementIndex) {
        *outElementIndex = -1;
    }
    if (outMGIndex) {
        *outMGIndex = Solstice::Parallax::PARALLAX_INVALID_INDEX;
    }
    if (!Solstice::Parallax::HasSchema(scene, prefab.SchemaName)) {
        errOut = "Unknown schema: " + prefab.SchemaName;
        return false;
    }
    if (prefab.SchemaName == "MGSpriteElement" || prefab.SchemaName == "MGTextElement" ||
        prefab.SchemaName == "MotionGraphicsRootElement") {
        const Solstice::Parallax::MGIndex gi =
            Solstice::Parallax::AddMGElement(scene, prefab.SchemaName, prefab.DisplayName.empty() ? prefab.Id : prefab.DisplayName,
                Solstice::Parallax::PARALLAX_INVALID_INDEX);
        if (gi == Solstice::Parallax::PARALLAX_INVALID_INDEX) {
            errOut = "AddMGElement failed.";
            return false;
        }
        if (outMGIndex) {
            *outMGIndex = gi;
        }
        if (!prefab.AttrPairs.empty()) {
            auto& rec = scene.GetMGElements()[gi];
            ParseAttrPairs(prefab.AttrPairs, [&](const std::string& k, const std::string& v) {
                std::string e2;
                const Solstice::Parallax::AttributeValue av = StringToValueForKey(k, v, e2);
                if (e2.empty() && !std::holds_alternative<std::monostate>(av)) {
                    rec.Attributes[k] = av;
                }
            });
        }
        return true;
    }
    const Solstice::Parallax::ElementIndex ei = Solstice::Parallax::AddElement(scene, prefab.SchemaName,
        prefab.DisplayName.empty() ? prefab.Id : prefab.DisplayName, Solstice::Parallax::PARALLAX_INVALID_INDEX);
    if (ei == Solstice::Parallax::PARALLAX_INVALID_INDEX) {
        errOut = "AddElement failed.";
        return false;
    }
    if (outElementIndex) {
        *outElementIndex = static_cast<int>(ei);
    }
    if (!prefab.AttrPairs.empty()) {
        ParseAttrPairs(prefab.AttrPairs, [&](const std::string& k, const std::string& v) {
            std::string e2;
            const Solstice::Parallax::AttributeValue av = StringToValueForKey(k, v, e2);
            if (e2.empty() && !std::holds_alternative<std::monostate>(av)) {
                Solstice::Parallax::SetAttribute(scene, ei, k, av);
            }
        });
    }
    return true;
}

bool LoadSessionAuthoring(const std::filesystem::path& path, SessionState& out, std::string* err) {
    out = {};
    if (!std::filesystem::exists(path)) {
        return true;
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (err) {
            *err = "Failed to read authoring: " + path.string();
        }
        return false;
    }
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::vector<std::string> c;
        if (line.rfind("asset\t", 0) == 0) {
            if (SplitTab(line, c, 6) < 0) {
                continue;
            }
            // asset hex path tags isproxy full
            AssetDbEntry a;
            (void)ParseUInt64Hex(c[1], a.Hash);
            a.PathHint = c[2];
            a.Tags = c[3];
            a.IsProxy = (c[4] == "1" || c[4] == "true");
            (void)ParseUInt64Hex(c[5], a.ResolvesToHash);
            out.AssetDb.push_back(std::move(a));
        } else if (line.rfind("prefab\t", 0) == 0) {
            if (SplitTab(line, c, 5) < 0) {
                continue;
            }
            PrefabEntry p;
            p.Id = c[1];
            p.DisplayName = c[2];
            p.SchemaName = c[3];
            p.AttrPairs = c[4];
            out.Prefabs.push_back(std::move(p));
        } else if (line.rfind("lipsync\t", 0) == 0) {
            if (SplitTab(line, c, 5) < 0) {
                continue;
            }
            LipsyncLineStub l;
            l.Label = c[1];
            l.StartTick = static_cast<uint64_t>(std::stoull(c[2]));
            l.EndTick = static_cast<uint64_t>(std::stoull(c[3]));
            l.Text = c[4];
            if (c.size() > 5) {
                try {
                    l.Strength = std::stof(c[5]);
                } catch (...) {
                }
            }
            if (c.size() > 6) {
                try {
                    l.PhoneticMode = static_cast<uint8_t>(std::clamp(std::stoi(c[6]), 0, 255));
                } catch (...) {
                }
            }
            if (c.size() > 7) {
                l.MaxKeyframes = static_cast<uint32_t>(std::stoul(c[7]));
            }
            out.LipsyncStubs.push_back(std::move(l));
        }
    }
    return true;
}

namespace {

static std::string SanTab(std::string s) {
    for (char& c : s) {
        if (c == '\t' || c == '\n' || c == '\r') {
            c = ' ';
        }
    }
    return s;
}

} // namespace

bool SaveSessionAuthoring(const std::filesystem::path& path, const SessionState& st, std::string* err) {
    try {
        std::error_code ec;
        if (const std::filesystem::path parent = path.parent_path(); !parent.empty()) {
            std::filesystem::create_directories(parent, ec);
        }
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out) {
            if (err) {
                *err = "Failed to open for write: " + path.string();
            }
            return false;
        }
        out << "# SMM authoring sidecar (TSV; TP1). Does not change .smm.json \"version\":1 — separate file.\n";
        for (const auto& a : st.AssetDb) {
            out << "asset\t0x" << std::hex << a.Hash << std::dec << '\t' << SanTab(a.PathHint) << '\t' << SanTab(a.Tags) << '\t'
                << (a.IsProxy ? 1 : 0) << "\t0x" << std::hex << a.ResolvesToHash << std::dec << '\n';
        }
        for (const auto& p : st.Prefabs) {
            out << "prefab\t" << SanTab(p.Id) << '\t' << SanTab(p.DisplayName) << '\t' << SanTab(p.SchemaName) << '\t'
                << SanTab(p.AttrPairs) << '\n';
        }
        for (const auto& l : st.LipsyncStubs) {
            out << "lipsync\t" << SanTab(l.Label) << '\t' << l.StartTick << '\t' << l.EndTick << '\t' << SanTab(l.Text) << '\t'
                << l.Strength << '\t' << static_cast<int>(l.PhoneticMode) << '\t' << l.MaxKeyframes << '\n';
        }
        out.flush();
        if (!out) {
            if (err) {
                *err = "Write failed: " + path.string();
            }
            return false;
        }
    } catch (const std::exception& e) {
        if (err) {
            *err = e.what();
        }
        return false;
    }
    return true;
}

void DrawAuthoringSessionTab(const char* tabId, SessionState& session, Solstice::Parallax::ParallaxScene& scene,
    Solstice::Parallax::DevSessionAssetResolver& resolver, const std::vector<LibUI::AssetBrowser::Entry>& browser, int& assetListSelected,
    bool& sceneDirty, int& elementSelected, int& mgElementSelected, std::string& statusLine, const std::filesystem::path& smmJsonPath,
    bool compressPrlx, uint32_t timeTicks, uint32_t ticksPerSecond) {
    (void)resolver;
    (void)timeTicks;
    (void)ticksPerSecond;
    ImGui::PushID(tabId);
    const std::string side = AuthoringSidecarPathForProject(smmJsonPath).string();
    ImGui::TextDisabled("Authoring data file: %s", side.c_str());
    if (ImGui::Button("Save authoring TSV##ath")) {
        std::string se;
        if (SaveSessionAuthoring(AuthoringSidecarPathForProject(smmJsonPath), session, &se)) {
            statusLine = "Saved " + side;
        } else {
            statusLine = "Authoring save failed: " + se;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload from disk##ath")) {
        std::string le;
        if (LoadSessionAuthoring(AuthoringSidecarPathForProject(smmJsonPath), session, &le)) {
            statusLine = "Loaded authoring from " + side;
        } else {
            statusLine = "Authoring load failed: " + le;
        }
    }
    if (ImGui::CollapsingHeader("Simplified asset database", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextDisabled("Tags are comma labels; 'proxy' = low-LOD / stand-in. Resolves = full-quality hash if known.");
        if (ImGui::Button("Add selected session asset row##asdb") && assetListSelected >= 0 &&
            assetListSelected < static_cast<int>(browser.size())) {
            const auto& b = browser[static_cast<size_t>(assetListSelected)];
            AssetDbEntry row;
            row.Hash = b.Hash;
            row.PathHint = b.DisplayName;
            row.IsProxy = false;
            row.ResolvesToHash = 0;
            session.AssetDb.push_back(std::move(row));
            char hbuf[32]{};
            std::snprintf(hbuf, sizeof(hbuf), "%llX", static_cast<unsigned long long>(b.Hash));
            statusLine = std::string("Authoring: pushed asset 0x") + hbuf + " into DB list.";
        }
        for (int i = 0; i < static_cast<int>(session.AssetDb.size()); ++i) {
            ImGui::PushID(i);
            AssetDbEntry& a = session.AssetDb[static_cast<size_t>(i)];
            char bufH[32]{};
            std::snprintf(bufH, sizeof(bufH), "%llX", static_cast<unsigned long long>(a.Hash));
            char bufF[32]{};
            std::snprintf(bufF, sizeof(bufF), "%llX", static_cast<unsigned long long>(a.ResolvesToHash));
            char pbuf[256]{};
            char tbuf[128]{};
            std::snprintf(pbuf, sizeof(pbuf), "%s", a.PathHint.c_str());
            std::snprintf(tbuf, sizeof(tbuf), "%s", a.Tags.c_str());
            if (ImGui::InputText("path", pbuf, sizeof(pbuf))) {
                a.PathHint = pbuf;
            }
            if (ImGui::InputText("tags", tbuf, sizeof(tbuf))) {
                a.Tags = tbuf;
            }
            ImGui::InputText("hash##ro", bufH, sizeof(bufH), ImGuiInputTextFlags_ReadOnly);
            if (ImGui::InputText("resolves to (hex)", bufF, sizeof(bufF))) {
                uint64_t tmp = 0;
                (void)ParseUInt64Hex(bufF, tmp);
                a.ResolvesToHash = tmp;
            }
            bool px = a.IsProxy;
            if (ImGui::Checkbox("proxy asset##px", &px)) {
                a.IsProxy = px;
            }
            if (ImGui::SmallButton("remove##asdb")) {
                session.AssetDb.erase(session.AssetDb.begin() + i);
                ImGui::PopID();
                break;
            }
            ImGui::Separator();
            ImGui::PopID();
        }
    }
    if (ImGui::CollapsingHeader("Prefabs (template instances)", ImGuiTreeNodeFlags_DefaultOpen)) {
        static char pId[64] = "cam_sp";
        static char pName[96] = "Shot Camera";
        static char pSchema[64] = "CameraElement";
        static char pAttrs[512] = "FovDegrees=55;Near=0.1;Far=5000;Position=0,1.6,0.2;Target=0,0,-1";
        ImGui::InputText("id##pf", pId, sizeof(pId));
        ImGui::InputText("name##pf", pName, sizeof(pName));
        ImGui::InputText("schema##pf", pSchema, sizeof(pSchema));
        ImGui::InputText("attrs (Key=val;..)##pf", pAttrs, sizeof(pAttrs));
        if (ImGui::Button("Save as prefab in list##pf")) {
            PrefabEntry pe;
            pe.Id = pId;
            pe.DisplayName = pName;
            pe.SchemaName = pSchema;
            pe.AttrPairs = pAttrs;
            session.Prefabs.push_back(std::move(pe));
            statusLine = "Prefabs: added " + std::string(pId) + " (use Instantiate to add to scene).";
        }
        for (int i = 0; i < static_cast<int>(session.Prefabs.size()); ++i) {
            const PrefabEntry& p = session.Prefabs[static_cast<size_t>(i)];
            ImGui::BulletText("%s  [%s]  %s", p.Id.c_str(), p.SchemaName.c_str(), p.DisplayName.c_str());
            ImGui::SameLine();
            ImGui::PushID(i);
            if (ImGui::SmallButton("Instantiate##pf")) {
                Smm::PushSceneUndoSnapshot(scene, compressPrlx);
                std::string e;
                int newEl = -1;
                Solstice::Parallax::MGIndex newMg = Solstice::Parallax::PARALLAX_INVALID_INDEX;
                if (InstantiatePrefab(p, scene, e, &newEl, &newMg)) {
                    sceneDirty = true;
                    if (newEl >= 0) {
                        elementSelected = newEl;
                    }
                    if (newMg != Solstice::Parallax::PARALLAX_INVALID_INDEX) {
                        mgElementSelected = static_cast<int>(newMg);
                    }
                    statusLine = "Instantiated prefab: " + p.Id;
                } else {
                    statusLine = e;
                }
            }
            if (ImGui::SmallButton("delete##pf")) {
                session.Prefabs.erase(session.Prefabs.begin() + i);
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
        }
    }
    if (ImGui::CollapsingHeader("Morpheme tokens + lipsync (placeholder; extensible)", ImGuiTreeNodeFlags_DefaultOpen)) {
        static char lipText[1024] = "Hello from Solstice";
        static char lipLabel[64] = "line_01";
        static uint64_t t0 = 0;
        static uint64_t t1 = 6000;
        static float lipStrength = 1.f;
        static int lipMode = 0; ///< 0 = even per-word, 1 = vowel peak timing.
        static int lipMaxK = 2048;
        ImGui::InputText("Mouth text (word tokens = morpheme proxies)##lp", lipText, sizeof(lipText));
        ImGui::InputText("Label##lp", lipLabel, sizeof(lipLabel));
        ImGui::InputScalar("Start tick", ImGuiDataType_U64, &t0);
        ImGui::InputScalar("End tick", ImGuiDataType_U64, &t1);
        ImGui::SliderFloat("Preview strength (also saved in stub)##lp", &lipStrength, 0.25f, 2.f, "%.2f");
        const char* mitems = "Word steps\0Vowel-peak sub-timing\0Viseme keys (Actor 3D)\0\0";
        ImGui::Combo("Time placement##lpm", &lipMode, mitems);
        ImGui::SetNextItemWidth(160.f);
        ImGui::InputInt("Max keyframes (safety)##lmax", &lipMaxK);
        lipMaxK = (std::max)(1, (std::min)(lipMaxK, 1 << 20));
        ImGui::TextDisabled("Modes 0–1: MG Depth placeholder. Mode 2: English letter→viseme keys on Actor (FacialViseme* channels).");
        if (ImGui::Button("Extract morpheme-like tokens + stats##lp")) {
            const auto toks = ExtractMorphemeLikeTokens(std::string(lipText));
            const Smm::LipsyncMorpheme::MorphemeStats ms = Smm::LipsyncMorpheme::ComputeMorphemeStats(std::string(lipText));
            std::string s;
            for (size_t k = 0; k < toks.size() && k < 12; ++k) {
                s += toks[k];
                if (k + 1 < toks.size() && k + 1 < 12) {
                    s += " | ";
                }
            }
            char b[200];
            std::snprintf(b, sizeof(b), "Morpheme tokens: %s — words~%zu, uni~%zu, vowelDensity~%.2f", s.c_str(), ms.WordCount, ms.UniqueTokens,
                static_cast<double>(ms.VowelDensity));
            statusLine = b;
        }
        LipsyncLineStub stub;
        stub.Label = lipLabel;
        stub.Text = lipText;
        stub.StartTick = t0;
        stub.EndTick = t1;
        stub.Strength = lipStrength;
        stub.PhoneticMode = static_cast<uint8_t>(std::clamp(lipMode, 0, 2));
        stub.MaxKeyframes = static_cast<uint32_t>(lipMaxK);
        std::vector<LipsyncSample> sm;
        std::string pvwarn;
        if (lipMode == 2) {
            std::vector<Solstice::Arzachel::VisemeKeyframeTick> vk;
            std::string vw;
            Solstice::Arzachel::BuildVisemeKeyframesFromEnglishText(stub.Text, (std::max)(scene.GetTicksPerSecond(), 1u), stub.StartTick,
                stub.EndTick, stub.MaxKeyframes, vk, vw);
            if (!vw.empty()) {
                ImGui::TextDisabled("Viseme preview: %s", vw.c_str());
            }
            ImGui::TextDisabled("Viseme ids (sample):");
            for (size_t s = 0; s < vk.size() && s < 16; ++s) {
                ImGui::Text("  tick %llu  %s  str %.2f", static_cast<unsigned long long>(vk[s].TimeTicks), vk[s].VisemeId.c_str(),
                    static_cast<double>(vk[s].Strength));
            }
        } else {
            Smm::LipsyncMorpheme::BuildLipsyncSamples(
                stub, (std::max)(scene.GetTicksPerSecond(), 1u), sm, pvwarn);
            if (!pvwarn.empty() && pvwarn[0] != 0) {
                ImGui::TextDisabled("Preview: %s", pvwarn.c_str());
            }
            ImGui::TextDisabled("Mouth open heuristic (per sample):");
            for (size_t s = 0; s < sm.size() && s < 16; ++s) {
                ImGui::Text(
                    "  tick %llu  open %.2f", static_cast<unsigned long long>(sm[s].Tick), static_cast<double>(sm[s].MouthOpen));
            }
        }
        if (ImGui::Button("Apply Depth key wiggle to selected MGText##lp")) {
            if (lipMode == 2) {
                statusLine = "Use “Apply viseme keys…” for mode 2 (Actor).";
            } else if (mgElementSelected < 0 || static_cast<size_t>(mgElementSelected) >= scene.GetMGElements().size()) {
                statusLine = "Select an MGText element in Properties list.";
            } else {
                Smm::PushSceneUndoSnapshot(scene, compressPrlx);
                std::string e2;
                if (TryApplyLipsyncPlaceholderToMgText(
                        stub, scene, static_cast<Solstice::Parallax::MGIndex>(mgElementSelected), e2)) {
                    sceneDirty = true;
                    statusLine = "Lipsync: Depth track keyframes (placeholder). " + e2;
                } else {
                    statusLine = e2;
                }
            }
        }
        if (ImGui::Button("Apply viseme keys to selected Actor##lp")) {
            if (elementSelected < 0 || static_cast<size_t>(elementSelected) >= scene.GetElements().size()) {
                statusLine = "Select an ActorElement in the scene tree / Properties.";
            } else {
                Smm::PushSceneUndoSnapshot(scene, compressPrlx);
                std::string e2;
                if (TryApplyLipsyncVisemeToActor(
                        stub, scene, static_cast<Solstice::Parallax::ElementIndex>(elementSelected), e2)) {
                    sceneDirty = true;
                    statusLine = "Lipsync: FacialVisemeId/Weight channels. " + e2;
                } else {
                    statusLine = e2;
                }
            }
        }
        if (ImGui::Button("Store line stub in list##lp")) {
            session.LipsyncStubs.push_back(stub);
            statusLine = "Lipsync stub added to TSV (save to persist).";
        }
    }
    ImGui::PopID();
}

} // namespace Smm::Authoring
