#include "PreviewPanels.hxx"

#include "Editing/SmmParticleEditor.hxx"
#include "SmmFileOps.hxx"
#include "EditorEnginePreview/EditorEnginePreview.hxx"
#include "LibUI/Tools/DiagLog.hxx"
#include "LibUI/Tools/ViewportSpatialPick.hxx"
#include "LibUI/Viewport/Viewport.hxx"
#include "LibUI/Viewport/ViewportGizmo.hxx"
#include "LibUI/Viewport/ViewportInteraction.hxx"
#include "LibUI/Viewport/ViewportMath.hxx"
#include "LibUI/Widgets/Widgets.hxx"

#include <Arzachel/FacialAnimation.hxx>
#include <Parallax/MGRaster.hxx>
#include <Parallax/ParallaxEditorHelpers.hxx>
#include <Parallax/ParallaxScene.hxx>

#include <Physics/Lighting/LightSource.hxx>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <limits>
#include <span>
#include <string>
#include <unordered_set>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace Solstice::MovieMaker::UI::Panels {

namespace {

float PreviewMouthOpenHint(const Solstice::Parallax::SceneEvaluationResult& ev, Solstice::Parallax::ElementIndex ei) {
    for (const Solstice::Parallax::ActorFacialPose& fp : ev.ActorFacialPoses) {
        if (fp.Element != ei) {
            continue;
        }
        float m = 0.f;
        const auto jaw = fp.BoneDeltasByName.find("jaw");
        if (jaw != fp.BoneDeltasByName.end()) {
            m = (std::max)(m, std::clamp(-jaw->second.Translation.y * 12.f, 0.f, 1.f));
        }
        const uint32_t jh = Solstice::Arzachel::MorphNameHash("jaw_open");
        const auto mj = fp.MorphWeights.find(jh);
        if (mj != fp.MorphWeights.end()) {
            m = (std::max)(m, std::clamp(mj->second, 0.f, 1.f));
        }
        const uint32_t bh = Solstice::Arzachel::MorphNameHash("blink");
        const auto blink = fp.MorphWeights.find(bh);
        if (blink != fp.MorphWeights.end()) {
            m = (std::max)(m, std::clamp(blink->second * 0.25f, 0.f, 0.15f));
        }
        return m;
    }
    return 0.f;
}

void DrawCinematicFramingOverlays(ImDrawList* dl, const ImVec2& imin, const ImVec2& imax) {
    const float w = imax.x - imin.x;
    const float h = imax.y - imin.y;
    if (w < 12.f || h < 12.f) {
        return;
    }
    const ImU32 col = IM_COL32(255, 240, 200, 130);
    const float th = 1.0f;
    // Title / action ~10% safe area
    const float m = 0.1f;
    const float sx = imin.x + w * m;
    const float sy = imin.y + h * m;
    const float ex = imax.x - w * m;
    const float ey = imax.y - h * m;
    dl->AddRect(ImVec2(sx, sy), ImVec2(ex, ey), col, 0.f, 0, th);
    for (int i = 1; i <= 2; ++i) {
        const float fx = imin.x + w * (float)i / 3.f;
        const float fy = imin.y + h * (float)i / 3.f;
        dl->AddLine(ImVec2(fx, imin.y), ImVec2(fx, imax.y), col, th);
        dl->AddLine(ImVec2(imin.x, fy), ImVec2(imax.x, fy), col, th);
    }
    const float cx = (imin.x + imax.x) * 0.5f;
    const float cy = (imin.y + imax.y) * 0.5f;
    const float d = (std::min)(w, h) * 0.04f;
    dl->AddLine(ImVec2(cx - d, cy), ImVec2(cx + d, cy), col, th);
    dl->AddLine(ImVec2(cx, cy - d), ImVec2(cx, cy + d), col, th);
}

constexpr int kSmmMaxViewportPickBoxes = 2048;
constexpr std::uint16_t kPickLayerSceneElement = 1;
constexpr std::uint16_t kPickLayerMgWorldSprite = 2;

static int SmmPrimarySel(const UnifiedViewportSettings& s) {
    return s.primaryElementIndex ? *s.primaryElementIndex : -1;
}

static bool SmmIsElementViewportSelected(const UnifiedViewportSettings& s, int elementIndex) {
    if (s.viewportSelectedElements && !s.viewportSelectedElements->empty()) {
        return s.viewportSelectedElements->find(elementIndex) != s.viewportSelectedElements->end();
    }
    const int p = SmmPrimarySel(s);
    return p >= 0 && p == elementIndex;
}

static void SmmBuildUnifiedPickLists(const Solstice::Parallax::SceneEvaluationResult& eval,
    Solstice::Parallax::ElementIndex particleEmitterElement, const Solstice::Math::Vec3& emitterWorld,
    bool includeParticle, bool includeWorldMg, std::vector<LibUI::Tools::AxisAlignedBox3>& boxes,
    std::vector<LibUI::Viewport::PickToken>& tokens) {
    boxes.clear();
    tokens.clear();
    const float heEl = Solstice::EditorEnginePreview::kSchematicPreviewHalfExtent;
    const float heLt = 0.28f;
    const float hePart = 0.42f;
    for (const auto& et : eval.ElementTransforms) {
        if (static_cast<int>(boxes.size()) >= kSmmMaxViewportPickBoxes) {
            break;
        }
        LibUI::Tools::AxisAlignedBox3 b{};
        b.minX = et.Position.x - heEl;
        b.minY = et.Position.y - heEl;
        b.minZ = et.Position.z - heEl;
        b.maxX = et.Position.x + heEl;
        b.maxY = et.Position.y + heEl;
        b.maxZ = et.Position.z + heEl;
        boxes.push_back(b);
        tokens.push_back(LibUI::Viewport::MakePickToken(kPickLayerSceneElement, static_cast<std::uint64_t>(et.Element)));
    }
    for (const auto& ls : eval.LightStates) {
        if (static_cast<int>(boxes.size()) >= kSmmMaxViewportPickBoxes) {
            break;
        }
        LibUI::Tools::AxisAlignedBox3 b{};
        b.minX = ls.Position.x - heLt;
        b.minY = ls.Position.y - heLt;
        b.minZ = ls.Position.z - heLt;
        b.maxX = ls.Position.x + heLt;
        b.maxY = ls.Position.y + heLt;
        b.maxZ = ls.Position.z + heLt;
        boxes.push_back(b);
        tokens.push_back(LibUI::Viewport::MakePickToken(kPickLayerSceneElement, static_cast<std::uint64_t>(ls.Element)));
    }
    for (const auto& fv : eval.FluidVolumes) {
        if (!fv.Enabled) {
            continue;
        }
        if (static_cast<int>(boxes.size()) >= kSmmMaxViewportPickBoxes) {
            break;
        }
        LibUI::Tools::AxisAlignedBox3 b{};
        b.minX = (std::min)(fv.BoundsMin.x, fv.BoundsMax.x);
        b.minY = (std::min)(fv.BoundsMin.y, fv.BoundsMax.y);
        b.minZ = (std::min)(fv.BoundsMin.z, fv.BoundsMax.z);
        b.maxX = (std::max)(fv.BoundsMin.x, fv.BoundsMax.x);
        b.maxY = (std::max)(fv.BoundsMin.y, fv.BoundsMax.y);
        b.maxZ = (std::max)(fv.BoundsMin.z, fv.BoundsMax.z);
        boxes.push_back(b);
        tokens.push_back(LibUI::Viewport::MakePickToken(kPickLayerSceneElement, static_cast<std::uint64_t>(fv.Element)));
    }
    if (includeWorldMg) {
        for (const auto& mgw : eval.MGWorldSprites) {
            if (static_cast<int>(boxes.size()) >= kSmmMaxViewportPickBoxes) {
                break;
            }
            const float he = (std::max)(0.18f, 0.45f * (std::max)({std::abs(mgw.Scale.x), std::abs(mgw.Scale.y), std::abs(mgw.Scale.z)}));
            LibUI::Tools::AxisAlignedBox3 b{};
            b.minX = mgw.Position.x - he;
            b.minY = mgw.Position.y - he;
            b.minZ = mgw.Position.z - he;
            b.maxX = mgw.Position.x + he;
            b.maxY = mgw.Position.y + he;
            b.maxZ = mgw.Position.z + he;
            boxes.push_back(b);
            tokens.push_back(LibUI::Viewport::MakePickToken(kPickLayerMgWorldSprite, static_cast<std::uint64_t>(mgw.MGElement)));
        }
    }
    if (includeParticle && particleEmitterElement != Solstice::Parallax::PARALLAX_INVALID_INDEX
        && static_cast<int>(boxes.size()) < kSmmMaxViewportPickBoxes) {
        LibUI::Tools::AxisAlignedBox3 b{};
        b.minX = emitterWorld.x - hePart;
        b.minY = emitterWorld.y - hePart;
        b.minZ = emitterWorld.z - hePart;
        b.maxX = emitterWorld.x + hePart;
        b.maxY = emitterWorld.y + hePart;
        b.maxZ = emitterWorld.z + hePart;
        boxes.push_back(b);
        tokens.push_back(LibUI::Viewport::MakePickToken(kPickLayerSceneElement, static_cast<std::uint64_t>(particleEmitterElement)));
    }
}

static void SmmApplyElementWorldDelta(Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::ElementIndex ei, float dx, float dy,
    float dz, bool compressPrlx, bool& sceneDirty) {
    const std::string_view st = Solstice::Parallax::GetElementSchema(scene, ei);
    if (st == "SmmFluidVolumeElement") {
        Solstice::Parallax::AttributeValue avMn = Solstice::Parallax::GetAttribute(scene, ei, "BoundsMin");
        Solstice::Parallax::AttributeValue avMx = Solstice::Parallax::GetAttribute(scene, ei, "BoundsMax");
        const auto* mn = std::get_if<Solstice::Math::Vec3>(&avMn);
        const auto* mx = std::get_if<Solstice::Math::Vec3>(&avMx);
        if (mn && mx) {
            Smm::PushSceneUndoSnapshot(scene, compressPrlx);
            Solstice::Parallax::SetAttribute(
                scene, ei, "BoundsMin", Solstice::Parallax::AttributeValue{Solstice::Math::Vec3{mn->x + dx, mn->y + dy, mn->z + dz}});
            Solstice::Parallax::SetAttribute(
                scene, ei, "BoundsMax", Solstice::Parallax::AttributeValue{Solstice::Math::Vec3{mx->x + dx, mx->y + dy, mx->z + dz}});
            sceneDirty = true;
        }
        return;
    }
    const Solstice::Parallax::AttributeValue posAv = Solstice::Parallax::GetAttribute(scene, ei, "Position");
    Solstice::Math::Vec3 pos{0.f, 0.f, 0.f};
    if (const auto* p = std::get_if<Solstice::Math::Vec3>(&posAv)) {
        pos = *p;
    }
    Smm::PushSceneUndoSnapshot(scene, compressPrlx);
    Solstice::Parallax::SetAttribute(scene, ei, "Position",
        Solstice::Parallax::AttributeValue{Solstice::Math::Vec3{pos.x + dx, pos.y + dy, pos.z + dz}});
    sceneDirty = true;
}

static float ReadElementFloat(const Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::ElementIndex ei,
    std::string_view key, float fallback) {
    const Solstice::Parallax::AttributeValue v = Solstice::Parallax::GetAttribute(scene, ei, key);
    if (const auto* f = std::get_if<float>(&v)) {
        return *f;
    }
    return fallback;
}

static Solstice::Math::Vec3 ReadElementVec3(const Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::ElementIndex ei,
    std::string_view key, const Solstice::Math::Vec3& fallback) {
    const Solstice::Parallax::AttributeValue v = Solstice::Parallax::GetAttribute(scene, ei, key);
    if (const auto* p = std::get_if<Solstice::Math::Vec3>(&v)) {
        return *p;
    }
    return fallback;
}

static void SmmReadElementEulerDegrees(
    const Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::ElementIndex ei, float& outPitch, float& outYaw, float& outRoll) {
    outPitch = ReadElementFloat(scene, ei, "PitchDeg", ReadElementFloat(scene, ei, "PitchDegrees", 0.f));
    outYaw = ReadElementFloat(scene, ei, "YawDeg", ReadElementFloat(scene, ei, "YawDegrees", 0.f));
    outRoll = ReadElementFloat(scene, ei, "RollDeg", ReadElementFloat(scene, ei, "RollDegrees", 0.f));
}

static void SmmSetElementEulerDegrees(Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::ElementIndex ei,
    float pitchDeg, float yawDeg, float rollDeg) {
    Solstice::Parallax::SetAttribute(scene, ei, "PitchDegrees", Solstice::Parallax::AttributeValue{pitchDeg});
    Solstice::Parallax::SetAttribute(scene, ei, "YawDegrees", Solstice::Parallax::AttributeValue{yawDeg});
    Solstice::Parallax::SetAttribute(scene, ei, "RollDegrees", Solstice::Parallax::AttributeValue{rollDeg});
}

static Solstice::Math::Vec2 ReadMgVec2(
    const Solstice::Parallax::MGElementRecord& mg, std::string_view key, const Solstice::Math::Vec2& fallback) {
    const auto it = mg.Attributes.find(std::string(key));
    if (it == mg.Attributes.end()) {
        return fallback;
    }
    if (const auto* v = std::get_if<Solstice::Math::Vec2>(&it->second)) {
        return *v;
    }
    return fallback;
}

static Solstice::Math::Vec3 ReadMgVec3(
    const Solstice::Parallax::MGElementRecord& mg, std::string_view key, const Solstice::Math::Vec3& fallback) {
    const auto it = mg.Attributes.find(std::string(key));
    if (it == mg.Attributes.end()) {
        return fallback;
    }
    if (const auto* v = std::get_if<Solstice::Math::Vec3>(&it->second)) {
        return *v;
    }
    return fallback;
}

static float ReadMgFloat(const Solstice::Parallax::MGElementRecord& mg, std::string_view key, float fallback) {
    const auto it = mg.Attributes.find(std::string(key));
    if (it == mg.Attributes.end()) {
        return fallback;
    }
    if (const auto* v = std::get_if<float>(&it->second)) {
        return *v;
    }
    return fallback;
}

static Solstice::Math::Vec2 SmmMgAccumulatedParentOffset(const Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::MGIndex mgIndex) {
    Solstice::Math::Vec2 o{0.f, 0.f};
    const auto& mg = scene.GetMGElements();
    if (mgIndex >= mg.size()) {
        return o;
    }
    Solstice::Parallax::MGIndex p = mg[mgIndex].Parent;
    while (p != Solstice::Parallax::PARALLAX_INVALID_INDEX && p < mg.size()) {
        const Solstice::Math::Vec2 pp = ReadMgVec2(mg[p], "Position", Solstice::Math::Vec2{0.f, 0.f});
        o.x += pp.x;
        o.y += pp.y;
        p = mg[p].Parent;
    }
    return o;
}

static bool PointInQuad(const ImVec2& p, const ImVec2& a, const ImVec2& b, const ImVec2& c, const ImVec2& d) {
    auto sameSide = [](const ImVec2& p0, const ImVec2& p1, const ImVec2& p2, const ImVec2& q) {
        const float x1 = p2.x - p1.x;
        const float y1 = p2.y - p1.y;
        const float cp = x1 * (q.y - p1.y) - y1 * (q.x - p1.x);
        const float cq = x1 * (p0.y - p1.y) - y1 * (p0.x - p1.x);
        return cp * cq >= 0.f;
    };
    return sameSide(a, b, c, p) && sameSide(b, c, d, p) && sameSide(c, d, a, p) && sameSide(d, a, b, p);
}

static int HitCornerHandle(const ImVec2 (&corners)[4], const ImVec2& mousePos, float radiusPx) {
    const float rr = radiusPx * radiusPx;
    for (int i = 0; i < 4; ++i) {
        const float dx = mousePos.x - corners[i].x;
        const float dy = mousePos.y - corners[i].y;
        if (dx * dx + dy * dy <= rr) {
            return i;
        }
    }
    return -1;
}

static void SmmRotateSpriteCornersComp(float posX, float posY, float sizeX, float sizeY, float rotRad, ImVec2 (&outCorners)[4]) {
    const float hw = sizeX * 0.5f;
    const float hh = sizeY * 0.5f;
    const float cx = posX + hw;
    const float cy = posY + hh;
    const float co = std::cos(rotRad);
    const float si = std::sin(rotRad);
    const float lxs[4] = {-hw, hw, hw, -hw};
    const float lys[4] = {-hh, -hh, hh, hh};
    for (int i = 0; i < 4; ++i) {
        const float lx = lxs[i];
        const float ly = lys[i];
        outCorners[i].x = cx + lx * co - ly * si;
        outCorners[i].y = cy + lx * si + ly * co;
    }
}

static void WriteSink(char* sink, size_t sinkBytes, const char* msg) {
    if (sink && sinkBytes > 0 && msg) {
        std::snprintf(sink, sinkBytes, "%s", msg);
    }
}

static void ClearSink(char* sink, size_t sinkBytes) {
    if (sink && sinkBytes > 0) {
        sink[0] = '\0';
    }
}

void BlendMgOverScene(std::vector<std::byte>& dstRgba, int w, int h, const std::vector<std::byte>& srcRgba, float a) {
    const size_t expected = static_cast<size_t>(w) * static_cast<size_t>(h) * 4u;
    if (dstRgba.size() != expected || srcRgba.size() != expected) {
        return;
    }
    auto* d = reinterpret_cast<uint8_t*>(dstRgba.data());
    const auto* s = reinterpret_cast<const uint8_t*>(srcRgba.data());
    const float as = std::clamp(a, 0.f, 1.f);
    const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
    for (size_t i = 0; i < n; ++i) {
        uint8_t* dp = d + i * 4u;
        const uint8_t* sp = s + i * 4u;
        const float sa = (static_cast<float>(sp[3]) / 255.f) * as;
        if (sa < 1e-4f) {
            continue;
        }
        const float inv = 1.f - sa;
        for (int c = 0; c < 3; ++c) {
            dp[c] = static_cast<uint8_t>(std::clamp(sa * static_cast<float>(sp[c]) + inv * static_cast<float>(dp[c]), 0.f, 255.f));
        }
        dp[3] = static_cast<uint8_t>(std::clamp(sa * 255.f + inv * static_cast<float>(dp[3]), 0.f, 255.f));
    }
}

static int CountMgSpritesInEntry(const Solstice::Parallax::MGDisplayList::Entry& e) {
    int n = (e.SchemaType == "MGSpriteElement") ? 1 : 0;
    for (const auto& c : e.Children) {
        n += CountMgSpritesInEntry(c);
    }
    return n;
}

static int CountMgSprites(const Solstice::Parallax::MGDisplayList& list) {
    int n = 0;
    for (const auto& e : list.Entries) {
        n += CountMgSpritesInEntry(e);
    }
    return n;
}

static bool IsMgSpriteWorldMode(const Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::MGIndex mgIndex) {
    if (mgIndex >= scene.GetMGElements().size()) {
        return false;
    }
    const auto& mg = scene.GetMGElements()[mgIndex];
    if (mg.SchemaIndex >= scene.GetSchemas().size() || scene.GetSchemas()[mg.SchemaIndex].TypeName != "MGSpriteElement") {
        return false;
    }
    const auto it = mg.Attributes.find("MGProjectionMode");
    if (it == mg.Attributes.end()) {
        return false;
    }
    if (const auto* mode = std::get_if<int32_t>(&it->second)) {
        return *mode == 1;
    }
    return false;
}

static bool SceneHasActorMeshAssets(const Solstice::Parallax::ParallaxScene& scene) {
    for (const auto& el : scene.GetElements()) {
        if (el.SchemaIndex >= scene.GetSchemas().size()) {
            continue;
        }
        if (scene.GetSchemas()[el.SchemaIndex].TypeName != "ActorElement") {
            continue;
        }
        auto it = el.Attributes.find("MeshAsset");
        if (it == el.Attributes.end()) {
            continue;
        }
        if (const auto* h = std::get_if<uint64_t>(&it->second); h && *h != 0ull) {
            return true;
        }
    }
    return false;
}

static Solstice::Parallax::MGDisplayList FilterWorldMgEntriesForRaster(
    const Solstice::Parallax::ParallaxScene& scene, const Solstice::Parallax::MGDisplayList& in) {
    Solstice::Parallax::MGDisplayList out = in;
    out.Entries.clear();
    out.Entries.reserve(in.Entries.size());
    const size_t n = (std::min)(in.Entries.size(), scene.GetMGElements().size());
    for (size_t i = 0; i < n; ++i) {
        if (IsMgSpriteWorldMode(scene, static_cast<Solstice::Parallax::MGIndex>(i))) {
            continue;
        }
        out.Entries.push_back(in.Entries[i]);
    }
    return out;
}

#if defined(_WIN32)
static bool CaptureOrbitRgbSafe(LibUI::Viewport::OrbitPanZoomState& nav, float cx, float cy, float cz, float fovYDeg, float aspect,
    int viewportW, int viewportH, const Solstice::EditorEnginePreview::PreviewEntity* entities, size_t entityCount,
    const Solstice::Physics::LightSource* lights, size_t lightCount, std::vector<std::byte>& outRgba, int& outW, int& outH,
    unsigned long& outSehCode) {
    outSehCode = 0;
    __try {
        return Solstice::EditorEnginePreview::CaptureOrbitRgb(nav, cx, cy, cz, fovYDeg, aspect, viewportW, viewportH, entities,
            entityCount, lights, lightCount, outRgba, outW, outH);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        outSehCode = GetExceptionCode();
        return false;
    }
}
#endif

} // namespace

