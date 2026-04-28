#include "SmmMg2DPanel.hxx"

#include "SmmFileOps.hxx"

#include <imgui.h>

#include <Math/Vector.hxx>
#include <Parallax/ParallaxTypes.hxx>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <string_view>

namespace Smm {
namespace {

static std::string_view MgSchema(
    const Solstice::Parallax::ParallaxScene& scene, const Solstice::Parallax::MGElementRecord& m) {
    if (m.SchemaIndex < scene.GetSchemas().size()) {
        return scene.GetSchemas()[m.SchemaIndex].TypeName;
    }
    return {};
}

static Solstice::Math::Vec2 ReadVec2(const std::unordered_map<std::string, Solstice::Parallax::AttributeValue>& attrs,
    const char* k, const Solstice::Math::Vec2& dflt) {
    const auto it = attrs.find(k);
    if (it == attrs.end()) {
        return dflt;
    }
    if (const auto* p = std::get_if<Solstice::Math::Vec2>(&it->second)) {
        return *p;
    }
    return dflt;
}

static float ReadF(const std::unordered_map<std::string, Solstice::Parallax::AttributeValue>& attrs, const char* k, float d) {
    const auto it = attrs.find(k);
    if (it == attrs.end()) {
        return d;
    }
    if (const auto* f = std::get_if<float>(&it->second)) {
        return *f;
    }
    return d;
}

static int ReadI(const std::unordered_map<std::string, Solstice::Parallax::AttributeValue>& attrs, const char* k, int d) {
    const auto it = attrs.find(k);
    if (it == attrs.end()) {
        return d;
    }
    if (const auto* in = std::get_if<int32_t>(&it->second)) {
        return static_cast<int>(*in);
    }
    return d;
}

static Solstice::Math::Vec3 ReadV3(
    const std::unordered_map<std::string, Solstice::Parallax::AttributeValue>& attrs, const char* k, const Solstice::Math::Vec3& d) {
    const auto it = attrs.find(k);
    if (it == attrs.end()) {
        return d;
    }
    if (const auto* p = std::get_if<Solstice::Math::Vec3>(&it->second)) {
        return *p;
    }
    return d;
}

} // namespace

void DrawMg2DCompTools(
    Solstice::Parallax::ParallaxScene& scene, int& mgElementSelected, bool& sceneDirty, bool compressPrlx, float& compW, float& compH) {
    compW = (std::max)(compW, 1.f);
    compH = (std::max)(compH, 1.f);
    ImGui::TextUnformatted("2D comp (nomin. px) — nudge, align, snap (AE-style working space)");
    float comp2[2] = {compW, compH};
    if (ImGui::DragFloat2("Nominal comp size", comp2, 1.f, 16.f, 16384.f)) {
        compW = comp2[0];
        compH = comp2[1];
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::SetTooltip("Used for alignment math only. Does not set render/export resolution (configure that in Export).");
    }
    ImGui::SameLine();
    if (ImGui::Button("HD 16:9##mgcomp")) {
        compW = 1920.f;
        compH = 1080.f;
    }
    ImGui::SameLine();
    if (ImGui::Button("9:16##mgcom2")) {
        compW = 1080.f;
        compH = 1920.f;
    }
    ImGui::SameLine();
    if (ImGui::Button("1:1##mgcom3")) {
        const float s = 0.5f * (compW + compH);
        compW = s;
        compH = s;
    }

    for (size_t i = 0; i < scene.GetMGElements().size(); ++i) {
        if (MgSchema(scene, scene.GetMGElements()[i]) != "MotionGraphicsRootElement") {
            continue;
        }
        auto& m = scene.GetMGElements()[i];
        float a = 1.f;
        if (const auto it = m.Attributes.find("CompositeAlpha"); it != m.Attributes.end()) {
            if (const auto* f = std::get_if<float>(&it->second)) {
                a = *f;
            }
        }
        if (ImGui::SliderFloat("MG layer opacity (root CompositeAlpha)", &a, 0.f, 1.f)) {
            Smm::PushSceneUndoSnapshot(scene, compressPrlx);
            m.Attributes["CompositeAlpha"] = Solstice::Parallax::AttributeValue{a};
            sceneDirty = true;
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
            ImGui::SetTooltip("Multiplies the whole 2D layer in EvaluateMG (0 = invisible, 1 = full).");
        }
        break;
    }

    if (ImGui::Button("Add MG Text##mgtxt")) {
        const Solstice::Parallax::MGIndex idx = Solstice::Parallax::AddMGElement(
            scene, "MGTextElement", "Text", Solstice::Parallax::PARALLAX_INVALID_INDEX);
        if (idx != Solstice::Parallax::PARALLAX_INVALID_INDEX) {
            Smm::PushSceneUndoSnapshot(scene, compressPrlx);
            auto& rec = scene.GetMGElements()[idx];
            rec.Attributes["Text"] = Solstice::Parallax::AttributeValue{std::string("Text")};
            rec.Attributes["Position"] = Solstice::Parallax::AttributeValue{Solstice::Math::Vec2(0.5f * compW, 0.5f * compH)};
            rec.Attributes["Color"] = Solstice::Parallax::AttributeValue{Solstice::Math::Vec4(1.f, 1.f, 1.f, 1.f)};
            mgElementSelected = static_cast<int>(idx);
            sceneDirty = true;
        }
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::SetTooltip("Lower-third friendly default position near comp center. Adjust in Transform below.");
    }

    if (mgElementSelected < 0 || static_cast<size_t>(mgElementSelected) >= scene.GetMGElements().size()) {
        return;
    }
    const std::string_view st = MgSchema(scene, scene.GetMGElements()[static_cast<size_t>(mgElementSelected)]);
    if (st == "MotionGraphicsRootElement") {
        auto& mgMut = scene.GetMGElements()[static_cast<size_t>(mgElementSelected)];
        ImGui::Separator();
        ImGui::TextUnformatted("Root (time scale, shake, post)");
        {
            float ts = ReadF(mgMut.Attributes, "MGTimeScale", 1.f);
            if (ImGui::DragFloat("MG time scale", &ts, 0.01f, 0.f, 8.f)) {
                Smm::PushSceneUndoSnapshot(scene, compressPrlx);
                mgMut.Attributes["MGTimeScale"] = Solstice::Parallax::AttributeValue{ts};
                sceneDirty = true;
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
                ImGui::SetTooltip("All MG tracks use evalTick = wall time * this (linear; not full spline time-remap).");
            }
        }
        {
            float s2[2] = {ReadF(mgMut.Attributes, "ScreenShakeAmpX", 0.f), ReadF(mgMut.Attributes, "ScreenShakeAmpY", 0.f)};
            if (ImGui::DragFloat2("Screen shake amp (px)", s2, 0.5f, 0.f, 200.f)) {
                Smm::PushSceneUndoSnapshot(scene, compressPrlx);
                mgMut.Attributes["ScreenShakeAmpX"] = Solstice::Parallax::AttributeValue{s2[0]};
                mgMut.Attributes["ScreenShakeAmpY"] = Solstice::Parallax::AttributeValue{s2[1]};
                sceneDirty = true;
            }
        }
        {
            float sh[2] = {ReadF(mgMut.Attributes, "ScreenShakeFrequency", 1.f), ReadF(mgMut.Attributes, "ScreenShakePhase", 0.f)};
            if (ImGui::DragFloat2("Shake freq / phase", sh, 0.01f, -32.f, 32.f)) {
                Smm::PushSceneUndoSnapshot(scene, compressPrlx);
                mgMut.Attributes["ScreenShakeFrequency"] = Solstice::Parallax::AttributeValue{sh[0]};
                mgMut.Attributes["ScreenShakePhase"] = Solstice::Parallax::AttributeValue{sh[1]};
                sceneDirty = true;
            }
        }
        float capx = ReadF(mgMut.Attributes, "ChromaticAberration", 0.f);
        if (ImGui::DragFloat("Chromatic aberration (px)", &capx, 0.1f, 0.f, 12.f)) {
            Smm::PushSceneUndoSnapshot(scene, compressPrlx);
            mgMut.Attributes["ChromaticAberration"] = Solstice::Parallax::AttributeValue{capx};
            sceneDirty = true;
        }
        {
            float g4[4] = {ReadF(mgMut.Attributes, "GradeExposure", 1.f), ReadF(mgMut.Attributes, "GradeSaturation", 1.f),
                ReadF(mgMut.Attributes, "GradeContrast", 1.f), ReadF(mgMut.Attributes, "GradeLift", 0.f)};
            if (ImGui::DragFloat4("Grade: exposure, sat, contrast, lift", g4, 0.01f, -0.5f, 4.f)) {
                Smm::PushSceneUndoSnapshot(scene, compressPrlx);
                mgMut.Attributes["GradeExposure"] = Solstice::Parallax::AttributeValue{g4[0]};
                mgMut.Attributes["GradeSaturation"] = Solstice::Parallax::AttributeValue{g4[1]};
                mgMut.Attributes["GradeContrast"] = Solstice::Parallax::AttributeValue{g4[2]};
                mgMut.Attributes["GradeLift"] = Solstice::Parallax::AttributeValue{g4[3]};
                sceneDirty = true;
            }
        }
        return;
    }
    if (st != "MGSpriteElement" && st != "MGTextElement") {
        return;
    }
    auto& mgMut = scene.GetMGElements()[static_cast<size_t>(mgElementSelected)];

    ImGui::Separator();
    ImGui::TextUnformatted("Transform (2D layer)");
    {
        const Solstice::Math::Vec2 pos = ReadVec2(mgMut.Attributes, "Position", Solstice::Math::Vec2{0, 0});
        float p2[2] = {pos.x, pos.y};
        if (ImGui::DragFloat2("Position (px)", p2, 0.5f, -1.0e6f, 1.0e6f)) {
            Smm::PushSceneUndoSnapshot(scene, compressPrlx);
            mgMut.Attributes["Position"] = Solstice::Parallax::AttributeValue{Solstice::Math::Vec2{p2[0], p2[1]}};
            sceneDirty = true;
        }
        const auto nud = [&](const char* id, float dx, float dy) {
            if (ImGui::Button(id)) {
                Smm::PushSceneUndoSnapshot(scene, compressPrlx);
                Solstice::Math::Vec2 p = ReadVec2(mgMut.Attributes, "Position", Solstice::Math::Vec2{0, 0});
                p.x += dx;
                p.y += dy;
                mgMut.Attributes["Position"] = Solstice::Parallax::AttributeValue{p};
                sceneDirty = true;
            }
        };
        ImGui::TextUnformatted("Nudge");
        nud("<<", -10.f, 0.f);
        ImGui::SameLine();
        nud("<", -1.f, 0.f);
        ImGui::SameLine();
        nud(">", 1.f, 0.f);
        ImGui::SameLine();
        nud(">>", 10.f, 0.f);
        ImGui::SameLine();
        nud("up10", 0.f, -10.f);
        ImGui::SameLine();
        nud("up1", 0.f, -1.f);
        ImGui::SameLine();
        nud("dn1", 0.f, 1.f);
        ImGui::SameLine();
        nud("dn10", 0.f, 10.f);
    }
    if (st == "MGSpriteElement") {
        static bool sLinkWh = true;
        ImGui::Checkbox("Link width / height (uniform scale)##mglk", &sLinkWh);
        const Solstice::Math::Vec2 oldSz = ReadVec2(mgMut.Attributes, "Size", Solstice::Math::Vec2{256, 256});
        float sz2[2] = {oldSz.x, oldSz.y};
        if (ImGui::DragFloat2("Size (px)", sz2, 0.5f, 2.f, 8192.f)) {
            Smm::PushSceneUndoSnapshot(scene, compressPrlx);
            if (sLinkWh) {
                if (oldSz.x > 0.001f && std::abs(sz2[0] - oldSz.x) > 0.0001f) {
                    const float r = oldSz.y / oldSz.x;
                    sz2[1] = sz2[0] * r;
                } else if (oldSz.y > 0.001f && std::abs(sz2[1] - oldSz.y) > 0.0001f) {
                    const float r = oldSz.x / oldSz.y;
                    sz2[0] = sz2[1] * r;
                }
            }
            mgMut.Attributes["Size"] = Solstice::Parallax::AttributeValue{Solstice::Math::Vec2{sz2[0], sz2[1]}};
            sceneDirty = true;
        }
        const Solstice::Math::Vec2 cSz = ReadVec2(mgMut.Attributes, "Size", Solstice::Math::Vec2{sz2[0], sz2[1]});
        const auto al = [&](const char* label, float nx, float ny) {
            if (ImGui::Button(label)) {
                Smm::PushSceneUndoSnapshot(scene, compressPrlx);
                mgMut.Attributes["Position"] = Solstice::Parallax::AttributeValue{Solstice::Math::Vec2{nx, ny}};
                sceneDirty = true;
            }
        };
        ImGui::TextUnformatted("Align in comp (sprite: upper-left anchor)");
        al("L", 0.f, ReadVec2(mgMut.Attributes, "Position", {}).y);
        ImGui::SameLine();
        al("C", 0.5f * (compW - cSz.x), ReadVec2(mgMut.Attributes, "Position", {}).y);
        ImGui::SameLine();
        al("R", compW - cSz.x, ReadVec2(mgMut.Attributes, "Position", {}).y);
        const float x = ReadVec2(mgMut.Attributes, "Position", {}).x;
        al("T", x, 0.f);
        ImGui::SameLine();
        al("M", x, 0.5f * (compH - cSz.y));
        ImGui::SameLine();
        al("B", x, compH - cSz.y);

        ImGui::Separator();
        ImGui::TextUnformatted("Sprite FX (export + ImGui / CPU paths)");
        const float rads = ReadF(mgMut.Attributes, "RotationZ", 0.f);
        float degs = rads * 180.f / 3.14159265f;
        if (ImGui::SliderFloat("Rotation Z (deg)", &degs, -180.f, 180.f)) {
            Smm::PushSceneUndoSnapshot(scene, compressPrlx);
            mgMut.Attributes["RotationZ"] = Solstice::Parallax::AttributeValue{degs * 3.14159265f / 180.f};
            sceneDirty = true;
        }
        int bbm = ReadI(mgMut.Attributes, "BillboardMode", 0);
        if (ImGui::SliderInt("Billboard mode (0=off)", &bbm, 0, 2)) {
            Smm::PushSceneUndoSnapshot(scene, compressPrlx);
            mgMut.Attributes["BillboardMode"] = Solstice::Parallax::AttributeValue{static_cast<int32_t>(bbm)};
            sceneDirty = true;
        }
        int smear = ReadI(mgMut.Attributes, "SmearCount", 0);
        float sfall = ReadF(mgMut.Attributes, "SmearFalloff", 0.65f);
        if (ImGui::SliderInt("Smear count", &smear, 0, 8)) {
            Smm::PushSceneUndoSnapshot(scene, compressPrlx);
            mgMut.Attributes["SmearCount"] = Solstice::Parallax::AttributeValue{static_cast<int32_t>(smear)};
            sceneDirty = true;
        }
        {
            float sm2[2] = {ReadF(mgMut.Attributes, "SmearDx", 0.f), ReadF(mgMut.Attributes, "SmearDy", 0.f)};
            if (ImGui::DragFloat2("Smear dxy (px per ghost)", sm2, 0.25f, -256.f, 256.f)) {
                Smm::PushSceneUndoSnapshot(scene, compressPrlx);
                mgMut.Attributes["SmearDx"] = Solstice::Parallax::AttributeValue{sm2[0]};
                mgMut.Attributes["SmearDy"] = Solstice::Parallax::AttributeValue{sm2[1]};
                sceneDirty = true;
            }
        }
        if (ImGui::SliderFloat("Smear falloff", &sfall, 0.05f, 0.99f)) {
            Smm::PushSceneUndoSnapshot(scene, compressPrlx);
            mgMut.Attributes["SmearFalloff"] = Solstice::Parallax::AttributeValue{sfall};
            sceneDirty = true;
        }
        {
            float uv[2] = {ReadF(mgMut.Attributes, "UvScrollU", 0.f), ReadF(mgMut.Attributes, "UvScrollV", 0.f)};
            if (ImGui::DragFloat2("UV scroll (wrap)", uv, 0.01f, -8.f, 8.f)) {
                Smm::PushSceneUndoSnapshot(scene, compressPrlx);
                mgMut.Attributes["UvScrollU"] = Solstice::Parallax::AttributeValue{uv[0]};
                mgMut.Attributes["UvScrollV"] = Solstice::Parallax::AttributeValue{uv[1]};
                sceneDirty = true;
            }
        }
        {
            float wv2[2] = {ReadF(mgMut.Attributes, "UvWaveU", 0.f), ReadF(mgMut.Attributes, "UvWaveV", 0.f)};
            if (ImGui::DragFloat2("UV wave amp", wv2, 0.002f, 0.f, 0.5f)) {
                Smm::PushSceneUndoSnapshot(scene, compressPrlx);
                mgMut.Attributes["UvWaveU"] = Solstice::Parallax::AttributeValue{wv2[0]};
                mgMut.Attributes["UvWaveV"] = Solstice::Parallax::AttributeValue{wv2[1]};
                sceneDirty = true;
            }
        }
        float wph = ReadF(mgMut.Attributes, "UvPhase", 0.f);
        if (ImGui::DragFloat("UV phase (animate key)", &wph, 0.02f, -64.f, 64.f)) {
            Smm::PushSceneUndoSnapshot(scene, compressPrlx);
            mgMut.Attributes["UvPhase"] = Solstice::Parallax::AttributeValue{wph};
            sceneDirty = true;
        }
        float aoM = ReadF(mgMut.Attributes, "AOMultiply", 1.f);
        float aoE = ReadF(mgMut.Attributes, "AOEdge", 0.f);
        if (ImGui::SliderFloat("AO multiply", &aoM, 0.f, 2.f)) {
            Smm::PushSceneUndoSnapshot(scene, compressPrlx);
            mgMut.Attributes["AOMultiply"] = Solstice::Parallax::AttributeValue{aoM};
            sceneDirty = true;
        }
        if (ImGui::SliderFloat("AO edge (sprite)", &aoE, 0.f, 1.f)) {
            Smm::PushSceneUndoSnapshot(scene, compressPrlx);
            mgMut.Attributes["AOEdge"] = Solstice::Parallax::AttributeValue{aoE};
            sceneDirty = true;
        }
        {
            ImGui::Separator();
            ImGui::TextUnformatted("Key / rotoscope / zoom blur (CPU + export)");
            Solstice::Math::Vec3 kc = ReadV3(mgMut.Attributes, "ChromaKeyColor", Solstice::Math::Vec3(0.f, 1.f, 0.f));
            float c3[3] = {kc.x, kc.y, kc.z};
            if (ImGui::ColorEdit3("Chroma key (linear RGB, tol>0=on)", c3, ImGuiColorEditFlags_Float)) {
                Smm::PushSceneUndoSnapshot(scene, compressPrlx);
                mgMut.Attributes["ChromaKeyColor"] = Solstice::Parallax::AttributeValue{
                    Solstice::Math::Vec3(c3[0], c3[1], c3[2])};
                sceneDirty = true;
            }
            float ckt = ReadF(mgMut.Attributes, "ChromaKeyTolerance", 0.f);
            float ckf = ReadF(mgMut.Attributes, "ChromaKeyFeather", 0.05f);
            if (ImGui::DragFloat("Chroma key tolerance (0=off)", &ckt, 0.01f, 0.f, 1.f)) {
                Smm::PushSceneUndoSnapshot(scene, compressPrlx);
                mgMut.Attributes["ChromaKeyTolerance"] = Solstice::Parallax::AttributeValue{ckt};
                sceneDirty = true;
            }
            if (ImGui::DragFloat("Chroma key feather", &ckf, 0.01f, 0.001f, 0.5f)) {
                Smm::PushSceneUndoSnapshot(scene, compressPrlx);
                mgMut.Attributes["ChromaKeyFeather"] = Solstice::Parallax::AttributeValue{ckf};
                sceneDirty = true;
            }
        }
        {
            float rs = ReadF(mgMut.Attributes, "RotoscopeStrength", 0.f);
            float re = ReadF(mgMut.Attributes, "RotoscopeEdgePx", 2.f);
            if (ImGui::DragFloat("Rotoscope strength (alpha edge darkening)", &rs, 0.02f, 0.f, 2.f)) {
                Smm::PushSceneUndoSnapshot(scene, compressPrlx);
                mgMut.Attributes["RotoscopeStrength"] = Solstice::Parallax::AttributeValue{rs};
                sceneDirty = true;
            }
            if (ImGui::DragFloat("Rotoscope edge (texture px scale)", &re, 0.1f, 0.1f, 12.f)) {
                Smm::PushSceneUndoSnapshot(scene, compressPrlx);
                mgMut.Attributes["RotoscopeEdgePx"] = Solstice::Parallax::AttributeValue{re};
                sceneDirty = true;
            }
        }
        {
            float zb = ReadF(mgMut.Attributes, "ZoomBlur", 0.f);
            float z2[2] = {ReadF(mgMut.Attributes, "ZoomBlurCenterU", 0.5f), ReadF(mgMut.Attributes, "ZoomBlurCenterV", 0.5f)};
            if (ImGui::DragFloat("Zoom / radial blur", &zb, 0.01f, 0.f, 1.f)) {
                Smm::PushSceneUndoSnapshot(scene, compressPrlx);
                mgMut.Attributes["ZoomBlur"] = Solstice::Parallax::AttributeValue{zb};
                sceneDirty = true;
            }
            if (ImGui::DragFloat2("Blur center UV (0-1 texture)", z2, 0.01f, 0.f, 1.f)) {
                Smm::PushSceneUndoSnapshot(scene, compressPrlx);
                mgMut.Attributes["ZoomBlurCenterU"] = Solstice::Parallax::AttributeValue{z2[0]};
                mgMut.Attributes["ZoomBlurCenterV"] = Solstice::Parallax::AttributeValue{z2[1]};
                sceneDirty = true;
            }
        }
    } else {
        if (ImGui::Button("Center in comp (anchor ~ center)##altxt")) {
            Smm::PushSceneUndoSnapshot(scene, compressPrlx);
            mgMut.Attributes["Position"] =
                Solstice::Parallax::AttributeValue{Solstice::Math::Vec2(0.5f * compW, 0.5f * compH)};
            sceneDirty = true;
        }
    }
}

} // namespace Smm
