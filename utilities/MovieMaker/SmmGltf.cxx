#include "SmmGltf.hxx"
#include "SmmFileOps.hxx"

#include <Parallax/ParallaxScene.hxx>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>

namespace Smm {

const LibUI::FileDialogs::FileFilter kGltfFilters[1] = {
    {"glTF asset", "gltf;glb"},
};

namespace {
std::mutex g_GltfMutex;
std::optional<std::string> g_PendingGltfImportPath;
std::optional<std::string> g_PendingGltfExportPath;
} // namespace

void QueueGltfImportPath(std::string p) {
    std::lock_guard<std::mutex> lock(g_GltfMutex);
    g_PendingGltfImportPath = std::move(p);
}

void QueueGltfExportPath(std::string p) {
    std::lock_guard<std::mutex> lock(g_GltfMutex);
    g_PendingGltfExportPath = std::move(p);
}

void DrainPendingGltfOps(Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::DevSessionAssetResolver& resolver,
    int elementSelected, std::string& smmLastError, bool compressPrlx, bool& sceneDirty) {
    std::optional<std::string> imp;
    std::optional<std::string> exp;
    {
        std::lock_guard<std::mutex> lock(g_GltfMutex);
        imp = std::move(g_PendingGltfImportPath);
        exp = std::move(g_PendingGltfExportPath);
    }

    if (imp) {
        if (elementSelected < 0 || static_cast<size_t>(elementSelected) >= scene.GetElements().size()) {
            smmLastError = "Select an Actor element before importing a glTF asset.";
        } else {
            const Solstice::Parallax::ElementIndex el = static_cast<Solstice::Parallax::ElementIndex>(elementSelected);
            if (Solstice::Parallax::GetElementSchema(scene, el) != "ActorElement") {
                smmLastError = "Select an Actor element (add Actor under Elements) to assign a glTF mesh.";
            } else if (const auto h = resolver.ImportFile(std::filesystem::path(*imp))) {
                PushSceneUndoSnapshot(scene, compressPrlx);
                Solstice::Parallax::SetAttribute(
                    scene, el, "MeshAsset", Solstice::Parallax::AttributeValue{*h});
                sceneDirty = true;
                smmLastError = "Imported glTF asset to selected Actor: " + *imp;
            } else {
                smmLastError = "glTF import failed: could not read file: " + *imp;
            }
        }
    }

    if (exp) {
        if (elementSelected < 0 || static_cast<size_t>(elementSelected) >= scene.GetElements().size()) {
            smmLastError = "Select an Actor element before exporting a glTF asset.";
        } else {
            const Solstice::Parallax::ElementIndex el = static_cast<Solstice::Parallax::ElementIndex>(elementSelected);
            if (Solstice::Parallax::GetElementSchema(scene, el) != "ActorElement") {
                smmLastError = "Select an Actor element to export its MeshAsset from the session.";
            } else {
                const Solstice::Parallax::AttributeValue av = Solstice::Parallax::GetAttribute(scene, el, "MeshAsset");
                const uint64_t* ph = std::get_if<uint64_t>(&av);
                if (!ph || *ph == 0) {
                    smmLastError = "Selected Actor has no MeshAsset in the scene.";
                } else {
                    Solstice::Parallax::AssetData ad{};
                    if (!resolver.Resolve(*ph, ad) || ad.Bytes.empty()) {
                        smmLastError = "MeshAsset is not in the current session (import glTF or reload assets).";
                    } else {
                        const std::filesystem::path dstPath(*exp);
                        std::error_code ec;
                        if (!dstPath.parent_path().empty()) {
                            std::filesystem::create_directories(dstPath.parent_path(), ec);
                        }
                        std::ofstream out(dstPath, std::ios::binary | std::ios::trunc);
                        if (!out) {
                            smmLastError = "Failed to open export path: " + *exp;
                        } else {
                            out.write(
                                reinterpret_cast<const char*>(ad.Bytes.data()), static_cast<std::streamsize>(ad.Bytes.size()));
                            if (!out) {
                                smmLastError = "Failed to write glTF export.";
                            } else {
                                smmLastError = "Exported selected Actor glTF asset: " + *exp;
                            }
                        }
                    }
                }
            }
        }
    }
}

} // namespace Smm