/// First frames after scene create / replace skip bgfx capture (Windows driver init ordering).
static int s_unifiedGpuWarmupFramesRemaining = 0;
/// After repeated `CaptureOrbitRgb` failures, optional session disables GPU preview (reset on app restart).
static int s_unifiedCaptureFailStreak = 0;
static constexpr int kUnifiedWarmupFrames = 3;
static constexpr int kUnifiedFailsBeforeSessionDisable = 3;

void ResetUnifiedViewportEnginePreviewWarmup() {
    s_unifiedGpuWarmupFramesRemaining = kUnifiedWarmupFrames;
    s_unifiedCaptureFailStreak = 0;
}

void DrawScene3dSchematicPanel(SDL_Window* window, const Solstice::Parallax::ParallaxScene& scene, uint64_t timeTicks,
    LibUI::Graphics::PreviewTextureRgba& previewTexture, float preferredHeight) {
    static LibUI::Viewport::OrbitPanZoomState s_sceneNav{};
    LibUI::Viewport::Frame sceneVp{};
    const float clampedHeight = std::max(220.0f, preferredHeight);
    if (!LibUI::Viewport::BeginHost("smm_scene_3d_viewport", ImVec2(-1, clampedHeight), true)) {
        return;
    }

    if (LibUI::Viewport::PollFrame(sceneVp) && sceneVp.draw_list) {
        Solstice::Parallax::SceneEvaluationResult eval{};
        Solstice::Parallax::EvaluateScene(scene, timeTicks, eval);
        const float aspect = std::max(sceneVp.size.y, 1.0f) > 0.f ? sceneVp.size.x / std::max(sceneVp.size.y, 1.0f) : 1.f;
        const int scW = std::max(2, static_cast<int>(sceneVp.size.x));
        const int scH = std::max(2, static_cast<int>(sceneVp.size.y));

        std::vector<Solstice::EditorEnginePreview::PreviewEntity> entities;
        entities.reserve(eval.ElementTransforms.size());
        for (const auto& et : eval.ElementTransforms) {
            Solstice::EditorEnginePreview::PreviewEntity pe{};
            pe.Position = et.Position;
            const std::string_view schemaType = Solstice::Parallax::GetElementSchema(scene, et.Element);
            if (schemaType == "CameraElement") {
                pe.Albedo = Solstice::Math::Vec3(1.f, 0.7f, 0.2f);
            } else if (schemaType == "ActorElement") {
                pe.Albedo = Solstice::Math::Vec3(0.35f, 0.62f, 1.f);
            } else {
                pe.Albedo = Solstice::Math::Vec3(0.62f, 0.66f, 0.78f);
            }
            pe.HalfExtent = Solstice::EditorEnginePreview::kSchematicPreviewHalfExtent;
            pe.BakedAOPreview = 0.42f;
            entities.push_back(pe);
        }

        std::vector<Solstice::Physics::LightSource> lights;
        {
            Solstice::Physics::LightSource sun{};
            sun.Type = Solstice::Physics::LightSource::LightType::Directional;
            sun.Position = Solstice::Math::Vec3(0.4f, 0.82f, 0.38f).Normalized();
            sun.Color = Solstice::Math::Vec3(1.f, 0.96f, 0.88f);
            sun.Intensity = 1.15f;
            lights.push_back(sun);
        }
        for (const auto& ls : eval.LightStates) {
            Solstice::Physics::LightSource pl{};
            pl.Type = Solstice::Physics::LightSource::LightType::Point;
            pl.Position = ls.Position;
            pl.Color = Solstice::Math::Vec3(ls.Color.x, ls.Color.y, ls.Color.z);
            pl.Intensity = std::max(0.35f, ls.Intensity);
            pl.Range = 48.f;
            lights.push_back(pl);
        }

        std::vector<std::byte> capture;
        int capW = 0;
        int capH = 0;
        if (Solstice::EditorEnginePreview::CaptureOrbitRgb(s_sceneNav, 0.f, 0.f, 0.f, 55.f, aspect, scW, scH, entities.data(),
                entities.size(), lights.data(), lights.size(), capture, capW, capH)) {
            previewTexture.SetSizeUpload(window, static_cast<uint32_t>(capW), static_cast<uint32_t>(capH), capture.data(),
                capture.size());
        }

        if (previewTexture.Valid()) {
            LibUI::Viewport::DrawTextureLetterboxed(sceneVp.draw_list, previewTexture.ImGuiTexId(), sceneVp.min, sceneVp.max,
                static_cast<float>(previewTexture.width), static_cast<float>(previewTexture.height));
        } else {
            LibUI::Viewport::DrawCheckerboard(
                sceneVp.draw_list, sceneVp.min, sceneVp.max, 14.f, IM_COL32(32, 32, 42, 255), IM_COL32(24, 24, 30, 255));
        }

        ImVec2 projScMin = sceneVp.min;
        ImVec2 projScMax = sceneVp.max;
        if (previewTexture.Valid() && previewTexture.width > 0 && previewTexture.height > 0) {
            LibUI::Viewport::ComputeLetterbox(sceneVp.min, sceneVp.max, static_cast<float>(previewTexture.width),
                static_cast<float>(previewTexture.height), projScMin, projScMax);
        }

        LibUI::Viewport::Mat4Col viewM{};
        LibUI::Viewport::Mat4Col projM{};
        LibUI::Viewport::ComputeOrbitViewProjectionColMajor(
            s_sceneNav, 0.f, 0.f, 0.f, 55.f, aspect, 0.12f, 2048.f, viewM, projM);
        LibUI::Viewport::DrawXZGrid(sceneVp.draw_list, projScMin, projScMax, viewM, projM, 1.f,
            IM_COL32(72, 72, 92, 200), 24);

        for (const auto& et : eval.ElementTransforms) {
            std::string_view schemaType = Solstice::Parallax::GetElementSchema(scene, et.Element);
            ImU32 col = IM_COL32(140, 200, 255, 255);
            if (schemaType == "CameraElement") {
                col = IM_COL32(255, 210, 90, 255);
            } else if (schemaType == "ActorElement") {
                col = IM_COL32(120, 220, 255, 255);
            }
            LibUI::Viewport::DrawWorldCrossXZ(
                sceneVp.draw_list, projScMin, projScMax, viewM, projM, et.Position.x, et.Position.y, et.Position.z, 0.3f, col);
        }

        for (const auto& ls : eval.LightStates) {
            ImVec2 sp{};
            if (LibUI::Viewport::WorldToScreen(
                    viewM, projM, ls.Position.x, ls.Position.y, ls.Position.z, projScMin, projScMax, sp)) {
                const ImU32 lcol = IM_COL32(static_cast<int>(ls.Color.x * 255.f), static_cast<int>(ls.Color.y * 255.f),
                    static_cast<int>(ls.Color.z * 255.f), 255);
                sceneVp.draw_list->AddCircleFilled(sp, 6.f, lcol);
                sceneVp.draw_list->AddCircle(sp, 7.f, IM_COL32(255, 255, 255, 200));
            }
        }

        LibUI::Viewport::ApplyOrbitPanZoom(s_sceneNav, sceneVp);
        LibUI::Viewport::DrawViewportLabel(
            sceneVp.draw_list, sceneVp.min, sceneVp.max, "Schematic 3D — bgfx + EvaluateScene lights", ImVec2(1.0f, 0.0f));
    }

    LibUI::Viewport::EndHost();
}

