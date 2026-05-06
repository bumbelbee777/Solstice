#include "SmmFluidPanel.hxx"

#include "../SmmFileOps.hxx"
#include "LibUI/Widgets/Widgets.hxx"

#include <Parallax/ParallaxScene.hxx>
#include <Parallax/ParallaxTypes.hxx>

#include <Math/Vector.hxx>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace Smm::Editing {

namespace {

std::string MakeUniqueFluidElementName(const Solstice::Parallax::ParallaxScene& scene, const char* prefix) {
    for (int n = 1; n < 100000; ++n) {
        std::string t = std::string(prefix) + "_" + std::to_string(n);
        bool clash = false;
        for (const auto& e : scene.GetElements()) {
            if (e.Name == t) {
                clash = true;
                break;
            }
        }
        if (!clash) {
            return t;
        }
    }
    return std::string(prefix) + "_unnamed";
}

void CollectFluidVolumeElementIndices(const Solstice::Parallax::ParallaxScene& scene, std::vector<Solstice::Parallax::ElementIndex>& out) {
    out.clear();
    for (Solstice::Parallax::ElementIndex i = 0; i < scene.GetElements().size(); ++i) {
        if (Solstice::Parallax::GetElementSchema(scene, i) == "SmmFluidVolumeElement") {
            out.push_back(i);
        }
    }
}

void CollectSoftBodyElementIndices(const Solstice::Parallax::ParallaxScene& scene, std::vector<Solstice::Parallax::ElementIndex>& out) {
    out.clear();
    for (Solstice::Parallax::ElementIndex i = 0; i < scene.GetElements().size(); ++i) {
        if (Solstice::Parallax::GetElementSchema(scene, i) == "SmmSoftBodyElement") {
            out.push_back(i);
        }
    }
}

void CollectVehicleElementIndices(const Solstice::Parallax::ParallaxScene& scene, std::vector<Solstice::Parallax::ElementIndex>& out) {
    out.clear();
    for (Solstice::Parallax::ElementIndex i = 0; i < scene.GetElements().size(); ++i) {
        if (Solstice::Parallax::GetElementSchema(scene, i) == "SmmVehicleElement") {
            out.push_back(i);
        }
    }
}

void ApplyDefaultFluidAttributes(Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::ElementIndex e) {
    using namespace Solstice::Parallax;
    SetAttribute(scene, e, "Enabled", AttributeValue{true});
    SetAttribute(scene, e, "EnableMacCormack", AttributeValue{true});
    SetAttribute(scene, e, "EnableBoussinesq", AttributeValue{false});
    SetAttribute(scene, e, "VolumeVisualizationClip", AttributeValue{false});
    SetAttribute(scene, e, "BoundsMin", AttributeValue{Solstice::Math::Vec3{0.f, 0.f, 0.f}});
    SetAttribute(scene, e, "BoundsMax", AttributeValue{Solstice::Math::Vec3{1.f, 1.f, 1.f}});
    SetAttribute(scene, e, "ResolutionX", AttributeValue{int32_t{32}});
    SetAttribute(scene, e, "ResolutionY", AttributeValue{int32_t{32}});
    SetAttribute(scene, e, "ResolutionZ", AttributeValue{int32_t{32}});
    SetAttribute(scene, e, "Diffusion", AttributeValue{0.0001f});
    SetAttribute(scene, e, "Viscosity", AttributeValue{0.0001f});
    SetAttribute(scene, e, "ReferenceDensity", AttributeValue{1000.f});
    SetAttribute(scene, e, "PressureRelaxationIterations", AttributeValue{int32_t{32}});
    SetAttribute(scene, e, "BuoyancyStrength", AttributeValue{1.f});
    SetAttribute(scene, e, "Prandtl", AttributeValue{0.71f});
}

void ApplyDefaultSoftBodyAttributes(Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::ElementIndex e) {
    using namespace Solstice::Parallax;
    SetAttribute(scene, e, "Enabled", AttributeValue{true});
    SetAttribute(scene, e, "AnchorTopRow", AttributeValue{true});
    SetAttribute(scene, e, "Origin", AttributeValue{Solstice::Math::Vec3{0.f, 4.f, 0.f}});
    SetAttribute(scene, e, "GridWidth", AttributeValue{int32_t{12}});
    SetAttribute(scene, e, "GridHeight", AttributeValue{int32_t{12}});
    SetAttribute(scene, e, "NodeSpacing", AttributeValue{0.25f});
    SetAttribute(scene, e, "NodeMass", AttributeValue{1.0f});
    SetAttribute(scene, e, "Damping", AttributeValue{0.05f});
    SetAttribute(scene, e, "StructuralStiffness", AttributeValue{0.95f});
    SetAttribute(scene, e, "ShearStiffness", AttributeValue{0.85f});
    SetAttribute(scene, e, "BendStiffness", AttributeValue{0.60f});
    SetAttribute(scene, e, "SolverIterations", AttributeValue{int32_t{8}});
}

void ApplyDefaultVehicleAttributes(Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::ElementIndex e) {
    using namespace Solstice::Parallax;
    SetAttribute(scene, e, "Enabled", AttributeValue{true});
    SetAttribute(scene, e, "Origin", AttributeValue{Solstice::Math::Vec3{0.f, 1.f, 0.f}});
    SetAttribute(scene, e, "WheelBase", AttributeValue{2.5f});
    SetAttribute(scene, e, "TrackWidth", AttributeValue{1.6f});
    SetAttribute(scene, e, "Mass", AttributeValue{1200.0f});
    SetAttribute(scene, e, "EngineForce", AttributeValue{9000.0f});
    SetAttribute(scene, e, "BrakeForce", AttributeValue{6000.0f});
    SetAttribute(scene, e, "MaxSteerAngleRadians", AttributeValue{0.55f});
}

} // namespace

