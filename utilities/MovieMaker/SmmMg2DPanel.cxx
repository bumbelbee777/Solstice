#include "SmmMg2DPanel.hxx"

#include "SmmFileOps.hxx"

#include <imgui.h>

#include <Math/Vector.hxx>
#include <unordered_map>
#include <algorithm>
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
