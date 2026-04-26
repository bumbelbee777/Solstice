#include "SmmParallaxAuthoringPanel.hxx"

#include "Media/SmmImage.hxx"
#include "SmmFileOps.hxx"

#include <LibUI/FileDialogs/FileDialogs.hxx>

#include <Parallax/ParallaxScene.hxx>
#include <Parallax/ParallaxTypes.hxx>

#include <imgui.h>

#include <cstring>
#include <string>

namespace Smm {

namespace {

static const char* kFaceKeys[6] = {
    "SkyboxFacePosX", "SkyboxFaceNegX", "SkyboxFacePosY", "SkyboxFaceNegY", "SkyboxFacePosZ", "SkyboxFaceNegZ"};
static const char* kFaceLabels[6] = {"+X (pos)", "−X (neg)", "+Y (pos)", "−Y (neg)", "+Z (pos)", "−Z (neg)"};

std::string GetStr(const Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::ElementIndex el, const char* key) {
    const Solstice::Parallax::AttributeValue a = Solstice::Parallax::GetAttribute(scene, el, key);
    if (const auto* s = std::get_if<std::string>(&a)) {
        return *s;
    }
    return {};
}

} // namespace

void DrawParallaxRootEnvironment(
    Solstice::Parallax::ParallaxScene& scene, SDL_Window* window, bool compressPrlx, bool& sceneDirty) {
    if (scene.GetElements().empty()) {
        return;
    }
    if (Solstice::Parallax::GetElementSchema(scene, 0) != "SceneRoot") {
        return;
    }

    static char s_bufFace[6][512]{};
    static bool s_skyboxHeaderWasOpen = false;
    const bool open = ImGui::CollapsingHeader("Environment / skybox (Scene root)##prlxenv");
    if (open && !s_skyboxHeaderWasOpen) {
        for (int i = 0; i < 6; ++i) {
            const std::string p = GetStr(scene, 0, kFaceKeys[static_cast<size_t>(i)]);
            std::strncpy(
                s_bufFace[i], p.c_str(), sizeof(s_bufFace[0]) - 1u); s_bufFace[i][sizeof(s_bufFace[0]) - 1] = '\0';
        }
    }
    s_skyboxHeaderWasOpen = open;
    if (!open) {
        return;
    }

    ImGui::TextWrapped(
        "Same semantics as Jackhammer’s `.smf` skybox: six face image paths (often relative to the export). "
        "Stored on `SceneRoot` in `.prlx` for engines that resolve environment maps.");
    bool enabled = false;
    {
        const Solstice::Parallax::AttributeValue aEn = Solstice::Parallax::GetAttribute(scene, 0, "SkyboxEnabled");
        if (const auto* b = std::get_if<bool>(&aEn)) {
            enabled = *b;
        }
    }
    if (ImGui::Checkbox("Skybox enabled##prlxsb", &enabled)) {
        Smm::PushSceneUndoSnapshot(scene, compressPrlx);
        Solstice::Parallax::SetAttribute(scene, 0, "SkyboxEnabled", Solstice::Parallax::AttributeValue{enabled});
        sceneDirty = true;
    }
    float bright = 1.f;
    {
        const Solstice::Parallax::AttributeValue aBr = Solstice::Parallax::GetAttribute(scene, 0, "SkyboxBrightness");
        if (const auto* f = std::get_if<float>(&aBr)) {
            bright = *f;
        }
    }
    if (ImGui::DragFloat("Sky brightness", &bright, 0.01f, 0.f, 8.f)) {
        Smm::PushSceneUndoSnapshot(scene, compressPrlx);
        Solstice::Parallax::SetAttribute(scene, 0, "SkyboxBrightness", Solstice::Parallax::AttributeValue{bright});
        sceneDirty = true;
    }
    float yaw = 0.f;
    {
        const Solstice::Parallax::AttributeValue aYw = Solstice::Parallax::GetAttribute(scene, 0, "SkyboxYawDegrees");
        if (const auto* f = std::get_if<float>(&aYw)) {
            yaw = *f;
        }
    }
    if (ImGui::DragFloat("Sky yaw (deg, Y-up)", &yaw, 0.5f, -180.f, 180.f)) {
        Smm::PushSceneUndoSnapshot(scene, compressPrlx);
        Solstice::Parallax::SetAttribute(scene, 0, "SkyboxYawDegrees", Solstice::Parallax::AttributeValue{yaw});
        sceneDirty = true;
    }
    ImGui::Separator();
    for (int i = 0; i < 6; ++i) {
        ImGui::PushID(i);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 88.f);
        if (ImGui::InputText(kFaceLabels[static_cast<size_t>(i)], s_bufFace[i], sizeof(s_bufFace[0]))) {
            Smm::PushSceneUndoSnapshot(scene, compressPrlx);
            Solstice::Parallax::SetAttribute(scene, 0, kFaceKeys[static_cast<size_t>(i)],
                Solstice::Parallax::AttributeValue{std::string(s_bufFace[i])});
            sceneDirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Browse##sbf")) {
            const int faceIdx = i;
            LibUI::FileDialogs::ShowOpenFile(window, "Open skybox face image",
                [&, faceIdx](std::optional<std::string> path) {
                    if (!path) {
                        return;
                    }
                    Smm::PushSceneUndoSnapshot(scene, compressPrlx);
                    std::strncpy(
                        s_bufFace[faceIdx], path->c_str(), sizeof(s_bufFace[0]) - 1u); s_bufFace[faceIdx][sizeof(s_bufFace[0]) - 1] = '\0';
                    Solstice::Parallax::SetAttribute(scene, 0, kFaceKeys[static_cast<size_t>(faceIdx)],
                        Solstice::Parallax::AttributeValue{std::move(*path)});
                    sceneDirty = true;
                },
                std::span<const LibUI::FileDialogs::FileFilter>(Smm::Image::kRasterImportFilters));
        }
        ImGui::PopID();
    }
}