void DrawMotionGraphicsPreviewPanel(SDL_Window* window, const Solstice::Parallax::ParallaxScene& scene,
    Solstice::Parallax::DevSessionAssetResolver& resolver, uint64_t timeTicks,
    LibUI::Graphics::PreviewTextureRgba& previewTexture, float preferredHeight) {
    LibUI::Viewport::Frame mgVp{};
    const float clampedHeight = std::max(200.0f, preferredHeight);
    if (!LibUI::Viewport::BeginHost("smm_mg_viewport", ImVec2(-1, clampedHeight), true)) {
        return;
    }
    if (LibUI::Viewport::PollFrame(mgVp) && mgVp.draw_list) {
        const int iw = std::max(2, static_cast<int>(mgVp.size.x));
        const int ih = std::max(2, static_cast<int>(mgVp.size.y));
        static thread_local std::vector<std::byte> s_MgRgbaScratch;
        s_MgRgbaScratch.resize(static_cast<size_t>(iw) * static_cast<size_t>(ih) * 4u);

        Solstice::Parallax::MGDisplayList mgList = Solstice::Parallax::EvaluateMG(scene, timeTicks);
        Solstice::Parallax::RasterizeMGDisplayList(mgList, &resolver, static_cast<uint32_t>(iw), static_cast<uint32_t>(ih),
            std::span<std::byte>(s_MgRgbaScratch.data(), s_MgRgbaScratch.size()));
        previewTexture.SetSizeUpload(
            window, static_cast<uint32_t>(iw), static_cast<uint32_t>(ih), s_MgRgbaScratch.data(), s_MgRgbaScratch.size());

        if (previewTexture.Valid()) {
            LibUI::Viewport::DrawTextureLetterboxed(
                mgVp.draw_list, previewTexture.ImGuiTexId(), mgVp.min, mgVp.max, static_cast<float>(iw), static_cast<float>(ih));
        } else {
            LibUI::Viewport::DrawCheckerboard(
                mgVp.draw_list, mgVp.min, mgVp.max, 12.0f, IM_COL32(38, 38, 48, 255), IM_COL32(26, 26, 34, 255));
        }

        char label[192]{};
        std::snprintf(label, sizeof(label), "2D tick %llu  |  %dx%d", static_cast<unsigned long long>(timeTicks), iw, ih);
        LibUI::Viewport::DrawViewportLabel(mgVp.draw_list, mgVp.min, mgVp.max, label, ImVec2(1.0f, 0.0f));
    }
    LibUI::Viewport::EndHost();
}

