#include "PreviewPanels.hxx"

#include "Editing/SmmParticleEditor.hxx"
#include "EditorEnginePreview/EditorEnginePreview.hxx"
#include "LibUI/Tools/ViewportSpatialPick.hxx"
#include "LibUI/Viewport/Viewport.hxx"
#include "LibUI/Viewport/ViewportGizmo.hxx"
#include "LibUI/Viewport/ViewportMath.hxx"

#include <Arzachel/FacialAnimation.hxx>
#include <Parallax/MGRaster.hxx>
#include <Parallax/ParallaxEditorHelpers.hxx>

#include <Physics/Lighting/LightSource.hxx>

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <exception>
#include <span>
#include <string>
#include <vector>

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

} // namespace

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

void DrawUnifiedViewportPanel(SDL_Window* window, const Solstice::Parallax::ParallaxScene& scene,
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
        Solstice::Parallax::SceneEvaluationResult eval{};
        Solstice::Parallax::EvaluateScene(scene, timeTicks, eval);
        const Solstice::Parallax::MGDisplayList& mgList = eval.MotionGraphics;
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
            if (schemaType == "ActorElement") {
                const float talk = PreviewMouthOpenHint(eval, et.Element);
                pe.Scale.y = 1.f + 0.22f * talk;
            }
            const bool isCamera = schemaType == "CameraElement";
            const bool isActor = schemaType == "ActorElement";
            const bool body = !isCamera && (isActor || !settings.smatActorsOnly);
            const bool selOk = !settings.smatSelectedOnly
                || (settings.selectedElementIndex >= 0 && static_cast<int>(et.Element) == settings.selectedElementIndex);
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
        bool capOk = false;
        try {
            capOk = Solstice::EditorEnginePreview::CaptureOrbitRgb(nav, 0.f, 0.f, 0.f, 55.f, aspect, scW, scH, entities.data(),
                entities.size(), lights.data(), lights.size(), capture, capW, capH);
        } catch (const std::exception& ex) {
            WriteSink(settings.enginePreviewErrorSink, settings.enginePreviewErrorSinkBytes,
                (std::string("3D preview (exception): ") + ex.what()).c_str());
        } catch (...) {
            WriteSink(
                settings.enginePreviewErrorSink, settings.enginePreviewErrorSinkBytes, "3D preview failed (non-C++ exception).");
        }
        if (capOk) {
            ClearSink(settings.enginePreviewErrorSink, settings.enginePreviewErrorSinkBytes);
            if (mgOverlayAlpha > 1e-3f) {
                try {
                    std::vector<std::byte> mgRgba(static_cast<size_t>(capW) * static_cast<size_t>(capH) * 4u);
                    Solstice::Parallax::RasterizeMGDisplayList(mgList, &resolver, static_cast<uint32_t>(capW),
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
        } else {
            if (settings.enginePreviewErrorSink && settings.enginePreviewErrorSinkBytes > 0
                && settings.enginePreviewErrorSink[0] == '\0') {
                WriteSink(settings.enginePreviewErrorSink, settings.enginePreviewErrorSinkBytes,
                    "3D preview: GPU capture failed or returned empty (reduce panel size if this persists).");
            }
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
            if (settings.selectedElementIndex >= 0 && static_cast<int>(et.Element) == settings.selectedElementIndex) {
                LibUI::Viewport::DrawWorldAxisAlignedBoxSelectionOutlineUniformImGui(vp.draw_list, projMin, projMax, viewM, projM,
                    et.Position.x, et.Position.y, et.Position.z, hePick, IM_COL32(255, 230, 100, 255), 2.0f, 2.8f,
                    IM_COL32(16, 14, 8, 240), 4.0f);
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
        }

        if (particles && particles->enabled) {
            if (particleSpriteTexture) {
                Smm::Editing::SyncParticleSpritePreview(window, *particleSpriteTexture, *particles);
            }
            const float dt = ImGui::GetIO().DeltaTime;
            Smm::Editing::TickParticlePreview(*particles, emitterWorld, dt);
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

        if (settings.camera && vp.hovered && ImGui::IsKeyPressed(ImGuiKey_F, false) && settings.selectedElementIndex >= 0) {
            for (const auto& et : eval.ElementTransforms) {
                if (static_cast<int>(et.Element) == settings.selectedElementIndex) {
                    LibUI::Viewport::FocusOrbitOnTarget(
                        *settings.camera, et.Position.x, et.Position.y, et.Position.z, 0.f, 0.f, 0.f);
                    break;
                }
            }
        }

        if (settings.onViewportPickElement) {
            if (vp.hovered && ImGui::GetIO().KeyShift) {
                const ImVec2 drag = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
                if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && drag.x * drag.x + drag.y * drag.y < 25.f) {
                    float rox, roy, roz, rdx, rdy, rdz;
                    if (LibUI::Viewport::ScreenToWorldRay(viewM, projM, projMin, projMax, ImGui::GetMousePos(), rox, roy, roz, rdx, rdy,
                            rdz)) {
                        const int nEt = (std::min)(static_cast<int>(eval.ElementTransforms.size()), kSmmMaxViewportPickBoxes);
                        std::vector<LibUI::Tools::AxisAlignedBox3> boxes;
                        boxes.reserve(static_cast<size_t>(nEt));
                        for (int i = 0; i < nEt; ++i) {
                            const auto& t = eval.ElementTransforms[static_cast<size_t>(i)];
                            LibUI::Tools::AxisAlignedBox3 b{};
                            b.minX = t.Position.x - hePick;
                            b.minY = t.Position.y - hePick;
                            b.minZ = t.Position.z - hePick;
                            b.maxX = t.Position.x + hePick;
                            b.maxY = t.Position.y + hePick;
                            b.maxZ = t.Position.z + hePick;
                            boxes.push_back(b);
                        }
                        int bestIdx = -1;
                        float tHit = 0.f;
                        if (boxes.empty() || !LibUI::Tools::PickClosestAxisAlignedBoxAlongRay(
                                    rox, roy, roz, rdx, rdy, rdz, boxes.data(), nEt, bestIdx, tHit)) {
                            settings.onViewportPickElement(-1);
                        } else if (bestIdx >= 0 && bestIdx < nEt) {
                            settings.onViewportPickElement(static_cast<int>(eval.ElementTransforms[static_cast<size_t>(bestIdx)].Element));
                        }
                    }
                }
            }
        }

        LibUI::Viewport::ApplyOrbitPanZoom(nav, vp);
        char overlay[400]{};
        const bool hasTex = settings.bindPreviewMaterialMaps
            && ((settings.previewMaterialAlbedoUtf8 && settings.previewMaterialAlbedoUtf8[0] != '\0')
                || (settings.previewMaterialNormalUtf8 && settings.previewMaterialNormalUtf8[0] != '\0')
                || (settings.previewMaterialRoughnessUtf8 && settings.previewMaterialRoughnessUtf8[0] != '\0'));
        const bool hasPtk = particles && particles->useImportedSprite && particles->particleSpritePath[0] != '\0';
        const bool canPick = static_cast<bool>(settings.onViewportPickElement);
        std::snprintf(overlay, sizeof(overlay), "Unified — MG:%zu spr:%d | fluid:%zu | particles:%zu%s%s%s%s%s%s", mgList.Entries.size(),
            mgSpriteCount, eval.FluidVolumes.size(), particles ? particles->particles.size() : size_t{0}, wantSmat ? " | smat" : "",
            (wantSmat && settings.smatSelectedOnly) ? " (sel)" : "", hasTex ? " | tex" : "", hasPtk ? " | ptk" : "",
            settings.showFluidVolumeOverlay ? "" : " | fluid off", canPick ? " | Shift+click: pick" : "");
        LibUI::Viewport::DrawViewportLabel(vp.draw_list, vp.min, vp.max, overlay, ImVec2(1.0f, 0.0f));
        {
            Solstice::Parallax::ParallaxSceneSummary sum{};
            Solstice::Parallax::GetParallaxSceneSummary(scene, sum);
            std::vector<Solstice::Parallax::ParallaxValidationMessage> val{};
            Solstice::Parallax::ValidateParallaxSceneEditing(scene, val);
            char health[360]{};
            if (!val.empty()) {
                const char* t = val[0].Text.c_str();
                const int budget = 280;
                if (static_cast<int>(val[0].Text.size()) > budget) {
                    std::snprintf(health, sizeof(health), "Scene: %zu el, %zu ch  |  first issue: %.*s…", sum.ElementCount, sum.ChannelCount,
                        budget, t);
                } else {
                    std::snprintf(health, sizeof(health), "Scene: %zu el, %zu ch  |  %s", sum.ElementCount, sum.ChannelCount, t);
                }
            } else {
                std::snprintf(health, sizeof(health), "Scene: %zu elements · %zu channels · %zu lights · %zu fluid volumes  |  no issues",
                    sum.ElementCount, sum.ChannelCount, eval.LightStates.size(), eval.FluidVolumes.size());
            }
            LibUI::Viewport::DrawViewportLabel(vp.draw_list, vp.min, vp.max, health, ImVec2(0.0f, 1.0f));
        }
    }

    LibUI::Viewport::EndHost();
}

} // namespace Solstice::MovieMaker::UI::Panels