void DrawActorArzachelFields(
    Solstice::Parallax::ParallaxScene& scene, int elementIndex, bool compressPrlx, bool& sceneDirty) {
    if (elementIndex < 0 || static_cast<size_t>(elementIndex) >= scene.GetElements().size()) {
        return;
    }
    const Solstice::Parallax::ElementIndex el = static_cast<Solstice::Parallax::ElementIndex>(elementIndex);
    if (Solstice::Parallax::GetElementSchema(scene, el) != "ActorElement") {
        return;
    }
    if (!ImGui::CollapsingHeader("Arzachel, LOD, animation presets##prlxarz")) {
        return;
    }
    static int s_lastEl = -1;
    static char s_animPreset[256]{};
    static char s_destPreset[256]{};
    if (s_lastEl != elementIndex) {
        {
            const std::string a = GetStr(scene, el, "ArzachelAnimationClipPreset");
            std::strncpy(s_animPreset, a.c_str(), sizeof(s_animPreset) - 1);
            s_animPreset[sizeof(s_animPreset) - 1] = '\0';
        }
        {
            const std::string d = GetStr(scene, el, "ArzachelDestructionAnimPreset");
            std::strncpy(s_destPreset, d.c_str(), sizeof(s_destPreset) - 1);
            s_destPreset[sizeof(s_destPreset) - 1] = '\0';
        }
        s_lastEl = elementIndex;
    }

    float dmg = 0.f;
    {
        const Solstice::Parallax::AttributeValue aDmg = Solstice::Parallax::GetAttribute(scene, el, "ArzachelRigidBodyDamage");
        if (const auto* f = std::get_if<float>(&aDmg)) {
            dmg = *f;
        }
    }
    if (ImGui::SliderFloat("Rigid-body damage (Arzachel amount)", &dmg, 0.f, 1.f, "%.3f")) {
        Smm::PushSceneUndoSnapshot(scene, compressPrlx);
        Solstice::Parallax::SetAttribute(
            scene, el, "ArzachelRigidBodyDamage", Solstice::Parallax::AttributeValue{dmg});
        sceneDirty = true;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::SetTooltip(
            "Drives `Arzachel::Damaged(baseGenerator, seed, amount)`-style mesh variation for props (0 = off).");
    }
    float lodH = 0.f;
    {
        const Solstice::Parallax::AttributeValue aH = Solstice::Parallax::GetAttribute(scene, el, "LodDistanceHigh");
        if (const auto* f = std::get_if<float>(&aH)) {
            lodH = *f;
        }
    }
    float lodL = 0.f;
    {
        const Solstice::Parallax::AttributeValue aL = Solstice::Parallax::GetAttribute(scene, el, "LodDistanceLow");
        if (const auto* f = std::get_if<float>(&aL)) {
            lodL = *f;
        }
    }
    if (ImGui::DragFloat("LOD distance (high → coarse)", &lodH, 0.5f, 0.f, 1.0e6f)) {
        Smm::PushSceneUndoSnapshot(scene, compressPrlx);
        Solstice::Parallax::SetAttribute(scene, el, "LodDistanceHigh", Solstice::Parallax::AttributeValue{lodH});
        sceneDirty = true;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::SetTooltip("World units: distance beyond which a coarser draw may be used. 0 = engine default.");
    }
    if (ImGui::DragFloat("Max draw distance", &lodL, 0.5f, 0.f, 1.0e6f)) {
        Smm::PushSceneUndoSnapshot(scene, compressPrlx);
        Solstice::Parallax::SetAttribute(scene, el, "LodDistanceLow", Solstice::Parallax::AttributeValue{lodL});
        sceneDirty = true;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::SetTooltip("Optional cull / far LOD bound. 0 = engine default.");
    }

    if (ImGui::InputText("Animation clip preset", s_animPreset, sizeof(s_animPreset))) {
        Smm::PushSceneUndoSnapshot(scene, compressPrlx);
        Solstice::Parallax::SetAttribute(
            scene, el, "ArzachelAnimationClipPreset", Solstice::Parallax::AttributeValue{std::string(s_animPreset)});
        sceneDirty = true;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::SetTooltip("Authoring label paired with the `AnimationClip` asset (engine / tooling-defined names).");
    }
    if (ImGui::InputText("Destruction / breakup preset", s_destPreset, sizeof(s_destPreset))) {
        Smm::PushSceneUndoSnapshot(scene, compressPrlx);
        Solstice::Parallax::SetAttribute(scene, el, "ArzachelDestructionAnimPreset",
            Solstice::Parallax::AttributeValue{std::string(s_destPreset)});
        sceneDirty = true;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::SetTooltip("Pairs with **Rigid-body damage** for scripted destruction or a second anim track.");
    }
}

} // namespace Smm