void DrawFluidVolumesPanel(const char* windowId, bool* pOpen, Solstice::Parallax::ParallaxScene& scene, uint64_t timeTicks,
    int& selectedFluidElementIndex, bool compressPrlx, bool& sceneDirty) {
    if (!pOpen || !*pOpen) {
        return;
    }
    if (LibUI::Widgets::BeginWindow(windowId, pOpen)) {
        std::vector<Solstice::Parallax::ElementIndex> fluidIdx;
        CollectFluidVolumeElementIndices(scene, fluidIdx);
        if (selectedFluidElementIndex >= 0
            && static_cast<size_t>(selectedFluidElementIndex) >= fluidIdx.size()) {
            selectedFluidElementIndex = fluidIdx.empty() ? -1 : static_cast<int>(fluidIdx.size()) - 1;
        }
        if (LibUI::Widgets::Button("Add fluid volume")) {
            Smm::PushSceneUndoSnapshot(scene, compressPrlx);
            const std::string nm = MakeUniqueFluidElementName(scene, "FluidVolume");
            const Solstice::Parallax::ElementIndex ne =
                Solstice::Parallax::AddElement(scene, "SmmFluidVolumeElement", nm, 0);
            if (ne != Solstice::Parallax::PARALLAX_INVALID_INDEX) {
                ApplyDefaultFluidAttributes(scene, ne);
                CollectFluidVolumeElementIndices(scene, fluidIdx);
                for (size_t i = 0; i < fluidIdx.size(); ++i) {
                    if (fluidIdx[i] == ne) {
                        selectedFluidElementIndex = static_cast<int>(i);
                        break;
                    }
                }
                sceneDirty = true;
            }
        }
        LibUI::Widgets::SameLine();
        if (LibUI::Widgets::Button("Duplicate selected") && selectedFluidElementIndex >= 0
            && static_cast<size_t>(selectedFluidElementIndex) < fluidIdx.size()) {
            Smm::PushSceneUndoSnapshot(scene, compressPrlx);
            const Solstice::Parallax::ElementIndex src = fluidIdx[static_cast<size_t>(selectedFluidElementIndex)];
            const auto& srcNode = scene.GetElements()[src];
            const std::string nm = MakeUniqueFluidElementName(scene, "FluidVolume");
            const Solstice::Parallax::ElementIndex ne =
                Solstice::Parallax::AddElement(scene, "SmmFluidVolumeElement", nm, srcNode.Parent);
            if (ne != Solstice::Parallax::PARALLAX_INVALID_INDEX) {
                ApplyDefaultFluidAttributes(scene, ne);
                // Copy attributes from source
                const char* keys[] = {"Enabled", "EnableMacCormack", "EnableBoussinesq", "VolumeVisualizationClip", "BoundsMin",
                    "BoundsMax", "ResolutionX", "ResolutionY", "ResolutionZ", "Diffusion", "Viscosity", "ReferenceDensity",
                    "PressureRelaxationIterations", "BuoyancyStrength", "Prandtl"};
                for (const char* k : keys) {
                    Solstice::Parallax::SetAttribute(scene, ne, k,
                        Solstice::Parallax::GetAttribute(scene, src, k));
                }
                CollectFluidVolumeElementIndices(scene, fluidIdx);
                selectedFluidElementIndex = static_cast<int>(fluidIdx.size()) - 1;
                sceneDirty = true;
            }
        }
        LibUI::Widgets::SameLine();
        if (LibUI::Widgets::Button("Fit bounds to actor/camera origins")) {
            if (selectedFluidElementIndex >= 0 && static_cast<size_t>(selectedFluidElementIndex) < fluidIdx.size()) {
                Solstice::Parallax::SceneEvaluationResult ev{};
                Solstice::Parallax::EvaluateScene(scene, timeTicks, ev);
                if (!ev.ElementTransforms.empty()) {
                    float minX = ev.ElementTransforms[0].Position.x;
                    float minY = ev.ElementTransforms[0].Position.y;
                    float minZ = ev.ElementTransforms[0].Position.z;
                    float maxX = minX;
                    float maxY = minY;
                    float maxZ = minZ;
                    for (const auto& t : ev.ElementTransforms) {
                        minX = (std::min)(minX, t.Position.x);
                        minY = (std::min)(minY, t.Position.y);
                        minZ = (std::min)(minZ, t.Position.z);
                        maxX = (std::max)(maxX, t.Position.x);
                        maxY = (std::max)(maxY, t.Position.y);
                        maxZ = (std::max)(maxZ, t.Position.z);
                    }
                    const float pad = 0.5f;
                    Smm::PushSceneUndoSnapshot(scene, compressPrlx);
                    Solstice::Parallax::ElementIndex e = fluidIdx[static_cast<size_t>(selectedFluidElementIndex)];
                    Solstice::Parallax::SetAttribute(scene, e, "BoundsMin",
                        Solstice::Parallax::AttributeValue{Solstice::Math::Vec3{minX - pad, minY - pad, minZ - pad}});
                    Solstice::Parallax::SetAttribute(scene, e, "BoundsMax",
                        Solstice::Parallax::AttributeValue{Solstice::Math::Vec3{maxX + pad, maxY + pad, maxZ + pad}});
                    sceneDirty = true;
                }
            }
        }

        if (fluidIdx.empty()) {
            LibUI::Widgets::Text("No fluid volumes. Add one to author NS-style volumes (saved in .prlx).");
        } else {
            int sel = (std::max)(0, selectedFluidElementIndex);
            sel = (std::min)(sel, static_cast<int>(fluidIdx.size()) - 1);
            if (LibUI::Widgets::SliderInt("Selected##smmfluid", &sel, 0, static_cast<int>(fluidIdx.size()) - 1)) {
                selectedFluidElementIndex = sel;
            }
            const Solstice::Parallax::ElementIndex e = fluidIdx[static_cast<size_t>(sel)];
            ImGui::LabelText("Element", "%s (index %u)", scene.GetElements()[e].Name.c_str(), static_cast<unsigned>(e));

            auto editBool = [&](const char* label, const char* key) {
                bool v = false;
                const Solstice::Parallax::AttributeValue av = Solstice::Parallax::GetAttribute(scene, e, key);
                if (const auto* b = std::get_if<bool>(&av)) {
                    v = *b;
                }
                if (LibUI::Widgets::Checkbox(label, &v)) {
                    Smm::PushSceneUndoSnapshot(scene, compressPrlx);
                    Solstice::Parallax::SetAttribute(scene, e, key, Solstice::Parallax::AttributeValue{v});
                    sceneDirty = true;
                }
            };
            auto editFloat = [&](const char* label, const char* key) {
                float v = 0.f;
                const Solstice::Parallax::AttributeValue av = Solstice::Parallax::GetAttribute(scene, e, key);
                if (const auto* f = std::get_if<float>(&av)) {
                    v = *f;
                }
                const float speed = (std::max)(std::abs(v) * 0.01f, 1e-6f);
                if (LibUI::Widgets::DragFloat(label, &v, speed, 0.f, 1e9f, "%.6f")) {
                    Smm::PushSceneUndoSnapshot(scene, compressPrlx);
                    Solstice::Parallax::SetAttribute(scene, e, key, Solstice::Parallax::AttributeValue{v});
                    sceneDirty = true;
                }
            };
            auto editInt = [&](const char* label, const char* key, int lo, int hi) {
                int32_t v = 32;
                const Solstice::Parallax::AttributeValue av = Solstice::Parallax::GetAttribute(scene, e, key);
                if (const auto* in = std::get_if<int32_t>(&av)) {
                    v = *in;
                }
                int vi = static_cast<int>(v);
                if (LibUI::Widgets::DragInt(label, &vi, 1, lo, hi)) {
                    Smm::PushSceneUndoSnapshot(scene, compressPrlx);
                    Solstice::Parallax::SetAttribute(
                        scene, e, key, Solstice::Parallax::AttributeValue{static_cast<int32_t>(vi)});
                    sceneDirty = true;
                }
            };
            auto editVec3 = [&](const char* label, const char* key) {
                Solstice::Math::Vec3 v{};
                const Solstice::Parallax::AttributeValue av = Solstice::Parallax::GetAttribute(scene, e, key);
                if (const auto* p = std::get_if<Solstice::Math::Vec3>(&av)) {
                    v = *p;
                }
                float b[3] = {v.x, v.y, v.z};
                if (LibUI::Widgets::DragFloat3(label, b, 0.05f)) {
                    Smm::PushSceneUndoSnapshot(scene, compressPrlx);
                    Solstice::Parallax::SetAttribute(scene, e, key,
                        Solstice::Parallax::AttributeValue{Solstice::Math::Vec3{b[0], b[1], b[2]}});
                    sceneDirty = true;
                }
            };

            editBool("Enabled", "Enabled");
            editBool("MacCormack", "EnableMacCormack");
            editBool("Boussinesq", "EnableBoussinesq");
            editBool("Volume visualization clip", "VolumeVisualizationClip");
            editVec3("Bounds min", "BoundsMin");
            editVec3("Bounds max", "BoundsMax");
            editInt("Nx##flr", "ResolutionX", 4, 128);
            editInt("Ny##flr", "ResolutionY", 4, 128);
            editInt("Nz##flr", "ResolutionZ", 4, 128);
            {
                int32_t nx = 32, ny = 32, nz = 32;
                {
                    const Solstice::Parallax::AttributeValue av = Solstice::Parallax::GetAttribute(scene, e, "ResolutionX");
                    if (const auto* v = std::get_if<int32_t>(&av)) {
                        nx = *v;
                    }
                }
                {
                    const Solstice::Parallax::AttributeValue av = Solstice::Parallax::GetAttribute(scene, e, "ResolutionY");
                    if (const auto* v = std::get_if<int32_t>(&av)) {
                        ny = *v;
                    }
                }
                {
                    const Solstice::Parallax::AttributeValue av = Solstice::Parallax::GetAttribute(scene, e, "ResolutionZ");
                    if (const auto* v = std::get_if<int32_t>(&av)) {
                        nz = *v;
                    }
                }
                const int64_t cells = static_cast<int64_t>(nx) * static_cast<int64_t>(ny) * static_cast<int64_t>(nz);
                if (cells > Solstice::Parallax::kParallaxFluidInteriorCellBudget) {
                    ImGui::TextColored(ImVec4(1.f, 0.45f, 0.35f, 1.f), "Interior cells %lld exceed budget (%lld).",
                        static_cast<long long>(cells), static_cast<long long>(Solstice::Parallax::kParallaxFluidInteriorCellBudget));
                } else {
                    ImGui::Text("Interior cells: %lld (budget %lld)", static_cast<long long>(cells),
                        static_cast<long long>(Solstice::Parallax::kParallaxFluidInteriorCellBudget));
                }
            }
            editFloat("Diffusion", "Diffusion");
            editFloat("Viscosity", "Viscosity");
            editFloat("Reference density", "ReferenceDensity");
            editInt("Pressure iterations", "PressureRelaxationIterations", 1, 256);
            editFloat("Buoyancy strength", "BuoyancyStrength");
            editFloat("Prandtl", "Prandtl");
        }

        LibUI::Widgets::Separator();
        LibUI::Widgets::Text("Soft bodies (PBD cloth)");
        std::vector<Solstice::Parallax::ElementIndex> softIdx;
        CollectSoftBodyElementIndices(scene, softIdx);
        if (LibUI::Widgets::Button("Add soft body")) {
            Smm::PushSceneUndoSnapshot(scene, compressPrlx);
            const std::string nm = MakeUniqueFluidElementName(scene, "SoftBody");
            const Solstice::Parallax::ElementIndex ne = Solstice::Parallax::AddElement(scene, "SmmSoftBodyElement", nm, 0);
            if (ne != Solstice::Parallax::PARALLAX_INVALID_INDEX) {
                ApplyDefaultSoftBodyAttributes(scene, ne);
                sceneDirty = true;
            }
        }
        if (!softIdx.empty()) {
            static int s_softSel = 0;
            s_softSel = std::clamp(s_softSel, 0, static_cast<int>(softIdx.size()) - 1);
            LibUI::Widgets::SliderInt("Selected soft body##smmsoft", &s_softSel, 0, static_cast<int>(softIdx.size()) - 1);
            const Solstice::Parallax::ElementIndex e = softIdx[static_cast<size_t>(s_softSel)];
            ImGui::LabelText("Soft element", "%s (index %u)", scene.GetElements()[e].Name.c_str(), static_cast<unsigned>(e));
            auto editSoftBool = [&](const char* label, const char* key) {
                bool v = false;
                const Solstice::Parallax::AttributeValue av = Solstice::Parallax::GetAttribute(scene, e, key);
                if (const auto* b = std::get_if<bool>(&av)) {
                    v = *b;
                }
                if (LibUI::Widgets::Checkbox(label, &v)) {
                    Smm::PushSceneUndoSnapshot(scene, compressPrlx);
                    Solstice::Parallax::SetAttribute(scene, e, key, Solstice::Parallax::AttributeValue{v});
                    sceneDirty = true;
                }
            };
            auto editSoftInt = [&](const char* label, const char* key, int lo, int hi) {
                int32_t v = 0;
                const Solstice::Parallax::AttributeValue av = Solstice::Parallax::GetAttribute(scene, e, key);
                if (const auto* in = std::get_if<int32_t>(&av)) {
                    v = *in;
                }
                int vi = static_cast<int>(v);
                if (LibUI::Widgets::DragInt(label, &vi, 1, lo, hi)) {
                    Smm::PushSceneUndoSnapshot(scene, compressPrlx);
                    Solstice::Parallax::SetAttribute(scene, e, key, Solstice::Parallax::AttributeValue{static_cast<int32_t>(vi)});
                    sceneDirty = true;
                }
            };
            auto editSoftFloat = [&](const char* label, const char* key, float speed, float lo, float hi) {
                float v = 0.f;
                const Solstice::Parallax::AttributeValue av = Solstice::Parallax::GetAttribute(scene, e, key);
                if (const auto* f = std::get_if<float>(&av)) {
                    v = *f;
                }
                if (LibUI::Widgets::DragFloat(label, &v, speed, lo, hi)) {
                    Smm::PushSceneUndoSnapshot(scene, compressPrlx);
                    Solstice::Parallax::SetAttribute(scene, e, key, Solstice::Parallax::AttributeValue{v});
                    sceneDirty = true;
                }
            };
            auto editSoftVec3 = [&](const char* label, const char* key) {
                Solstice::Math::Vec3 v{};
                const Solstice::Parallax::AttributeValue av = Solstice::Parallax::GetAttribute(scene, e, key);
                if (const auto* p = std::get_if<Solstice::Math::Vec3>(&av)) {
                    v = *p;
                }
                float b[3] = {v.x, v.y, v.z};
                if (LibUI::Widgets::DragFloat3(label, b, 0.05f)) {
                    Smm::PushSceneUndoSnapshot(scene, compressPrlx);
                    Solstice::Parallax::SetAttribute(scene, e, key, Solstice::Parallax::AttributeValue{Solstice::Math::Vec3{b[0], b[1], b[2]}});
                    sceneDirty = true;
                }
            };
            editSoftBool("Enabled##smmsb", "Enabled");
            editSoftBool("Anchor top row##smmsb", "AnchorTopRow");
            editSoftVec3("Origin##smmsb", "Origin");
            editSoftInt("Grid width##smmsb", "GridWidth", 2, 256);
            editSoftInt("Grid height##smmsb", "GridHeight", 2, 256);
            editSoftInt("Solver iterations##smmsb", "SolverIterations", 1, 64);
            editSoftFloat("Node spacing##smmsb", "NodeSpacing", 0.01f, 0.01f, 5.0f);
            editSoftFloat("Node mass##smmsb", "NodeMass", 0.01f, 0.001f, 100.0f);
            editSoftFloat("Damping##smmsb", "Damping", 0.005f, 0.0f, 1.0f);
            editSoftFloat("Structural stiffness##smmsb", "StructuralStiffness", 0.005f, 0.0f, 1.0f);
            editSoftFloat("Shear stiffness##smmsb", "ShearStiffness", 0.005f, 0.0f, 1.0f);
            editSoftFloat("Bend stiffness##smmsb", "BendStiffness", 0.005f, 0.0f, 1.0f);
        }

        LibUI::Widgets::Separator();
        LibUI::Widgets::Text("Vehicles (arcade)");
        std::vector<Solstice::Parallax::ElementIndex> vehicleIdx;
        CollectVehicleElementIndices(scene, vehicleIdx);
        if (LibUI::Widgets::Button("Add vehicle")) {
            Smm::PushSceneUndoSnapshot(scene, compressPrlx);
            const std::string nm = MakeUniqueFluidElementName(scene, "Vehicle");
            const Solstice::Parallax::ElementIndex ne = Solstice::Parallax::AddElement(scene, "SmmVehicleElement", nm, 0);
            if (ne != Solstice::Parallax::PARALLAX_INVALID_INDEX) {
                ApplyDefaultVehicleAttributes(scene, ne);
                sceneDirty = true;
            }
        }
        if (!vehicleIdx.empty()) {
            static int s_vehicleSel = 0;
            s_vehicleSel = std::clamp(s_vehicleSel, 0, static_cast<int>(vehicleIdx.size()) - 1);
            LibUI::Widgets::SliderInt("Selected vehicle##smmveh", &s_vehicleSel, 0, static_cast<int>(vehicleIdx.size()) - 1);
            const Solstice::Parallax::ElementIndex e = vehicleIdx[static_cast<size_t>(s_vehicleSel)];
            ImGui::LabelText("Vehicle element", "%s (index %u)", scene.GetElements()[e].Name.c_str(), static_cast<unsigned>(e));
            auto editVehicleBool = [&](const char* label, const char* key) {
                bool v = false;
                const Solstice::Parallax::AttributeValue av = Solstice::Parallax::GetAttribute(scene, e, key);
                if (const auto* b = std::get_if<bool>(&av)) {
                    v = *b;
                }
                if (LibUI::Widgets::Checkbox(label, &v)) {
                    Smm::PushSceneUndoSnapshot(scene, compressPrlx);
                    Solstice::Parallax::SetAttribute(scene, e, key, Solstice::Parallax::AttributeValue{v});
                    sceneDirty = true;
                }
            };
            auto editVehicleFloat = [&](const char* label, const char* key, float speed, float lo, float hi) {
                float v = 0.f;
                const Solstice::Parallax::AttributeValue av = Solstice::Parallax::GetAttribute(scene, e, key);
                if (const auto* f = std::get_if<float>(&av)) {
                    v = *f;
                }
                if (LibUI::Widgets::DragFloat(label, &v, speed, lo, hi)) {
                    Smm::PushSceneUndoSnapshot(scene, compressPrlx);
                    Solstice::Parallax::SetAttribute(scene, e, key, Solstice::Parallax::AttributeValue{v});
                    sceneDirty = true;
                }
            };
            auto editVehicleVec3 = [&](const char* label, const char* key) {
                Solstice::Math::Vec3 v{};
                const Solstice::Parallax::AttributeValue av = Solstice::Parallax::GetAttribute(scene, e, key);
                if (const auto* p = std::get_if<Solstice::Math::Vec3>(&av)) {
                    v = *p;
                }
                float b[3] = {v.x, v.y, v.z};
                if (LibUI::Widgets::DragFloat3(label, b, 0.05f)) {
                    Smm::PushSceneUndoSnapshot(scene, compressPrlx);
                    Solstice::Parallax::SetAttribute(scene, e, key, Solstice::Parallax::AttributeValue{Solstice::Math::Vec3{b[0], b[1], b[2]}});
                    sceneDirty = true;
                }
            };
            editVehicleBool("Enabled##smmveh", "Enabled");
            editVehicleVec3("Origin##smmveh", "Origin");
            editVehicleFloat("Wheel base##smmveh", "WheelBase", 0.01f, 0.5f, 10.0f);
            editVehicleFloat("Track width##smmveh", "TrackWidth", 0.01f, 0.5f, 10.0f);
            editVehicleFloat("Mass##smmveh", "Mass", 1.0f, 1.0f, 100000.0f);
            editVehicleFloat("Engine force##smmveh", "EngineForce", 10.0f, 0.0f, 1000000.0f);
            editVehicleFloat("Brake force##smmveh", "BrakeForce", 10.0f, 0.0f, 1000000.0f);
            editVehicleFloat("Max steer radians##smmveh", "MaxSteerAngleRadians", 0.005f, 0.01f, 1.5f);
        }
    }
    LibUI::Widgets::EndWindow();
}

} // namespace Smm::Editing