void DrawUnifiedViewportPanel(SDL_Window* window, Solstice::Parallax::ParallaxScene& scene,
    Solstice::Parallax::DevSessionAssetResolver& resolver, uint64_t timeTicks,
    LibUI::Graphics::PreviewTextureRgba& previewTexture, float preferredHeight, Smm::Editing::ParticleEditorState* particles,
    LibUI::Graphics::PreviewTextureRgba* particleSpriteTexture, const Solstice::Math::Vec3& emitterWorld, float mgOverlayAlpha,
    const UnifiedViewportSettings& settings) {
    static LibUI::Viewport::OrbitPanZoomState s_fallbackNav{};
    LibUI::Viewport::OrbitPanZoomState& nav = settings.camera ? *settings.camera : s_fallbackNav;
    LibUI::Viewport::Frame vp{};
    const float clampedHeight = std::max(220.0f, preferredHeight);
    if (!LibUI::Viewport::BeginHost("smm_unified_viewport", ImVec2(-1, clampedHeight), true)) {
        return;
    }

    if (LibUI::Viewport::PollFrame(vp) && vp.draw_list) {
        static LibUI::Viewport::TransformTool s_viewTool = LibUI::Viewport::TransformTool::Translate;
        static LibUI::Viewport::TransformAxis s_viewAxis = LibUI::Viewport::TransformAxis::None;
        (void)LibUI::Viewport::ApplyStandardTransformToolHotkeys(s_viewTool, vp.hovered);
        (void)LibUI::Viewport::ApplyStandardTransformAxisHotkeys(s_viewAxis, vp.hovered);
#if defined(_WIN32)
        if (LibUI::Tools::EnvVarTruthy("SOLSTICE_SMM_DISABLE_ENGINE_PREVIEW")) {
            // Hard safety mode for Windows crash triage: avoid all preview capture/upload paths.
            LibUI::Viewport::DrawCheckerboard(
                vp.draw_list, vp.min, vp.max, 14.f, IM_COL32(32, 32, 42, 255), IM_COL32(24, 24, 30, 255));
            LibUI::Viewport::DrawViewportLabel(
                vp.draw_list, vp.min, vp.max, "Unified viewport (safe mode): engine preview disabled", ImVec2(1.0f, 0.0f));
            LibUI::Viewport::DrawViewportLabel(
                vp.draw_list, vp.min, vp.max, "Unset SOLSTICE_SMM_DISABLE_ENGINE_PREVIEW to re-enable", ImVec2(0.0f, 1.0f));
            LibUI::Viewport::ApplyOrbitPanZoom(nav, vp, {}, false);
            LibUI::Viewport::EndHost();
            return;
        }
#endif
        {
            const Solstice::EditorEnginePreview::CinematicViewStatePod z{};
            const Solstice::EditorEnginePreview::CinematicViewStatePod& cv = settings.cinematicView3D ? *settings.cinematicView3D : z;
            Solstice::EditorEnginePreview::SetPendingCinematicViewState(cv);
        }
        Solstice::Parallax::SceneEvaluationResult eval{};
        Solstice::Parallax::EvaluateScene(scene, timeTicks, eval);
        const Solstice::Parallax::MGDisplayList& mgList = eval.MotionGraphics;
        const bool unifiedWorldMgMode = (settings.mgWorkflowMode == 1);
        const Solstice::Parallax::MGDisplayList rasterMgList
            = unifiedWorldMgMode ? FilterWorldMgEntriesForRaster(scene, mgList) : mgList;

        Solstice::Parallax::MGIndex selectedMg = Solstice::Parallax::PARALLAX_INVALID_INDEX;
        bool selectedMgIsWorld = false;
        if (settings.selectedMgElementIndex && *settings.selectedMgElementIndex >= 0) {
            selectedMg = static_cast<Solstice::Parallax::MGIndex>(*settings.selectedMgElementIndex);
            selectedMgIsWorld = IsMgSpriteWorldMode(scene, selectedMg);
        }

        const int mgSpriteCount = CountMgSprites(mgList);
        const float aspect = std::max(vp.size.y, 1.0f) > 0.f ? vp.size.x / std::max(vp.size.y, 1.0f) : 1.f;
        const int scW = std::max(2, static_cast<int>(vp.size.x));
        const int scH = std::max(2, static_cast<int>(vp.size.y));

        const char* smatUtf8 = settings.previewSmatUtf8;
        const bool wantSmat = settings.usePreviewSmat && smatUtf8 && smatUtf8[0] != '\0';

        std::vector<Solstice::EditorEnginePreview::PreviewEntity> entities;
        entities.reserve(eval.ElementTransforms.size());
        for (const auto& et : eval.ElementTransforms) {
            Solstice::EditorEnginePreview::PreviewEntity pe{};
            pe.Position = et.Position;
            const std::string_view schemaType = Solstice::Parallax::GetElementSchema(scene, et.Element);
            if (schemaType == "CameraElement") {
                pe.Albedo = Solstice::Math::Vec3(1.f, 0.7f, 0.2f);
            } else if (schemaType == "ActorElement") {
                pe.Albedo = Solstice::Math::Vec3(0.35f, 0.62f, 1.f);
            } else {
                pe.Albedo = Solstice::Math::Vec3(0.62f, 0.66f, 0.78f);
            }
            pe.HalfExtent = Solstice::EditorEnginePreview::kSchematicPreviewHalfExtent;
            pe.Scale = ReadElementVec3(scene, et.Element, "Scale", Solstice::Math::Vec3{1.f, 1.f, 1.f});
            SmmReadElementEulerDegrees(scene, et.Element, pe.PitchDeg, pe.YawDeg, pe.RollDeg);
            if (schemaType == "ActorElement") {
                const float talk = PreviewMouthOpenHint(eval, et.Element);
                pe.Scale.y = 1.f + 0.22f * talk;
            }
            const bool isCamera = schemaType == "CameraElement";
            const bool isActor = schemaType == "ActorElement";
            const bool body = !isCamera && (isActor || !settings.smatActorsOnly);
            const bool selOk =
                !settings.smatSelectedOnly || SmmIsElementViewportSelected(settings, static_cast<int>(et.Element));
            const bool assignPreviewMats = body && selOk;
            if (wantSmat && assignPreviewMats) {
                std::strncpy(pe.PreviewSmatPath, smatUtf8, sizeof(pe.PreviewSmatPath) - 1);
                pe.PreviewSmatPath[sizeof(pe.PreviewSmatPath) - 1] = '\0';
            }
            if (settings.bindPreviewMaterialMaps && assignPreviewMats) {
                if (settings.previewMaterialAlbedoUtf8 && settings.previewMaterialAlbedoUtf8[0] != '\0') {
                    std::strncpy(pe.PreviewAlbedoTexturePath, settings.previewMaterialAlbedoUtf8,
                        sizeof(pe.PreviewAlbedoTexturePath) - 1);
                    pe.PreviewAlbedoTexturePath[sizeof(pe.PreviewAlbedoTexturePath) - 1] = '\0';
                }
                if (settings.previewMaterialNormalUtf8 && settings.previewMaterialNormalUtf8[0] != '\0') {
                    std::strncpy(pe.PreviewNormalTexturePath, settings.previewMaterialNormalUtf8,
                        sizeof(pe.PreviewNormalTexturePath) - 1);
                    pe.PreviewNormalTexturePath[sizeof(pe.PreviewNormalTexturePath) - 1] = '\0';
                }
                if (settings.previewMaterialRoughnessUtf8 && settings.previewMaterialRoughnessUtf8[0] != '\0') {
                    std::strncpy(pe.PreviewRoughnessTexturePath, settings.previewMaterialRoughnessUtf8,
                        sizeof(pe.PreviewRoughnessTexturePath) - 1);
                    pe.PreviewRoughnessTexturePath[sizeof(pe.PreviewRoughnessTexturePath) - 1] = '\0';
                }
            }
            pe.BakedAOPreview = settings.lowPolyAOPreview;
            entities.push_back(pe);
        }
        if (unifiedWorldMgMode) {
            for (const auto& mgw : eval.MGWorldSprites) {
                Solstice::EditorEnginePreview::PreviewEntity pe{};
                pe.Position = mgw.Position;
                pe.Albedo = Solstice::Math::Vec3(
                    std::clamp(mgw.Color.x, 0.f, 1.f), std::clamp(mgw.Color.y, 0.f, 1.f), std::clamp(mgw.Color.z, 0.f, 1.f));
                pe.Scale = mgw.Scale;
                pe.PitchDeg = mgw.PitchDeg;
                pe.YawDeg = mgw.YawDeg;
                pe.RollDeg = mgw.RollDeg;
                pe.HalfExtent = 0.5f;
                pe.UseQuadProxy = true;
                pe.CastShadows = mgw.CastShadows;
                entities.push_back(pe);
            }
        }

        std::vector<Solstice::Physics::LightSource> lights;
        {
            Solstice::Physics::LightSource sun{};
            sun.Type = Solstice::Physics::LightSource::LightType::Directional;
            sun.Position = Solstice::Math::Vec3(0.4f, 0.82f, 0.38f).Normalized();
            sun.Color = Solstice::Math::Vec3(1.f, 0.96f, 0.88f);
            sun.Intensity = 1.15f;
            lights.push_back(sun);
        }
        for (const auto& ls : eval.LightStates) {
            Solstice::Physics::LightSource pl{};
            pl.Type = Solstice::Physics::LightSource::LightType::Point;
            pl.Position = ls.Position;
            pl.Color = Solstice::Math::Vec3(ls.Color.x, ls.Color.y, ls.Color.z);
            pl.Intensity = std::max(0.35f, ls.Intensity);
            pl.Range = 48.f;
            lights.push_back(pl);
        }

        const bool sessionGpuOff
            = settings.enginePreviewSessionDisabled != nullptr && *settings.enginePreviewSessionDisabled;
#if defined(_WIN32)
        // Windows Intel/D3D11 preview path has been crash-prone during startup/new-scene capture.
        // Allow forcing MG/CPU viewport for debugging, while keeping engine preview enabled by default.
        const bool forceCpuPreview = LibUI::Tools::EnvVarTruthy("SOLSTICE_SMM_DISABLE_ENGINE_PREVIEW");
#else
        const bool forceCpuPreview = false;
#endif
        const bool pure2DNo3D = (settings.mgWorkflowMode == 0) && settings.disable3DInPure2D;
        const bool warmupGpuSkip = (!sessionGpuOff) && (s_unifiedGpuWarmupFramesRemaining > 0);
        const bool skipGpuCapture = sessionGpuOff || forceCpuPreview || warmupGpuSkip || pure2DNo3D;
        if (warmupGpuSkip && s_unifiedGpuWarmupFramesRemaining > 0) {
            --s_unifiedGpuWarmupFramesRemaining;
        }

        std::vector<std::byte> capture;
        int capW = 0;
        int capH = 0;
        bool capOk = false;
        bool usedGpuCapture = false;

        if (skipGpuCapture) {
            try {
                capture.resize(static_cast<size_t>(scW) * static_cast<size_t>(scH) * 4u);
                Solstice::Parallax::RasterizeMGDisplayList(rasterMgList, &resolver, static_cast<uint32_t>(scW),
                    static_cast<uint32_t>(scH), std::span<std::byte>(capture.data(), capture.size()));
                capW = scW;
                capH = scH;
                capOk = !capture.empty();
            } catch (const std::exception& ex) {
                WriteSink(settings.enginePreviewErrorSink, settings.enginePreviewErrorSinkBytes,
                    (std::string("MG-only preview (exception): ") + ex.what()).c_str());
            } catch (...) {
                WriteSink(settings.enginePreviewErrorSink, settings.enginePreviewErrorSinkBytes,
                    "MG-only preview failed (non-C++ exception).");
            }
        } else {
            try {
#if defined(_WIN32)
                unsigned long sehCode = 0;
                capOk = CaptureOrbitRgbSafe(nav, 0.f, 0.f, 0.f, 55.f, aspect, scW, scH, entities.data(), entities.size(), lights.data(),
                    lights.size(), capture, capW, capH, sehCode);
                if (sehCode != 0) {
                    ++s_unifiedCaptureFailStreak;
                    char err[256]{};
                    std::snprintf(err, sizeof(err), "3D preview disabled after native exception 0x%08lX in CaptureOrbitRgb.",
                        sehCode);
                    WriteSink(settings.enginePreviewErrorSink, settings.enginePreviewErrorSinkBytes, err);
                    if (settings.enginePreviewSessionDisabled) {
                        *settings.enginePreviewSessionDisabled = true;
                        s_unifiedCaptureFailStreak = 0;
                    }
                    capOk = false;
                }
#else
                capOk = Solstice::EditorEnginePreview::CaptureOrbitRgb(nav, 0.f, 0.f, 0.f, 55.f, aspect, scW, scH, entities.data(),
                    entities.size(), lights.data(), lights.size(), capture, capW, capH);
#endif
                usedGpuCapture = true;
                if (capOk) {
                    s_unifiedCaptureFailStreak = 0;
                } else {
                    ++s_unifiedCaptureFailStreak;
                    if (s_unifiedCaptureFailStreak >= kUnifiedFailsBeforeSessionDisable && settings.enginePreviewSessionDisabled) {
                        *settings.enginePreviewSessionDisabled = true;
                        s_unifiedCaptureFailStreak = 0;
                        WriteSink(settings.enginePreviewErrorSink, settings.enginePreviewErrorSinkBytes,
                            "3D preview disabled after repeated GPU capture failures — using MG/CPU viewport. Restart app to retry "
                            "CaptureOrbitRgb.");
                    } else if (settings.enginePreviewErrorSink && settings.enginePreviewErrorSinkBytes > 0
                        && settings.enginePreviewErrorSink[0] == '\0') {
                        WriteSink(settings.enginePreviewErrorSink, settings.enginePreviewErrorSinkBytes,
                            "3D preview: GPU capture failed or returned empty (reduce panel size if this persists).");
                    }
                }
            } catch (const std::exception& ex) {
                ++s_unifiedCaptureFailStreak;
                WriteSink(settings.enginePreviewErrorSink, settings.enginePreviewErrorSinkBytes,
                    (std::string("3D preview (exception): ") + ex.what()).c_str());
                if (s_unifiedCaptureFailStreak >= kUnifiedFailsBeforeSessionDisable && settings.enginePreviewSessionDisabled) {
                    *settings.enginePreviewSessionDisabled = true;
                    s_unifiedCaptureFailStreak = 0;
                    WriteSink(settings.enginePreviewErrorSink, settings.enginePreviewErrorSinkBytes,
                        "3D preview disabled after repeated exceptions — MG/CPU only until restart.");
                }
            } catch (...) {
                ++s_unifiedCaptureFailStreak;
                WriteSink(
                    settings.enginePreviewErrorSink, settings.enginePreviewErrorSinkBytes, "3D preview failed (non-C++ exception).");
                if (s_unifiedCaptureFailStreak >= kUnifiedFailsBeforeSessionDisable && settings.enginePreviewSessionDisabled) {
                    *settings.enginePreviewSessionDisabled = true;
                    s_unifiedCaptureFailStreak = 0;
                }
            }
        }

        if (capOk) {
            // Preserve stable error strings for MG-only / warmup frames (do not erase "disabled" banners).
            if (usedGpuCapture) {
                ClearSink(settings.enginePreviewErrorSink, settings.enginePreviewErrorSinkBytes);
            }
            if (usedGpuCapture && mgOverlayAlpha > 1e-3f) {
                try {
                    std::vector<std::byte> mgRgba(static_cast<size_t>(capW) * static_cast<size_t>(capH) * 4u);
                    Solstice::Parallax::RasterizeMGDisplayList(rasterMgList, &resolver, static_cast<uint32_t>(capW),
                        static_cast<uint32_t>(capH), std::span<std::byte>(mgRgba.data(), mgRgba.size()));
                    BlendMgOverScene(capture, capW, capH, mgRgba, mgOverlayAlpha);
                } catch (const std::exception& ex) {
                    WriteSink(settings.enginePreviewErrorSink, settings.enginePreviewErrorSinkBytes,
                        (std::string("MG composite (exception): ") + ex.what()).c_str());
                } catch (...) {
                    WriteSink(settings.enginePreviewErrorSink, settings.enginePreviewErrorSinkBytes,
                        "MG composite failed (non-C++ exception).");
                }
            }
            previewTexture.SetSizeUpload(window, static_cast<uint32_t>(capW), static_cast<uint32_t>(capH), capture.data(),
                capture.size());
        }

        if (previewTexture.Valid()) {
            LibUI::Viewport::DrawTextureLetterboxed(vp.draw_list, previewTexture.ImGuiTexId(), vp.min, vp.max,
                static_cast<float>(previewTexture.width), static_cast<float>(previewTexture.height));
        } else {
            LibUI::Viewport::DrawCheckerboard(
                vp.draw_list, vp.min, vp.max, 14.f, IM_COL32(32, 32, 42, 255), IM_COL32(24, 24, 30, 255));
        }

        ImVec2 projMin = vp.min;
        ImVec2 projMax = vp.max;
        if (previewTexture.Valid() && previewTexture.width > 0 && previewTexture.height > 0) {
            LibUI::Viewport::ComputeLetterbox(
                vp.min, vp.max, static_cast<float>(previewTexture.width), static_cast<float>(previewTexture.height), projMin, projMax);
        }

        if (settings.showFramingGuides && previewTexture.Valid()) {
            DrawCinematicFramingOverlays(vp.draw_list, projMin, projMax);
        }

        LibUI::Viewport::Mat4Col viewM{};
        LibUI::Viewport::Mat4Col projM{};
        LibUI::Viewport::ComputeOrbitViewProjectionColMajor(
            nav, 0.f, 0.f, 0.f, 55.f, aspect, 0.12f, 2048.f, viewM, projM);
        LibUI::Viewport::DrawXZGrid(vp.draw_list, projMin, projMax, viewM, projM, 1.f, IM_COL32(72, 72, 92, 200), 24);

        const float hePick = Solstice::EditorEnginePreview::kSchematicPreviewHalfExtent;
        for (const auto& et : eval.ElementTransforms) {
            std::string_view schemaType = Solstice::Parallax::GetElementSchema(scene, et.Element);
            ImU32 col = IM_COL32(140, 200, 255, 255);
            if (schemaType == "CameraElement") {
                col = IM_COL32(255, 210, 90, 255);
            } else if (schemaType == "ActorElement") {
                col = IM_COL32(120, 220, 255, 255);
            }
            LibUI::Viewport::DrawWorldCrossXZ(
                vp.draw_list, projMin, projMax, viewM, projM, et.Position.x, et.Position.y, et.Position.z, 0.3f, col);
            if (SmmIsElementViewportSelected(settings, static_cast<int>(et.Element))) {
                LibUI::Viewport::DrawWorldAxisAlignedBoxSelectionOutlineUniformImGui(vp.draw_list, projMin, projMax, viewM, projM,
                    et.Position.x, et.Position.y, et.Position.z, hePick, IM_COL32(255, 230, 100, 255), 2.0f, 2.8f,
                    IM_COL32(16, 14, 8, 240), 4.0f);
            }
        }
        if (unifiedWorldMgMode) {
            for (const auto& mgw : eval.MGWorldSprites) {
                LibUI::Viewport::DrawWorldCrossXZ(
                    vp.draw_list, projMin, projMax, viewM, projM, mgw.Position.x, mgw.Position.y, mgw.Position.z, 0.24f,
                    IM_COL32(255, 155, 95, 255));
                if (selectedMg == mgw.MGElement) {
                    const float heMg = (std::max)(0.18f, 0.45f * (std::max)({std::abs(mgw.Scale.x), std::abs(mgw.Scale.y), std::abs(mgw.Scale.z)}));
                    LibUI::Viewport::DrawWorldAxisAlignedBoxSelectionOutlineUniformImGui(vp.draw_list, projMin, projMax, viewM, projM,
                        mgw.Position.x, mgw.Position.y, mgw.Position.z, heMg, IM_COL32(255, 175, 110, 255), 2.0f, 2.8f,
                        IM_COL32(16, 14, 8, 240), 4.0f);
                }
            }
        }

        if (settings.showFluidVolumeOverlay) {
            for (const auto& fv : eval.FluidVolumes) {
                if (!fv.Enabled) {
                    continue;
                }
                const float bminX = (std::min)(fv.BoundsMin.x, fv.BoundsMax.x);
                const float bminY = (std::min)(fv.BoundsMin.y, fv.BoundsMax.y);
                const float bminZ = (std::min)(fv.BoundsMin.z, fv.BoundsMax.z);
                const float bmaxX = (std::max)(fv.BoundsMin.x, fv.BoundsMax.x);
                const float bmaxY = (std::max)(fv.BoundsMin.y, fv.BoundsMax.y);
                const float bmaxZ = (std::max)(fv.BoundsMin.z, fv.BoundsMax.z);
                const ImU32 fcol = IM_COL32(90, 200, 255, 200);
                LibUI::Viewport::DrawWorldAxisAlignedBoxWireframeImGui(vp.draw_list, projMin, projMax, viewM, projM, bminX, bminY, bminZ,
                    bmaxX, bmaxY, bmaxZ, fcol, 1.4f, 0.f);
                if (SmmIsElementViewportSelected(settings, static_cast<int>(fv.Element))) {
                    const float cx = 0.5f * (bminX + bmaxX);
                    const float cy = 0.5f * (bminY + bmaxY);
                    const float cz = 0.5f * (bminZ + bmaxZ);
                    const float ex = 0.5f * (bmaxX - bminX);
                    const float ey = 0.5f * (bmaxY - bminY);
                    const float ez = 0.5f * (bmaxZ - bminZ);
                    const float heFluid = (std::max)(ex, (std::max)(ey, ez));
                    LibUI::Viewport::DrawWorldAxisAlignedBoxSelectionOutlineUniformImGui(vp.draw_list, projMin, projMax, viewM, projM,
                        cx, cy, cz, std::max(heFluid, 0.05f), IM_COL32(255, 230, 100, 255), 2.0f, 2.8f,
                        IM_COL32(16, 14, 8, 240), 4.0f);
                }
            }
        }

        for (const auto& ls : eval.LightStates) {
            ImVec2 sp{};
            if (LibUI::Viewport::WorldToScreen(
                    viewM, projM, ls.Position.x, ls.Position.y, ls.Position.z, projMin, projMax, sp)) {
                const ImU32 lcol = IM_COL32(static_cast<int>(ls.Color.x * 255.f), static_cast<int>(ls.Color.y * 255.f),
                    static_cast<int>(ls.Color.z * 255.f), 255);
                vp.draw_list->AddCircleFilled(sp, 6.f, lcol);
                vp.draw_list->AddCircle(sp, 7.f, IM_COL32(255, 255, 255, 200));
            }
            if (SmmIsElementViewportSelected(settings, static_cast<int>(ls.Element))) {
                LibUI::Viewport::DrawWorldAxisAlignedBoxSelectionOutlineUniformImGui(vp.draw_list, projMin, projMax, viewM, projM,
                    ls.Position.x, ls.Position.y, ls.Position.z, 0.28f, IM_COL32(255, 230, 100, 255), 2.0f, 2.8f,
                    IM_COL32(16, 14, 8, 240), 4.0f);
            }
        }

        if (particles && particles->enabled) {
            if (particleSpriteTexture) {
                Smm::Editing::SyncParticleSpritePreview(window, *particleSpriteTexture, *particles);
            }
            const float dt = ImGui::GetIO().DeltaTime;
            Smm::Editing::TickParticlePreview(*particles, emitterWorld, dt);
            if (particles->ribbonTrails) {
                for (const auto& p : particles->particles) {
                    if (p.ribbonCount < 2) {
                        continue;
                    }
                    for (uint8_t ri = 0; ri < p.ribbonCount - 1; ++ri) {
                        const auto& aW = p.ribbon[ri];
                        const auto& bW = p.ribbon[ri + 1];
                        ImVec2 sa{};
                        ImVec2 sb{};
                        if (!LibUI::Viewport::WorldToScreen(
                                viewM, projM, aW.x, aW.y, aW.z, projMin, projMax, sa)
                            || !LibUI::Viewport::WorldToScreen(
                                viewM, projM, bW.x, bW.y, bW.z, projMin, projMax, sb)) {
                            continue;
                        }
                        const float tseg = (static_cast<float>(ri) + 0.5f) / static_cast<float>(p.ribbonCount);
                        const float tlife = std::clamp(p.age / (std::max)(p.lifetime, 1e-4f), 0.f, 1.f) * 0.65f
                            + 0.35f * tseg;
                        float c4[4]{};
                        Smm::Editing::SampleParticleColorOverLife(*particles, tlife, c4);
                        const int ir = static_cast<int>(std::clamp(c4[0] * 255.f, 0.f, 255.f));
                        const int ig = static_cast<int>(std::clamp(c4[1] * 255.f, 0.f, 255.f));
                        const int ib = static_cast<int>(std::clamp(c4[2] * 255.f, 0.f, 255.f));
                        const int ia = static_cast<int>(std::clamp(c4[3] * 200.f, 0.f, 255.f));
                        const ImU32 lcol = IM_COL32(ir, ig, ib, ia);
                        vp.draw_list->AddLine(sa, sb, lcol, 1.8f);
                    }
                }
            }
            const bool drawSprite = particleSpriteTexture && particles->useImportedSprite && particleSpriteTexture->Valid();
            const float tw = drawSprite ? static_cast<float>(particleSpriteTexture->width) : 1.f;
            const float th = drawSprite ? static_cast<float>(particleSpriteTexture->height) : 1.f;
            for (const auto& p : particles->particles) {
                ImVec2 sp{};
                if (LibUI::Viewport::WorldToScreen(viewM, projM, p.position.x, p.position.y, p.position.z, projMin, projMax, sp)) {
                    const float r = std::max(2.f, p.size * 120.f);
                    const float t = std::clamp(p.age / (std::max)(p.lifetime, 1e-4f), 0.f, 1.f);
                    float c4[4]{};
                    Smm::Editing::SampleParticleColorOverLife(*particles, t, c4);
                    const float cr = c4[0];
                    const float cg = c4[1];
                    const float cb = c4[2];
                    const float ca = c4[3];
                    const int ir = static_cast<int>(std::clamp(cr * 255.f, 0.f, 255.f));
                    const int ig = static_cast<int>(std::clamp(cg * 255.f, 0.f, 255.f));
                    const int ib = static_cast<int>(std::clamp(cb * 255.f, 0.f, 255.f));
                    const int ia = static_cast<int>(std::clamp(ca * 255.f, 0.f, 255.f));
                    const ImU32 tint = IM_COL32(ir, ig, ib, ia);
                    if (drawSprite && tw > 0.f && th > 0.f) {
                        const float box = 2.f * r;
                        float sx = box;
                        float sy = box;
                        const float ar = tw / th;
                        if (ar >= 1.f) {
                            sx = box;
                            sy = box / ar;
                        } else {
                            sy = box;
                            sx = box * ar;
                        }
                        const ImVec2 h(sx * 0.5f, sy * 0.5f);
                        vp.draw_list->AddImage(particleSpriteTexture->ImGuiTexId(), ImVec2(sp.x - h.x, sp.y - h.y),
                            ImVec2(sp.x + h.x, sp.y + h.y), ImVec2(0.f, 0.f), ImVec2(1.f, 1.f), tint);
                    } else {
                        vp.draw_list->AddCircleFilled(sp, r, tint);
                        vp.draw_list->AddCircle(sp, r + 1.f, IM_COL32(20, 20, 30, 160));
                    }
                }
            }
        }

        {
            const Solstice::Parallax::ElementIndex smmPeiOut =
                Solstice::Parallax::FindElement(scene, "SMM_ParticleEmitter");
            if (particles && particles->enabled && smmPeiOut != Solstice::Parallax::PARALLAX_INVALID_INDEX
                && SmmIsElementViewportSelected(settings, static_cast<int>(smmPeiOut))) {
                LibUI::Viewport::DrawWorldAxisAlignedBoxSelectionOutlineUniformImGui(vp.draw_list, projMin, projMax, viewM, projM,
                    emitterWorld.x, emitterWorld.y, emitterWorld.z, 0.42f, IM_COL32(255, 230, 100, 255), 2.0f, 2.8f,
                    IM_COL32(16, 14, 8, 240), 4.0f);
            }
        }
        if (SmmPrimarySel(settings) >= 0) {
            const int gizmoEl = SmmPrimarySel(settings);
            bool drewGizmo = false;
            for (const auto& et : eval.ElementTransforms) {
                if (static_cast<int>(et.Element) == gizmoEl) {
                    LibUI::Viewport::DrawWorldTransformGizmo(vp.draw_list, projMin, projMax, viewM, projM, et.Position.x, et.Position.y,
                        et.Position.z, s_viewTool, s_viewAxis, 0.75f, 19.0f);
                    drewGizmo = true;
                    break;
                }
            }
            if (!drewGizmo) {
                for (const auto& ls : eval.LightStates) {
                    if (static_cast<int>(ls.Element) == gizmoEl) {
                        LibUI::Viewport::DrawWorldTransformGizmo(vp.draw_list, projMin, projMax, viewM, projM, ls.Position.x, ls.Position.y,
                            ls.Position.z, s_viewTool, s_viewAxis, 0.6f, 18.0f);
                        break;
                    }
                }
            }
        }

        if (settings.camera && vp.hovered && LibUI::Widgets::IsKeyPressed(ImGuiKey_F, false) && SmmPrimarySel(settings) >= 0) {
            const int focusEl = SmmPrimarySel(settings);
            for (const auto& et : eval.ElementTransforms) {
                if (static_cast<int>(et.Element) == focusEl) {
                    LibUI::Viewport::FocusOrbitOnTarget(
                        *settings.camera, et.Position.x, et.Position.y, et.Position.z, 0.f, 0.f, 0.f);
                    break;
                }
            }
            for (const auto& ls : eval.LightStates) {
                if (static_cast<int>(ls.Element) == focusEl) {
                    LibUI::Viewport::FocusOrbitOnTarget(
                        *settings.camera, ls.Position.x, ls.Position.y, ls.Position.z, 0.f, 0.f, 0.f);
                    break;
                }
            }
        }

        ImGuiIO& ioVp = ImGui::GetIO();
        static LibUI::Viewport::OrbitLeftDragSuppression s_smmOrbitSuppress{};
        static ImVec2 s_smmDragPlanePrev{};
        static int s_smmDragElement{-1};
        static int s_smmDragWorldMg{-1};
        static bool s_smmDragUndoPushed{false};
        static bool s_smmMarqueeActive{false};
        static bool s_smmMarqueeFromChord{false};
        static ImVec2 s_smmMarqueeA{};
        static ImVec2 s_smmMarqueeB{};
        enum class MgDragMode : uint8_t {
            None = 0,
            Move,
            ResizeCorner,
            Rotate,
        };
        static MgDragMode s_mgDragMode = MgDragMode::None;
        static int s_mgDragCorner = -1;
        static Solstice::Parallax::MGIndex s_mgDragIndex = Solstice::Parallax::PARALLAX_INVALID_INDEX;
        static Solstice::Math::Vec2 s_mgStartPos{0.f, 0.f};
        static Solstice::Math::Vec2 s_mgStartSize{64.f, 64.f};
        static float s_mgStartRot = 0.f;
        static float s_mgStartMouseAngle = 0.f;
        static ImVec2 s_mgPrevMouseComp{};
        static bool s_mgUndoPushed = false;

        const auto compToScreen = [&](const ImVec2& c) {
            if (!previewTexture.Valid() || previewTexture.width <= 0 || previewTexture.height <= 0) {
                return c;
            }
            const float sx = (c.x / static_cast<float>(previewTexture.width));
            const float sy = (c.y / static_cast<float>(previewTexture.height));
            return ImVec2(projMin.x + sx * (projMax.x - projMin.x), projMin.y + sy * (projMax.y - projMin.y));
        };
        const auto screenToComp = [&](const ImVec2& s, ImVec2& out) {
            float u = 0.f;
            float v = 0.f;
            if (!previewTexture.Valid() || previewTexture.width <= 0 || previewTexture.height <= 0
                || !LibUI::Viewport::ScreenToLetterboxUv(
                    s, vp.min, vp.max, static_cast<float>(previewTexture.width), static_cast<float>(previewTexture.height), u, v)) {
                return false;
            }
            out.x = u * static_cast<float>(previewTexture.width);
            out.y = v * static_cast<float>(previewTexture.height);
            return true;
        };

        bool hasSelectedMgSprite = false;
        Solstice::Math::Vec2 mgSpritePos{};
        Solstice::Math::Vec2 mgSpriteSize{};
        float mgSpriteRot = 0.f;
        ImVec2 mgCornersComp[4]{};
        ImVec2 mgCornersScreen[4]{};
        const float mgRootX = 16.f + mgList.Post.ScreenShakeX;
        const float mgRootY = 16.f + mgList.Post.ScreenShakeY;
        if (selectedMg != Solstice::Parallax::PARALLAX_INVALID_INDEX && selectedMg < scene.GetMGElements().size()) {
            const auto& mgRec = scene.GetMGElements()[selectedMg];
            std::string_view mgSchema{};
            if (mgRec.SchemaIndex < scene.GetSchemas().size()) {
                mgSchema = scene.GetSchemas()[mgRec.SchemaIndex].TypeName;
            }
            if (mgSchema == "MGSpriteElement" && (!unifiedWorldMgMode || !selectedMgIsWorld)) {
                const Solstice::Math::Vec2 parentOffset = SmmMgAccumulatedParentOffset(scene, selectedMg);
                const Solstice::Math::Vec2 localPos = ReadMgVec2(mgRec, "Position", Solstice::Math::Vec2{16.f, 16.f});
                mgSpritePos = Solstice::Math::Vec2{mgRootX + parentOffset.x + localPos.x, mgRootY + parentOffset.y + localPos.y};
                mgSpriteSize = ReadMgVec2(mgRec, "Size", Solstice::Math::Vec2{256.f, 256.f});
                mgSpriteRot = ReadMgFloat(mgRec, "RotationZ", 0.f);
                SmmRotateSpriteCornersComp(mgSpritePos.x, mgSpritePos.y, mgSpriteSize.x, mgSpriteSize.y, mgSpriteRot, mgCornersComp);
                for (int i = 0; i < 4; ++i) {
                    mgCornersScreen[i] = compToScreen(mgCornersComp[i]);
                }
                hasSelectedMgSprite = true;
            }
        }

        if (hasSelectedMgSprite) {
            vp.draw_list->AddPolyline(mgCornersScreen, 4, IM_COL32(255, 205, 90, 235), ImDrawFlags_Closed, 1.8f);
            for (int i = 0; i < 4; ++i) {
                const ImU32 hc = (i == s_mgDragCorner && s_mgDragMode == MgDragMode::ResizeCorner) ? IM_COL32(255, 230, 140, 255)
                                                                                                     : IM_COL32(255, 190, 80, 220);
                vp.draw_list->AddCircleFilled(mgCornersScreen[i], 4.5f, hc);
                vp.draw_list->AddCircle(mgCornersScreen[i], 4.5f, IM_COL32(35, 25, 8, 220), 0.f, 1.0f);
            }
            const ImVec2 centerComp(
                mgSpritePos.x + 0.5f * mgSpriteSize.x, mgSpritePos.y + 0.5f * mgSpriteSize.y);
            const ImVec2 centerScreen = compToScreen(centerComp);
            LibUI::Viewport::DrawScreenTransformGizmo(vp.draw_list, centerScreen, s_viewTool, s_viewAxis, 18.0f, 14.0f);
        }

        std::vector<LibUI::Tools::AxisAlignedBox3> smmPickBoxes;
        std::vector<LibUI::Viewport::PickToken> smmPickTokens;
        const Solstice::Parallax::ElementIndex smmParticleEi =
            Solstice::Parallax::FindElement(scene, "SMM_ParticleEmitter");
        const bool smmWantParticlePick = particles && particles->enabled && smmParticleEi != Solstice::Parallax::PARALLAX_INVALID_INDEX;
        SmmBuildUnifiedPickLists(
            eval, smmParticleEi, emitterWorld, smmWantParticlePick, unifiedWorldMgMode, smmPickBoxes, smmPickTokens);

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            s_smmDragElement = -1;
            s_smmDragWorldMg = -1;
            s_smmDragUndoPushed = false;
            s_mgDragMode = MgDragMode::None;
            s_mgDragCorner = -1;
            s_mgDragIndex = Solstice::Parallax::PARALLAX_INVALID_INDEX;
            s_mgUndoPushed = false;
            s_smmOrbitSuppress.ClearStroke();
            if (s_smmMarqueeActive && s_smmMarqueeFromChord && smmPickBoxes.size() == smmPickTokens.size()) {
                const float rx0 = (std::min)(s_smmMarqueeA.x, s_smmMarqueeB.x);
                const float ry0 = (std::min)(s_smmMarqueeA.y, s_smmMarqueeB.y);
                const float rx1 = (std::max)(s_smmMarqueeA.x, s_smmMarqueeB.x);
                const float ry1 = (std::max)(s_smmMarqueeA.y, s_smmMarqueeB.y);
                const ImVec2 rmin(rx0, ry0);
                const ImVec2 rmax(rx1, ry1);
                if (settings.viewportSelectedElements && settings.primaryElementIndex) {
                    if (!ioVp.KeyShift) {
                        settings.viewportSelectedElements->clear();
                    }
                    for (size_t bi = 0; bi < smmPickBoxes.size() && bi < smmPickTokens.size(); ++bi) {
                        const LibUI::Tools::AxisAlignedBox3& bx = smmPickBoxes[bi];
                        if (LibUI::Viewport::ScreenMarqueeIntersectsWorldAabb(projMin, projMax, viewM, projM, bx.minX, bx.minY,
                                bx.minZ, bx.maxX, bx.maxY, bx.maxZ, rmin, rmax)) {
                            const int el = static_cast<int>(LibUI::Viewport::PickTokenItem(smmPickTokens[bi]));
                            settings.viewportSelectedElements->insert(el);
                        }
                    }
                    if (!settings.viewportSelectedElements->empty()) {
                        *settings.primaryElementIndex = *settings.viewportSelectedElements->begin();
                    }
                }
            }
            s_smmMarqueeActive = false;
            s_smmMarqueeFromChord = false;
        }

        if (vp.hovered && ioVp.KeyCtrl && ioVp.KeyAlt && LibUI::Widgets::IsMouseClicked(ImGuiMouseButton_Left)) {
            s_smmMarqueeActive = true;
            s_smmMarqueeFromChord = true;
            s_smmMarqueeA = LibUI::Widgets::GetMousePos();
            s_smmMarqueeB = s_smmMarqueeA;
        }
        if (s_smmMarqueeActive && LibUI::Widgets::IsMouseDown(ImGuiMouseButton_Left) && s_smmMarqueeFromChord) {
            s_smmMarqueeB = LibUI::Widgets::GetMousePos();
            LibUI::Viewport::DrawMarqueeRect(vp.draw_list, s_smmMarqueeA, s_smmMarqueeB, IM_COL32(120, 180, 255, 55),
                IM_COL32(220, 240, 255, 200), 1.25f);
        }

        bool rayPickHit = false;
        LibUI::Viewport::PickToken hitTok = LibUI::Viewport::kPickTokenNone;
        if (vp.hovered && LibUI::Widgets::IsMouseClicked(ImGuiMouseButton_Left) && !(ioVp.KeyCtrl && ioVp.KeyAlt)) {
            bool mgPickHit = false;
            if (hasSelectedMgSprite && settings.sceneDirty != nullptr) {
                const ImVec2 mouseScreen = LibUI::Widgets::GetMousePos();
                const int hitCorner = HitCornerHandle(mgCornersScreen, mouseScreen, 8.0f);
                const bool insideSprite =
                    PointInQuad(mouseScreen, mgCornersScreen[0], mgCornersScreen[1], mgCornersScreen[2], mgCornersScreen[3]);
                ImVec2 mouseComp{};
                const bool haveCompMouse = screenToComp(mouseScreen, mouseComp);
                if ((hitCorner >= 0 || insideSprite) && haveCompMouse) {
                    mgPickHit = true;
                    s_mgDragIndex = selectedMg;
                    s_mgStartPos = mgSpritePos;
                    s_mgStartSize = mgSpriteSize;
                    s_mgStartRot = mgSpriteRot;
                    s_mgPrevMouseComp = mouseComp;
                    s_mgUndoPushed = false;
                    if (s_viewTool == LibUI::Viewport::TransformTool::Rotate) {
                        s_mgDragMode = MgDragMode::Rotate;
                        const ImVec2 c(mgSpritePos.x + 0.5f * mgSpriteSize.x, mgSpritePos.y + 0.5f * mgSpriteSize.y);
                        s_mgStartMouseAngle = std::atan2(mouseComp.y - c.y, mouseComp.x - c.x);
                    } else if (s_viewTool == LibUI::Viewport::TransformTool::Scale && hitCorner >= 0) {
                        s_mgDragMode = MgDragMode::ResizeCorner;
                        s_mgDragCorner = hitCorner;
                    } else if (insideSprite) {
                        s_mgDragMode = MgDragMode::Move;
                    } else {
                        s_mgDragMode = MgDragMode::None;
                    }
                }
            }

            float rox = 0.f, roy = 0.f, roz = 0.f, rdx = 0.f, rdy = 0.f, rdz = 0.f;
            float tRay = 0.f;
            if (!mgPickHit
                && LibUI::Viewport::ScreenToWorldRay(
                    viewM, projM, projMin, projMax, LibUI::Widgets::GetMousePos(), rox, roy, roz, rdx, rdy, rdz)) {
                if (!smmPickBoxes.empty()
                    && LibUI::Viewport::PickClosestMergedAlongRay(rox, roy, roz, rdx, rdy, rdz, smmPickBoxes.data(),
                        smmPickTokens.data(), static_cast<int>(smmPickBoxes.size()), hitTok, tRay)) {
                    rayPickHit = (hitTok != LibUI::Viewport::kPickTokenNone);
                    (void)tRay;
                }
            }
            s_smmOrbitSuppress.OnLeftPress(true, rayPickHit || mgPickHit);
            if (rayPickHit && settings.primaryElementIndex) {
                const std::uint16_t pickLayer = LibUI::Viewport::PickTokenLayer(hitTok);
                const int elHit = static_cast<int>(LibUI::Viewport::PickTokenItem(hitTok));
                s_smmDragPlanePrev = LibUI::Widgets::GetMousePos();
                if (pickLayer == kPickLayerMgWorldSprite) {
                    s_smmDragWorldMg = elHit;
                    s_smmDragElement = -1;
                    if (settings.selectedMgElementIndex) {
                        *settings.selectedMgElementIndex = elHit;
                    }
                } else {
                    s_smmDragElement = elHit;
                    s_smmDragWorldMg = -1;
                    if (settings.viewportSelectedElements) {
                        if (ioVp.KeyCtrl) {
                            if (settings.viewportSelectedElements->count(elHit)) {
                                settings.viewportSelectedElements->erase(elHit);
                            } else {
                                settings.viewportSelectedElements->insert(elHit);
                            }
                        } else {
                            settings.viewportSelectedElements->clear();
                            settings.viewportSelectedElements->insert(elHit);
                        }
                    }
                    *settings.primaryElementIndex = elHit;
                    if (settings.onViewportPickElement) {
                        settings.onViewportPickElement(elHit);
                    }
                }
            } else {
                s_smmDragElement = -1;
                s_smmDragWorldMg = -1;
            }
        }

        if (s_smmOrbitSuppress.ShouldSuppressOrbitLeftDrag() && s_mgDragMode != MgDragMode::None
            && s_mgDragIndex != Solstice::Parallax::PARALLAX_INVALID_INDEX && LibUI::Widgets::IsMouseDown(ImGuiMouseButton_Left)
            && settings.sceneDirty != nullptr && s_mgDragIndex < scene.GetMGElements().size()) {
            ImVec2 curComp{};
            if (screenToComp(LibUI::Widgets::GetMousePos(), curComp)) {
                auto& mgMut = scene.GetMGElements()[s_mgDragIndex];
                const Solstice::Math::Vec2 parentOffset = SmmMgAccumulatedParentOffset(scene, s_mgDragIndex);
                auto ensureUndo = [&]() {
                    if (!s_mgUndoPushed) {
                        Smm::PushSceneUndoSnapshot(scene, settings.compressPrlxForUndo);
                        s_mgUndoPushed = true;
                    }
                };
                if (s_mgDragMode == MgDragMode::Move) {
                    ImVec2 d(curComp.x - s_mgPrevMouseComp.x, curComp.y - s_mgPrevMouseComp.y);
                    if (s_viewAxis == LibUI::Viewport::TransformAxis::X) {
                        d.y = 0.f;
                    } else if (s_viewAxis == LibUI::Viewport::TransformAxis::Y) {
                        d.x = 0.f;
                    }
                    if (std::abs(d.x) > 1e-4f || std::abs(d.y) > 1e-4f) {
                        ensureUndo();
                        const Solstice::Math::Vec2 oldLocal = ReadMgVec2(mgMut, "Position", Solstice::Math::Vec2{16.f, 16.f});
                        mgMut.Attributes["Position"] = Solstice::Parallax::AttributeValue{
                            Solstice::Math::Vec2{oldLocal.x + d.x, oldLocal.y + d.y}};
                        *settings.sceneDirty = true;
                        s_mgPrevMouseComp = curComp;
                    }
                } else if (s_mgDragMode == MgDragMode::ResizeCorner) {
                    const ImVec2 d(curComp.x - s_mgPrevMouseComp.x, curComp.y - s_mgPrevMouseComp.y);
                    if (std::abs(d.x) > 1e-4f || std::abs(d.y) > 1e-4f) {
                        const float co = std::cos(s_mgStartRot);
                        const float si = std::sin(s_mgStartRot);
                        const float localDx = d.x * co + d.y * si;
                        const float localDy = -d.x * si + d.y * co;
                        const int sx = (s_mgDragCorner == 1 || s_mgDragCorner == 2) ? 1 : -1;
                        const int sy = (s_mgDragCorner == 2 || s_mgDragCorner == 3) ? 1 : -1;
                        Solstice::Math::Vec2 localSize = ReadMgVec2(mgMut, "Size", Solstice::Math::Vec2{256.f, 256.f});
                        if (s_viewAxis != LibUI::Viewport::TransformAxis::Y) {
                            localSize.x += static_cast<float>(sx) * localDx;
                        }
                        if (s_viewAxis != LibUI::Viewport::TransformAxis::X) {
                            localSize.y += static_cast<float>(sy) * localDy;
                        }
                        if (std::abs(localSize.x) < 2.f) {
                            localSize.x = (localSize.x < 0.f) ? -2.f : 2.f;
                        }
                        if (std::abs(localSize.y) < 2.f) {
                            localSize.y = (localSize.y < 0.f) ? -2.f : 2.f;
                        }
                        if (ioVp.KeyShift) {
                            const float ax = std::abs(s_mgStartSize.x);
                            const float ay = std::abs(s_mgStartSize.y);
                            if (ax > 1e-4f && ay > 1e-4f) {
                                const float ratio = ay / ax;
                                if (std::abs(localDx) >= std::abs(localDy)) {
                                    const float sx = (localSize.x < 0.f) ? -1.f : 1.f;
                                    const float sy = (localSize.y < 0.f) ? -1.f : 1.f;
                                    localSize.y = sy * std::max(2.f, std::abs(localSize.x) * ratio);
                                    localSize.x = sx * std::max(2.f, std::abs(localSize.x));
                                } else {
                                    const float sx = (localSize.x < 0.f) ? -1.f : 1.f;
                                    const float sy = (localSize.y < 0.f) ? -1.f : 1.f;
                                    localSize.x = sx * std::max(2.f, std::abs(localSize.y) / ratio);
                                    localSize.y = sy * std::max(2.f, std::abs(localSize.y));
                                }
                            }
                        }
                        localSize.x = std::clamp(localSize.x, -8192.f, 8192.f);
                        localSize.y = std::clamp(localSize.y, -8192.f, 8192.f);
                        ensureUndo();
                        mgMut.Attributes["Size"] = Solstice::Parallax::AttributeValue{localSize};
                        *settings.sceneDirty = true;
                        s_mgPrevMouseComp = curComp;
                    }
                } else if (s_mgDragMode == MgDragMode::Rotate) {
                    const ImVec2 c(
                        s_mgStartPos.x + 0.5f * s_mgStartSize.x, s_mgStartPos.y + 0.5f * s_mgStartSize.y);
                    const float a = std::atan2(curComp.y - c.y, curComp.x - c.x);
                    float newRot = s_mgStartRot + (a - s_mgStartMouseAngle);
                    if (ioVp.KeyShift) {
                        constexpr float kStep = 3.14159265358979323846f / 12.0f; // 15 degrees
                        newRot = std::round(newRot / kStep) * kStep;
                    }
                    const float oldRot = ReadMgFloat(mgMut, "RotationZ", 0.f);
                    if (std::abs(newRot - oldRot) > 1e-4f) {
                        ensureUndo();
                        mgMut.Attributes["RotationZ"] = Solstice::Parallax::AttributeValue{newRot};
                        *settings.sceneDirty = true;
                    }
                }
                (void)parentOffset;
            }
        }

        if (unifiedWorldMgMode && s_smmOrbitSuppress.ShouldSuppressOrbitLeftDrag() && s_smmDragWorldMg >= 0
            && LibUI::Widgets::IsMouseDown(ImGuiMouseButton_Left) && settings.sceneDirty != nullptr
            && static_cast<size_t>(s_smmDragWorldMg) < scene.GetMGElements().size()) {
            bool& dirty = *settings.sceneDirty;
            auto& mgMut = scene.GetMGElements()[static_cast<size_t>(s_smmDragWorldMg)];
            const ImVec2 curM = LibUI::Widgets::GetMousePos();
            if (s_viewTool == LibUI::Viewport::TransformTool::Rotate) {
                const float dYaw = LibUI::Viewport::ComputeTransformRotateDeltaDeg(curM.x - s_smmDragPlanePrev.x, ioVp.KeyShift);
                if (std::abs(dYaw) > 1e-4f) {
                    if (!s_smmDragUndoPushed) {
                        Smm::PushSceneUndoSnapshot(scene, settings.compressPrlxForUndo);
                        s_smmDragUndoPushed = true;
                    }
                    s_smmDragPlanePrev = curM;
                    float yaw = ReadMgFloat(mgMut, "WorldYawDeg", 0.f);
                    float pitch = ReadMgFloat(mgMut, "WorldPitchDeg", 0.f);
                    float roll = ReadMgFloat(mgMut, "WorldRollDeg", 0.f);
                    if (s_viewAxis == LibUI::Viewport::TransformAxis::X) {
                        pitch += dYaw;
                    } else if (s_viewAxis == LibUI::Viewport::TransformAxis::Z) {
                        roll += dYaw;
                    } else {
                        yaw += dYaw;
                    }
                    mgMut.Attributes["WorldYawDeg"] = Solstice::Parallax::AttributeValue{yaw};
                    mgMut.Attributes["WorldPitchDeg"] = Solstice::Parallax::AttributeValue{pitch};
                    mgMut.Attributes["WorldRollDeg"] = Solstice::Parallax::AttributeValue{roll};
                    dirty = true;
                }
            } else if (s_viewTool == LibUI::Viewport::TransformTool::Scale) {
                const float dScale = LibUI::Viewport::ComputeTransformScaleDelta(curM.x - s_smmDragPlanePrev.x, ioVp.KeyShift);
                if (std::abs(dScale) > 1e-4f) {
                    if (!s_smmDragUndoPushed) {
                        Smm::PushSceneUndoSnapshot(scene, settings.compressPrlxForUndo);
                        s_smmDragUndoPushed = true;
                    }
                    s_smmDragPlanePrev = curM;
                    Solstice::Math::Vec3 sc = ReadMgVec3(mgMut, "WorldScale", Solstice::Math::Vec3{1.f, 1.f, 1.f});
                    if (s_viewAxis == LibUI::Viewport::TransformAxis::X) {
                        sc.x = std::clamp(sc.x + dScale, 0.05f, 256.f);
                    } else if (s_viewAxis == LibUI::Viewport::TransformAxis::Y) {
                        sc.y = std::clamp(sc.y + dScale, 0.05f, 256.f);
                    } else if (s_viewAxis == LibUI::Viewport::TransformAxis::Z) {
                        sc.z = std::clamp(sc.z + dScale, 0.05f, 256.f);
                    } else {
                        const float s = std::clamp(sc.x + dScale, 0.05f, 256.f);
                        sc = Solstice::Math::Vec3{s, s, s};
                    }
                    mgMut.Attributes["WorldScale"] = Solstice::Parallax::AttributeValue{sc};
                    dirty = true;
                }
            } else {
                float dwx = 0.f, dwz = 0.f, dwy = 0.f;
                const ImVec2 prevM = s_smmDragPlanePrev;
                if (LibUI::Viewport::WorldDeltaOnHorizontalPlane(viewM, projM, projMin, projMax, s_smmDragPlanePrev, curM, 0.f, dwx, dwz)) {
                    if (!s_smmDragUndoPushed) {
                        Smm::PushSceneUndoSnapshot(scene, settings.compressPrlxForUndo);
                        s_smmDragUndoPushed = true;
                    }
                    if (s_viewAxis == LibUI::Viewport::TransformAxis::X) {
                        dwz = 0.f;
                    } else if (s_viewAxis == LibUI::Viewport::TransformAxis::Z) {
                        dwx = 0.f;
                    } else if (s_viewAxis == LibUI::Viewport::TransformAxis::Y) {
                        dwx = 0.f;
                        dwz = 0.f;
                        dwy = -(curM.y - prevM.y) * 0.01f;
                        if (ioVp.KeyShift) {
                            dwy = std::round(dwy / 0.1f) * 0.1f;
                        }
                    }
                    Solstice::Math::Vec3 p = ReadMgVec3(mgMut, "WorldPosition", Solstice::Math::Vec3{0.f, 1.25f, 0.f});
                    p.x += dwx;
                    p.y += dwy;
                    p.z += dwz;
                    mgMut.Attributes["WorldPosition"] = Solstice::Parallax::AttributeValue{p};
                    dirty = true;
                    s_smmDragPlanePrev = curM;
                }
            }
        }

        if (s_smmOrbitSuppress.ShouldSuppressOrbitLeftDrag() && s_smmDragElement >= 0 && LibUI::Widgets::IsMouseDown(ImGuiMouseButton_Left)
            && settings.sceneDirty != nullptr) {
            bool& dirty = *settings.sceneDirty;
            const ImVec2 curM = LibUI::Widgets::GetMousePos();
            const Solstice::Parallax::ElementIndex dei = static_cast<Solstice::Parallax::ElementIndex>(s_smmDragElement);
            if (s_viewTool == LibUI::Viewport::TransformTool::Rotate) {
                const float dYaw = LibUI::Viewport::ComputeTransformRotateDeltaDeg(
                    curM.x - s_smmDragPlanePrev.x, ioVp.KeyShift);
                if (std::abs(dYaw) > 1e-4f) {
                    if (!s_smmDragUndoPushed) {
                        Smm::PushSceneUndoSnapshot(scene, settings.compressPrlxForUndo);
                        s_smmDragUndoPushed = true;
                    }
                    s_smmDragPlanePrev = curM;
                    auto applyRotate = [&](int el) {
                        if (smmParticleEi != Solstice::Parallax::PARALLAX_INVALID_INDEX
                            && el == static_cast<int>(smmParticleEi)) {
                            return;
                        }
                        const Solstice::Parallax::ElementIndex ei = static_cast<Solstice::Parallax::ElementIndex>(el);
                        float p = 0.f, y = 0.f, r = 0.f;
                        SmmReadElementEulerDegrees(scene, ei, p, y, r);
                        if (s_viewAxis == LibUI::Viewport::TransformAxis::X) {
                            p += dYaw;
                        } else if (s_viewAxis == LibUI::Viewport::TransformAxis::Z) {
                            r += dYaw;
                        } else {
                            y += dYaw;
                        }
                        SmmSetElementEulerDegrees(scene, ei, p, y, r);
                        dirty = true;
                    };
                    if (settings.viewportSelectedElements && settings.viewportSelectedElements->size() > 1) {
                        for (int el : *settings.viewportSelectedElements) {
                            applyRotate(el);
                        }
                    } else {
                        applyRotate(static_cast<int>(dei));
                    }
                }
            } else if (s_viewTool == LibUI::Viewport::TransformTool::Scale) {
                const float dScale = LibUI::Viewport::ComputeTransformScaleDelta(
                    curM.x - s_smmDragPlanePrev.x, ioVp.KeyShift);
                if (std::abs(dScale) > 1e-4f) {
                    if (!s_smmDragUndoPushed) {
                        Smm::PushSceneUndoSnapshot(scene, settings.compressPrlxForUndo);
                        s_smmDragUndoPushed = true;
                    }
                    s_smmDragPlanePrev = curM;
                    auto applyScale = [&](int el) {
                        if (smmParticleEi != Solstice::Parallax::PARALLAX_INVALID_INDEX
                            && el == static_cast<int>(smmParticleEi)) {
                            return;
                        }
                        const Solstice::Parallax::ElementIndex ei = static_cast<Solstice::Parallax::ElementIndex>(el);
                        const Solstice::Math::Vec3 oldScale =
                            ReadElementVec3(scene, ei, "Scale", Solstice::Math::Vec3{1.f, 1.f, 1.f});
                        Solstice::Math::Vec3 ns = oldScale;
                        if (s_viewAxis == LibUI::Viewport::TransformAxis::X) {
                            ns.x = std::clamp(oldScale.x + dScale, 0.05f, 128.0f);
                        } else if (s_viewAxis == LibUI::Viewport::TransformAxis::Y) {
                            ns.y = std::clamp(oldScale.y + dScale, 0.05f, 128.0f);
                        } else if (s_viewAxis == LibUI::Viewport::TransformAxis::Z) {
                            ns.z = std::clamp(oldScale.z + dScale, 0.05f, 128.0f);
                        } else {
                            const float s = std::clamp(oldScale.x + dScale, 0.05f, 128.0f);
                            ns = Solstice::Math::Vec3{s, s, s};
                        }
                        Solstice::Parallax::SetAttribute(scene, ei, "Scale", Solstice::Parallax::AttributeValue{ns});
                        dirty = true;
                    };
                    if (settings.viewportSelectedElements && settings.viewportSelectedElements->size() > 1) {
                        for (int el : *settings.viewportSelectedElements) {
                            applyScale(el);
                        }
                    } else {
                        applyScale(static_cast<int>(dei));
                    }
                }
            } else {
                float dwx = 0.f, dwz = 0.f;
                float dwy = 0.f;
                const ImVec2 prevM = s_smmDragPlanePrev;
                if (LibUI::Viewport::WorldDeltaOnHorizontalPlane(
                        viewM, projM, projMin, projMax, s_smmDragPlanePrev, curM, 0.f, dwx, dwz)) {
                    if (!s_smmDragUndoPushed) {
                        Smm::PushSceneUndoSnapshot(scene, settings.compressPrlxForUndo);
                        s_smmDragUndoPushed = true;
                    }
                    if (s_viewAxis == LibUI::Viewport::TransformAxis::X) {
                        dwz = 0.f;
                    } else if (s_viewAxis == LibUI::Viewport::TransformAxis::Z) {
                        dwx = 0.f;
                    } else if (s_viewAxis == LibUI::Viewport::TransformAxis::Y) {
                        dwx = 0.f;
                        dwz = 0.f;
                        dwy = -(curM.y - prevM.y) * 0.01f;
                        if (ioVp.KeyShift) {
                            dwy = std::round(dwy / 0.1f) * 0.1f;
                        }
                    }
                    auto applyParticleManual = [&]() {
                        if (smmParticleEi != Solstice::Parallax::PARALLAX_INVALID_INDEX && settings.manualParticleEmitterWorld
                            && settings.manualParticleEmitterWorldVec) {
                            *settings.manualParticleEmitterWorld = true;
                            settings.manualParticleEmitterWorldVec->x += dwx;
                            settings.manualParticleEmitterWorldVec->y += dwy;
                            settings.manualParticleEmitterWorldVec->z += dwz;
                            dirty = true;
                        }
                    };
                    if (settings.viewportSelectedElements && settings.viewportSelectedElements->size() > 1) {
                        bool didParticleManual = false;
                        for (int el : *settings.viewportSelectedElements) {
                            if (smmParticleEi != Solstice::Parallax::PARALLAX_INVALID_INDEX
                                && el == static_cast<int>(smmParticleEi)) {
                                if (!didParticleManual) {
                                    applyParticleManual();
                                    didParticleManual = true;
                                }
                            } else {
                                SmmApplyElementWorldDelta(scene, static_cast<Solstice::Parallax::ElementIndex>(el), dwx, dwy, dwz,
                                    settings.compressPrlxForUndo, dirty);
                            }
                        }
                    } else if (smmParticleEi != Solstice::Parallax::PARALLAX_INVALID_INDEX && dei == smmParticleEi) {
                        applyParticleManual();
                    } else {
                        SmmApplyElementWorldDelta(scene, dei, dwx, dwy, dwz, settings.compressPrlxForUndo, dirty);
                    }
                    s_smmDragPlanePrev = curM;
                }
            }
        }

        if (LibUI::Viewport::BeginViewportContextPopup("smm_uview_ctx", vp.hovered)) {
            const int pEl = SmmPrimarySel(settings);
            if (ImGui::MenuItem("Focus (F)", nullptr, false, pEl >= 0 && settings.camera != nullptr)) {
                if (settings.camera && pEl >= 0) {
                    for (const auto& et : eval.ElementTransforms) {
                        if (static_cast<int>(et.Element) == pEl) {
                            LibUI::Viewport::FocusOrbitOnTarget(*settings.camera, et.Position.x, et.Position.y, et.Position.z, 0.f, 0.f, 0.f);
                            break;
                        }
                    }
                    for (const auto& ls : eval.LightStates) {
                        if (static_cast<int>(ls.Element) == pEl) {
                            LibUI::Viewport::FocusOrbitOnTarget(*settings.camera, ls.Position.x, ls.Position.y, ls.Position.z, 0.f, 0.f, 0.f);
                            break;
                        }
                    }
                }
            }
            if (ImGui::MenuItem("Clear viewport selection", nullptr, false, settings.viewportSelectedElements != nullptr && !settings.viewportSelectedElements->empty())) {
                if (settings.viewportSelectedElements) {
                    settings.viewportSelectedElements->clear();
                }
                if (settings.primaryElementIndex) {
                    *settings.primaryElementIndex = -1;
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Axis lock: free", nullptr, s_viewAxis == LibUI::Viewport::TransformAxis::None)) {
                s_viewAxis = LibUI::Viewport::TransformAxis::None;
            }
            if (ImGui::MenuItem("Axis lock: X", nullptr, s_viewAxis == LibUI::Viewport::TransformAxis::X)) {
                s_viewAxis = LibUI::Viewport::TransformAxis::X;
            }
            if (ImGui::MenuItem("Axis lock: Y", nullptr, s_viewAxis == LibUI::Viewport::TransformAxis::Y)) {
                s_viewAxis = LibUI::Viewport::TransformAxis::Y;
            }
            if (ImGui::MenuItem("Axis lock: Z", nullptr, s_viewAxis == LibUI::Viewport::TransformAxis::Z)) {
                s_viewAxis = LibUI::Viewport::TransformAxis::Z;
            }
            if (hasSelectedMgSprite && selectedMg < scene.GetMGElements().size() && settings.sceneDirty != nullptr) {
                ImGui::Separator();
                if (ImGui::MenuItem("Flip selected raster X")) {
                    Smm::PushSceneUndoSnapshot(scene, settings.compressPrlxForUndo);
                    auto& mgMut = scene.GetMGElements()[selectedMg];
                    Solstice::Math::Vec2 sz = ReadMgVec2(mgMut, "Size", Solstice::Math::Vec2{256.f, 256.f});
                    sz.x = -sz.x;
                    mgMut.Attributes["Size"] = Solstice::Parallax::AttributeValue{sz};
                    *settings.sceneDirty = true;
                }
                if (ImGui::MenuItem("Flip selected raster Y")) {
                    Smm::PushSceneUndoSnapshot(scene, settings.compressPrlxForUndo);
                    auto& mgMut = scene.GetMGElements()[selectedMg];
                    Solstice::Math::Vec2 sz = ReadMgVec2(mgMut, "Size", Solstice::Math::Vec2{256.f, 256.f});
                    sz.y = -sz.y;
                    mgMut.Attributes["Size"] = Solstice::Parallax::AttributeValue{sz};
                    *settings.sceneDirty = true;
                }
            }
            ImGui::EndPopup();
        }

        LibUI::Viewport::ApplyOrbitPanZoom(nav, vp, {}, s_smmOrbitSuppress.ShouldSuppressOrbitLeftDrag());
        char overlay[520]{};
        const bool hasTex = settings.bindPreviewMaterialMaps
            && ((settings.previewMaterialAlbedoUtf8 && settings.previewMaterialAlbedoUtf8[0] != '\0')
                || (settings.previewMaterialNormalUtf8 && settings.previewMaterialNormalUtf8[0] != '\0')
                || (settings.previewMaterialRoughnessUtf8 && settings.previewMaterialRoughnessUtf8[0] != '\0'));
        const bool hasPtk = particles && particles->useImportedSprite && particles->particleSpritePath[0] != '\0';
        std::string flags;
        if (wantSmat) {
            flags += " | smat";
            if (settings.smatSelectedOnly) {
                flags += "(sel)";
            }
        }
        if (hasTex) {
            flags += " | tex";
        }
        if (hasPtk) {
            flags += " | ptk";
        }
        if (!settings.showFluidVolumeOverlay) {
            flags += " | fluid off";
        }
        if ((settings.mgWorkflowMode == 0) && settings.disable3DInPure2D) {
            flags += " | pure2d no3d";
        }
        if (SceneHasActorMeshAssets(scene)) {
            flags += " | mesh proxy";
        }
        std::snprintf(overlay, sizeof(overlay),
            "tool:%s axis:%s (%s, X/Y/Z lock, Esc free) | MG:%zu spr:%d | fluid:%zu | particles:%zu%s",
            LibUI::Viewport::TransformToolLabel(s_viewTool), LibUI::Viewport::TransformAxisLabel(s_viewAxis),
            LibUI::Viewport::TransformToolUsageHint(true),
            mgList.Entries.size(), mgSpriteCount, eval.FluidVolumes.size(), particles ? particles->particles.size() : size_t{0},
            flags.c_str());
        LibUI::Viewport::DrawViewportLabel(vp.draw_list, vp.min, vp.max, overlay, ImVec2(1.0f, 0.0f));
        {
            Solstice::Parallax::ParallaxSceneSummary sum{};
            Solstice::Parallax::GetParallaxSceneSummary(scene, sum);
            std::vector<Solstice::Parallax::ParallaxValidationMessage> val{};
            Solstice::Parallax::ValidateParallaxSceneEditing(scene, val);
            const float fps = ImGui::GetIO().Framerate;
            const float frameMs = (fps > 1e-4f) ? (1000.0f / fps) : 0.0f;
            char health[360]{};
            if (!val.empty()) {
                const char* t = val[0].Text.c_str();
                const int budget = 280;
                if (static_cast<int>(val[0].Text.size()) > budget) {
                    std::snprintf(health, sizeof(health),
                        "Scene: %zu el, %zu ch  |  first issue: %.*s…  |  %.1f FPS (%.2f ms)",
                        sum.ElementCount, sum.ChannelCount, budget, t, fps, frameMs);
                } else {
                    std::snprintf(health, sizeof(health),
                        "Scene: %zu el, %zu ch  |  %s  |  %.1f FPS (%.2f ms)",
                        sum.ElementCount, sum.ChannelCount, t, fps, frameMs);
                }
            } else {
                std::snprintf(health, sizeof(health),
                    "Scene: %zu elements · %zu channels · %zu lights · %zu fluid volumes  |  no issues  |  %.1f FPS (%.2f ms)",
                    sum.ElementCount, sum.ChannelCount, eval.LightStates.size(), eval.FluidVolumes.size(), fps, frameMs);
            }
            LibUI::Viewport::DrawViewportLabel(vp.draw_list, vp.min, vp.max, health, ImVec2(0.0f, 1.0f));
        }
    }

    LibUI::Viewport::EndHost();
}

} // namespace Solstice::MovieMaker::UI::Panels
