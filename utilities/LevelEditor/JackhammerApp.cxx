// Jackhammer — Solstice Level Editor: .smf (Solstice Map Format v1) authoring.

#include "JackhammerBspTextureOps.hxx"
#include "JackhammerLightmapBake.hxx"
#include "JackhammerPrefabs.hxx"
#include "JackhammerViewportGeoTools.hxx"

#include "LibUI/Core/Core.hxx"
#include "LibUI/Tools/OpenGlDebugBase.hxx"
#include "LibUI/Tools/Win32ExceptionDiag.hxx"
#include "LibUI/FileDialogs/FileDialogs.hxx"
#include "LibUI/Undo/SnapshotStack.hxx"
#include "LibUI/Viewport/Viewport.hxx"
#include "LibUI/Viewport/ViewportGizmo.hxx"
#include "LibUI/Viewport/ViewportMath.hxx"
#include "LibUI/Tools/RecentPathsUi.hxx"
#include "LibUI/Tools/ClipboardButton.hxx"
#include "LibUI/Tools/UnsavedChangesModal.hxx"
#include "LibUI/Tools/AppAbout.hxx"
#include "UtilityPluginHost/UtilityPluginUi.hxx"
#include "EditorEnginePreview/EditorEnginePreview.hxx"
#include "LibUI/Graphics/PreviewTexture.hxx"
#include "LibUI/Icons/Icons.hxx"
#include "LibUI/Shell/Frame.hxx"
#include "LibUI/Shell/DropFile.hxx"
#include "LibUI/Shell/GlWindow.hxx"
#include "LibUI/Shell/MainHost.hxx"
#include "LibUI/Tools/FileChangedOnDiskModal.hxx"
#include "LibVersion.hxx"

#include <Math/Vector.hxx>
#include <Physics/Lighting/LightSource.hxx>

#include <imgui.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <Smf/SmfBinary.hxx>
#include <Smf/SmfMapEditor.hxx>
#include <Smf/SmfValidate.hxx>
#include <Smf/SmfSpatial.hxx>
#include <Smf/SmfUtil.hxx>

#include "JackhammerParticles.hxx"
#include "JackhammerSpatial.hxx"
#include "JackhammerTexturePaint.hxx"
#include "JackhammerMeshOps.hxx"
#include "JackhammerArzachelProps.hxx"
#include "JackhammerViewportDraw.hxx"
#include <Arzachel/MapSerializer.hxx>
#include <Solstice/EditorAudio/EditorRecovery.hxx>
#include <Solstice/FileWatch/FileWatcher.hxx>
#include <Solstice/SettingsStore/SettingsStore.hxx>
#include <Solstice/EditorAudio/EditorAudio.hxx>

#include "UtilityPluginAbi.hxx"
#include "UtilityPluginHost.hxx"
#include <SolsticeAPI/V1/Common.h>
#include <SolsticeAPI/V1/Smf.h>

#include <Core/Relic/PathTable.hxx>
#include <Core/Relic/Reader.hxx>
#include <Core/Relic/Types.hxx>
#include <Core/Relic/Unpack.hxx>
#include <Core/Relic/Writer.hxx>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <fstream>
#include <filesystem>
#include <iostream>
#include "LibUI/Tools/RgbaImageFile.hxx"

#include <map>
#include <mutex>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include <chrono>
#include <new>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

constexpr uint32_t kJhMaxRasterEdgePx = 4096;
constexpr uint64_t kJhMaxRasterPixels = 16ull * 1024ull * 1024ull;
constexpr size_t kJhMaxEnginePreviewEntities = 768;
constexpr size_t kJhMaxLightOverlayDraw = 512;
constexpr size_t kJhMaxEntityOriginMarkers = 4096;
constexpr int kJhEntityListMaxDisplayedRows = 16384;
constexpr size_t kJhEntityListMaxScanEntities = 100000;
constexpr size_t kJhMaxTextureTintCacheEntries = 512;

static Jackhammer::MeshOps::JhTriangleMesh s_jhMeshWorkshop;
static char s_jhMeshWorkshopLine[288] = "";
static std::vector<std::pair<Solstice::Smf::SmfVec3, Solstice::Smf::SmfVec3>> s_jhBspCsgPieces;
// 0=none, 1=block drag (LMB in viewport; use RMB to orbit)
static int s_jhGeoTool = 0;
static bool s_jhBlockDragActive = false;
static float s_jhBlockDragX0 = 0.f;
static float s_jhBlockDragZ0 = 0.f;
static float s_jhBlockDragX1 = 0.f;
static float s_jhBlockDragZ1 = 0.f;
static float s_jhBlockBaseY = 0.f;
static float s_jhBlockHeight = 2.f;
// Primitive build → mesh workshop
static float s_jhPrimCylR = 1.f;
static float s_jhPrimCylH = 2.f;
static int s_jhPrimCylRad = 16;
static int s_jhPrimCylRing = 1;
static float s_jhPrimSphR = 1.f;
static int s_jhPrimSphLa = 12;
static int s_jhPrimSphLo = 24;
static float s_jhPrimTorMajor = 1.f;
static float s_jhPrimTorMinor = 0.3f;
static int s_jhPrimTorMaj = 24;
static int s_jhPrimTorMin = 12;
static float s_jhPrimArchW = 4.f;
static float s_jhPrimArchH = 2.f;
static float s_jhPrimArchD = 0.5f;
static int s_jhPrimArchSeg = 12;
static float s_jhPrimArchCurve = 90.f;
static Solstice::Smf::SmfVec3 s_jhPrimAabbMin{0.f, 0.f, 0.f};
static Solstice::Smf::SmfVec3 s_jhPrimAabbMax{1.f, 1.f, 1.f};
// Mesh workshop sub-object
static int s_jhMwSubMode = 0; // 0=vertex 1=edge 2=face
static int s_jhMwVIndex = 0;
static int s_jhMwE0 = 0;
static int s_jhMwE1 = 1;
static int s_jhMwFaceTri = 0;
static Solstice::Smf::SmfVec3 s_jhMwVPos{0.f, 0.f, 0.f};
static int s_jhMwVLast = -1;
// Edit → Add Arzachel prop: uniform scale and isosphere subdivision (other shapes reuse Parametric * sliders).
static float s_jhArzUniformScale = 1.f;
static int s_jhArzIsoSubdiv = 2;
static LibUI::Viewport::Mat4Col s_jhOrbitViewLast{};
static bool s_jhOrbitViewValid = false;
static Jackhammer::ViewportGeo::MeasureState s_jhMeasure{};
static float s_jhTerrainBrushR = 2.f;
static float s_jhTerrainRaise = 0.04f; // per-frame while LMB down in terrain tool
static float s_jhMwVertSnap = 0.f;     // 0 = off; grid snap for workshop vertex apply + Snap all
static int s_jhHfCellX = 8;
static int s_jhHfCellZ = 8;
static float s_jhHfSizeX = 32.f;
static float s_jhHfSizeZ = 32.f;
static float s_jhHfBaseY = 0.f;
static float s_jhHfOriginX = 0.f;
static float s_jhHfOriginZ = 0.f;
static int s_jhDispPower = 3;
static float s_jhLmAmb[3] = {0.04f, 0.04f, 0.04f};
static int s_jhLmFaceSide = 0;
static bool s_jhLmModelSpot = true;

static void ClampJhEngineFramebuffer(int& w, int& h) {
    w = std::max(2, std::min(w, static_cast<int>(kJhMaxRasterEdgePx)));
    h = std::max(2, std::min(h, static_cast<int>(kJhMaxRasterEdgePx)));
    uint64_t area = static_cast<uint64_t>(w) * static_cast<uint64_t>(h);
    if (area <= kJhMaxRasterPixels) {
        return;
    }
    // Shrink both axes together so aspect matches the viewport (independent trims caused letterboxing + grid mismatch).
    while (area > kJhMaxRasterPixels && w > 32 && h > 32) {
        w = std::max(32, (w * 127) / 128);
        h = std::max(32, (h * 127) / 128);
        area = static_cast<uint64_t>(w) * static_cast<uint64_t>(h);
    }
}

using Solstice::Smf::SmfAttributeType;
using Solstice::Smf::SmfDefaultValueForType;
using Solstice::Smf::SmfAttributeTypeLabel;
using Solstice::Smf::SmfEntity;
using Solstice::Smf::SmfErrorMessage;
using Solstice::Smf::SmfFileHeader;
using Solstice::Smf::SmfMakeEntity;
using Solstice::Smf::SmfMap;
using Solstice::Smf::SmfProperty;
using Solstice::Smf::SmfQuaternion;
using Solstice::Smf::SmfVec2;
using Solstice::Smf::SmfVec3;
using Solstice::Smf::SmfVec4;
using Solstice::Smf::SmfMatrix4;
using Solstice::Smf::SmfValue;
using Solstice::Smf::SmfAuthoringBsp;
using Solstice::Smf::SmfAuthoringBspNode;
using Solstice::Smf::SmfAuthoringOctree;
using Solstice::Smf::SmfAuthoringOctreeNode;
using Solstice::Smf::SmfAcousticZone;
using Solstice::Smf::SmfAuthoringLight;
using Solstice::Smf::SmfAuthoringLightType;
using Solstice::Smf::SmfFluidVolume;
using Solstice::Smf::FindEntityIndex;

static void SyncSmfGameplayToEngine(const SmfMap& map) noexcept {
    try {
        Solstice::Arzachel::MapSerializer::ApplyGameplayFromSmfMap(map);
    } catch (const std::bad_alloc&) {
    } catch (const std::exception&) {
    } catch (...) {
    }
}

/// Picks a unique entity name among existing `map.Entities` (for duplicate / paste).
static std::string MakeUniqueEntityName(const SmfMap& map, const std::string& preferred) {
    std::string base = preferred.empty() ? std::string("entity") : preferred;
    if (!FindEntityIndex(map, base)) {
        return base;
    }
    for (int i = 2; i < 100000; ++i) {
        std::string c = base + "_" + std::to_string(i);
        if (!FindEntityIndex(map, c)) {
            return c;
        }
    }
    return base + "_dup";
}

static std::string MakeUniqueAcousticZoneName(const SmfMap& map, const std::string& preferred) {
    const std::string base = preferred.empty() ? std::string("reverb_zone") : preferred;
    auto used = [&](const std::string& n) {
        for (const auto& z : map.AcousticZones) {
            if (z.Name == n) {
                return true;
            }
        }
        return false;
    };
    if (!used(base)) {
        return base;
    }
    for (int i = 2; i < 100000; ++i) {
        const std::string c = base + "_" + std::to_string(i);
        if (!used(c)) {
            return c;
        }
    }
    return base + "_dup";
}

static std::string MakeUniqueAuthoringLightName(const SmfMap& map, const std::string& preferred) {
    const std::string base = preferred.empty() ? std::string("light") : preferred;
    auto used = [&](const std::string& n) {
        for (const auto& L : map.AuthoringLights) {
            if (L.Name == n) {
                return true;
            }
        }
        return false;
    };
    if (!used(base)) {
        return base;
    }
    for (int i = 2; i < 100000; ++i) {
        const std::string c = base + "_" + std::to_string(i);
        if (!used(c)) {
            return c;
        }
    }
    return base + "_dup";
}

static std::string MakeUniqueFluidVolumeName(const SmfMap& map, const std::string& preferred) {
    const std::string base = preferred.empty() ? std::string("fluid_volume") : preferred;
    auto used = [&](const std::string& n) {
        for (const auto& f : map.FluidVolumes) {
            if (f.Name == n) {
                return true;
            }
        }
        return false;
    };
    if (!used(base)) {
        return base;
    }
    for (int i = 2; i < 100000; ++i) {
        const std::string c = base + "_" + std::to_string(i);
        if (!used(c)) {
            return c;
        }
    }
    return base + "_dup";
}

static const char* SmfAuthoringLightTypeLabel(SmfAuthoringLightType t) {
    switch (t) {
    case SmfAuthoringLightType::Point:
        return "Point";
    case SmfAuthoringLightType::Spot:
        return "Spot";
    case SmfAuthoringLightType::Directional:
        return "Directional";
    default:
        return "Point";
    }
}

static const LibUI::FileDialogs::FileFilter kJhHookPathBrowseFilters[] = {
    {"Any", "*"},
    {"Moonwalk", "*.mw"},
    {"JSON", "*.json"},
    {nullptr, nullptr},
};

static const LibUI::FileDialogs::FileFilter kJhImageFileFilters[] = {
    {"Image", "png;jpg;jpeg;bmp;tga;webp;gif;hdr;pic;pnm;pgm;ppm"},
    {"All files", "*"},
};

static const char* kDiffuseTextureKeys[] = {"diffuseTexture", "albedoTexture", "texture"};
static const char* kMaterialPathKeys[] = {"materialPath", "smatPath"};
static const char* kNormalTextureKeys[] = {"normalTexture", "normalMap"};
static const char* kRoughnessTextureKeys[] = {"roughnessTexture", "roughnessMap"};
static const char* kModelAssetKeys[] = {"modelPath", "meshPath", "gltfPath"};

static const char* TryGetEntityDiffuseTexturePath(const SmfEntity& ent) {
    for (const char* key : kDiffuseTextureKeys) {
        for (const auto& pr : ent.Properties) {
            if (pr.Key == key && pr.Type == SmfAttributeType::String) {
                if (const auto* s = std::get_if<std::string>(&pr.Value)) {
                    if (!s->empty()) {
                        return s->c_str();
                    }
                }
            }
        }
    }
    return nullptr;
}

static void SetEntityDiffuseTexturePath(SmfEntity& ent, const std::string& pathUtf8) {
    if (pathUtf8.empty()) {
        for (const char* key : kDiffuseTextureKeys) {
            ent.Properties.erase(std::remove_if(ent.Properties.begin(), ent.Properties.end(),
                                     [key](const SmfProperty& p) { return p.Key == key && p.Type == SmfAttributeType::String; }),
                ent.Properties.end());
        }
        return;
    }
    for (const char* key : kDiffuseTextureKeys) {
        for (auto& pr : ent.Properties) {
            if (pr.Key == key && pr.Type == SmfAttributeType::String) {
                pr.Value = pathUtf8;
                return;
            }
        }
    }
    ent.Properties.push_back({"diffuseTexture", SmfAttributeType::String, pathUtf8});
}

static const char* TryGetEntityMaterialPath(const SmfEntity& ent) {
    for (const char* key : kMaterialPathKeys) {
        for (const auto& pr : ent.Properties) {
            if (pr.Key == key && pr.Type == SmfAttributeType::String) {
                if (const auto* s = std::get_if<std::string>(&pr.Value)) {
                    if (!s->empty()) {
                        return s->c_str();
                    }
                }
            }
        }
    }
    return nullptr;
}

static void SetEntityMaterialPath(SmfEntity& ent, const std::string& pathUtf8) {
    if (pathUtf8.empty()) {
        for (const char* key : kMaterialPathKeys) {
            ent.Properties.erase(std::remove_if(ent.Properties.begin(), ent.Properties.end(),
                                     [key](const SmfProperty& p) { return p.Key == key && p.Type == SmfAttributeType::String; }),
                ent.Properties.end());
        }
        return;
    }
    for (const char* key : kMaterialPathKeys) {
        for (auto& pr : ent.Properties) {
            if (pr.Key == key && pr.Type == SmfAttributeType::String) {
                pr.Value = pathUtf8;
                return;
            }
        }
    }
    ent.Properties.push_back({"materialPath", SmfAttributeType::String, pathUtf8});
}

static const char* TryGetEntityNormalTexturePath(const SmfEntity& ent) {
    for (const char* key : kNormalTextureKeys) {
        for (const auto& pr : ent.Properties) {
            if (pr.Key == key && pr.Type == SmfAttributeType::String) {
                if (const auto* s = std::get_if<std::string>(&pr.Value)) {
                    if (!s->empty()) {
                        return s->c_str();
                    }
                }
            }
        }
    }
    return nullptr;
}

static void SetEntityNormalTexturePath(SmfEntity& ent, const std::string& pathUtf8) {
    if (pathUtf8.empty()) {
        for (const char* key : kNormalTextureKeys) {
            ent.Properties.erase(std::remove_if(ent.Properties.begin(), ent.Properties.end(),
                                     [key](const SmfProperty& p) { return p.Key == key && p.Type == SmfAttributeType::String; }),
                ent.Properties.end());
        }
        return;
    }
    for (const char* key : kNormalTextureKeys) {
        for (auto& pr : ent.Properties) {
            if (pr.Key == key && pr.Type == SmfAttributeType::String) {
                pr.Value = pathUtf8;
                return;
            }
        }
    }
    ent.Properties.push_back({"normalTexture", SmfAttributeType::String, pathUtf8});
}

static const char* TryGetEntityRoughnessTexturePath(const SmfEntity& ent) {
    for (const char* key : kRoughnessTextureKeys) {
        for (const auto& pr : ent.Properties) {
            if (pr.Key == key && pr.Type == SmfAttributeType::String) {
                if (const auto* s = std::get_if<std::string>(&pr.Value)) {
                    if (!s->empty()) {
                        return s->c_str();
                    }
                }
            }
        }
    }
    return nullptr;
}

static void SetEntityRoughnessTexturePath(SmfEntity& ent, const std::string& pathUtf8) {
    if (pathUtf8.empty()) {
        for (const char* key : kRoughnessTextureKeys) {
            ent.Properties.erase(std::remove_if(ent.Properties.begin(), ent.Properties.end(),
                                     [key](const SmfProperty& p) { return p.Key == key && p.Type == SmfAttributeType::String; }),
                ent.Properties.end());
        }
        return;
    }
    for (const char* key : kRoughnessTextureKeys) {
        for (auto& pr : ent.Properties) {
            if (pr.Key == key && pr.Type == SmfAttributeType::String) {
                pr.Value = pathUtf8;
                return;
            }
        }
    }
    ent.Properties.push_back({"roughnessTexture", SmfAttributeType::String, pathUtf8});
}

static const char* TryGetEntityModelAssetPath(const SmfEntity& ent) {
    for (const char* key : kModelAssetKeys) {
        for (const auto& pr : ent.Properties) {
            if (pr.Key == key && pr.Type == SmfAttributeType::String) {
                if (const auto* s = std::get_if<std::string>(&pr.Value)) {
                    if (!s->empty()) {
                        return s->c_str();
                    }
                }
            }
        }
    }
    return nullptr;
}

static void SetEntityModelAssetPath(SmfEntity& ent, const std::string& pathUtf8) {
    if (pathUtf8.empty()) {
        for (const char* key : kModelAssetKeys) {
            ent.Properties.erase(std::remove_if(ent.Properties.begin(), ent.Properties.end(),
                                     [key](const SmfProperty& p) { return p.Key == key && p.Type == SmfAttributeType::String; }),
                ent.Properties.end());
        }
        return;
    }
    for (const char* key : kModelAssetKeys) {
        for (auto& pr : ent.Properties) {
            if (pr.Key == key && pr.Type == SmfAttributeType::String) {
                pr.Value = pathUtf8;
                return;
            }
        }
    }
    ent.Properties.push_back({"modelPath", SmfAttributeType::String, pathUtf8});
}

static std::string ToMapRelativePathIfPossible(const std::string& assetPathUtf8, const std::optional<std::string>& currentMapPath) {
    if (!currentMapPath || currentMapPath->empty()) {
        return assetPathUtf8;
    }
    std::error_code ec;
    const std::filesystem::path mapDir = std::filesystem::path(*currentMapPath).parent_path();
    const std::filesystem::path absAsset = std::filesystem::absolute(std::filesystem::path(assetPathUtf8), ec);
    if (ec || absAsset.empty() || mapDir.empty()) {
        return assetPathUtf8;
    }
    const std::filesystem::path rel = std::filesystem::relative(absAsset, mapDir, ec);
    if (ec || rel.empty()) {
        return assetPathUtf8;
    }
    return rel.generic_string();
}

/// Resolve a map-relative or absolute UTF-8 path for engine preview I/O (`.smat`, raster maps).
static std::string JhResolveMapAssetPath(const std::optional<std::string>& currentMapPath, const std::string& pathUtf8) {
    if (pathUtf8.empty()) {
        return {};
    }
    std::error_code ec;
    const std::filesystem::path p(pathUtf8);
    if (p.is_absolute()) {
        return p.lexically_normal().generic_string();
    }
    if (!currentMapPath || currentMapPath->empty()) {
        return pathUtf8;
    }
    const std::filesystem::path mapDir = std::filesystem::path(*currentMapPath).parent_path();
    if (mapDir.empty()) {
        return pathUtf8;
    }
    return (mapDir / p).lexically_normal().generic_string();
}

struct JhAcousticImportOp {
    std::string SourcePath;
    bool IsMusic{true};
    int ZoneIndex{-1};
};

std::mutex g_FileOpMutex;
std::optional<std::string> g_PendingOpenPath;
std::optional<std::string> g_PendingSavePath;
std::optional<std::string> g_PendingRelicImportPath;
std::optional<std::string> g_PendingRelicExportPath;
std::optional<std::string> g_PendingGltfImportPath;
std::optional<std::string> g_PendingGltfExportPath;
std::optional<JhAcousticImportOp> g_PendingAcousticImport;

constexpr std::size_t kMaxMapUndo = 48;
LibUI::Undo::SnapshotStack<SmfMap> g_mapUndo{kMaxMapUndo};

static void ClampEntitySelection(const SmfMap& map, int& selectedEntity) {
    if (map.Entities.empty()) {
        selectedEntity = -1;
    } else {
        selectedEntity = std::min(selectedEntity, static_cast<int>(map.Entities.size()) - 1);
        if (selectedEntity < 0) {
            selectedEntity = 0;
        }
    }
}

void PushMapUndoSnapshot(const SmfMap& m) {
    try {
        g_mapUndo.PushBeforeChange(m);
    } catch (const std::bad_alloc&) {
    }
}

void UndoMap(SmfMap& map, int& selectedEntity, bool& dirty) {
    if (!g_mapUndo.Undo(map)) {
        return;
    }
    dirty = true;
    ClampEntitySelection(map, selectedEntity);
    SyncSmfGameplayToEngine(map);
}

void RedoMap(SmfMap& map, int& selectedEntity, bool& dirty) {
    if (!g_mapUndo.Redo(map)) {
        return;
    }
    dirty = true;
    ClampEntitySelection(map, selectedEntity);
    SyncSmfGameplayToEngine(map);
}

Solstice::UtilityPluginHost::UtilityPluginHost g_LevelPlugins;
std::vector<std::pair<std::string, std::string>> g_LevelPluginLoadErrors;

void LoadLevelEditorPlugins() {
    g_LevelPlugins.UnloadAll();
    g_LevelPluginLoadErrors.clear();
    try {
        const char* base = SDL_GetBasePath();
        std::filesystem::path dir = base ? std::filesystem::path(base) / "plugins" : std::filesystem::path("plugins");
        Solstice::UtilityPluginHost::PluginAbiSymbols abi{};
        abi.GetName = SOLSTICE_UTILITY_ABI_LEVEL_EDITOR_GETNAME;
        abi.OnLoad = SOLSTICE_UTILITY_ABI_LEVEL_EDITOR_ONLOAD;
        abi.OnUnload = SOLSTICE_UTILITY_ABI_LEVEL_EDITOR_ONUNLOAD;
        g_LevelPlugins.LoadAllFromDirectory(dir.string(), abi, g_LevelPluginLoadErrors);
    } catch (const std::exception& ex) {
        g_LevelPluginLoadErrors.push_back({"plugins/", std::string("Reload failed: ") + ex.what()});
    } catch (...) {
        g_LevelPluginLoadErrors.push_back({"plugins/", "Reload failed: unknown exception."});
    }
}

void LevelEditorPluginsDrawPanel(bool* pOpen) {
    Solstice::UtilityPluginHost::DrawPluginManagerWindow(g_LevelPlugins, pOpen, "Plugins##Jackhammer", "LevelEditor",
        g_LevelPluginLoadErrors, [] { LoadLevelEditorPlugins(); });
}

void QueueOpenPath(std::string p);

static void JackhammerOpenRecentPath(void* /*ctx*/, const char* pathUtf8) {
    if (pathUtf8 && pathUtf8[0]) {
        QueueOpenPath(std::string(pathUtf8));
    }
}

void QueueOpenPath(std::string p) {
    if (p.empty()) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_FileOpMutex);
    g_PendingOpenPath = std::move(p);
}

void QueueSavePath(std::string p) {
    std::lock_guard<std::mutex> lock(g_FileOpMutex);
    g_PendingSavePath = std::move(p);
}

void QueueRelicImportPath(std::string p) {
    std::lock_guard<std::mutex> lock(g_FileOpMutex);
    g_PendingRelicImportPath = std::move(p);
}

void QueueRelicExportPath(std::string p) {
    std::lock_guard<std::mutex> lock(g_FileOpMutex);
    g_PendingRelicExportPath = std::move(p);
}

void QueueGltfImportPath(std::string p) {
    std::lock_guard<std::mutex> lock(g_FileOpMutex);
    g_PendingGltfImportPath = std::move(p);
}

void QueueGltfExportPath(std::string p) {
    std::lock_guard<std::mutex> lock(g_FileOpMutex);
    g_PendingGltfExportPath = std::move(p);
}

void QueueAcousticImportOp(JhAcousticImportOp op) {
    std::lock_guard<std::mutex> lock(g_FileOpMutex);
    g_PendingAcousticImport = std::move(op);
}

static uint64_t JhHashBytesFNV1a(std::span<const std::byte> data) {
    uint64_t h = 14695981039346656037ull;
    for (std::byte b : data) {
        h ^= static_cast<uint8_t>(b);
        h *= 1099511628211ull;
    }
    return h;
}

static void JhEnsurePathTableRow(SmfMap& map, const std::string& relPath, uint64_t hash) {
    for (auto& row : map.PathTable) {
        if (row.first == relPath) {
            row.second = hash;
            return;
        }
    }
    map.PathTable.push_back({relPath, hash});
}

static void DrainPendingAcousticAudioImport(SmfMap& map, const std::optional<std::string>& currentPath, std::string& status,
    bool& dirty) {
    std::optional<JhAcousticImportOp> op;
    {
        std::lock_guard<std::mutex> lock(g_FileOpMutex);
        op = std::move(g_PendingAcousticImport);
    }
    if (!op) {
        return;
    }
    if (!currentPath) {
        status = "Save the map to a .smf first; audio is stored under its directory as assets/audio/.";
        return;
    }
    if (op->ZoneIndex < 0 || op->ZoneIndex >= static_cast<int>(map.AcousticZones.size())) {
        status = "Acoustic import: that zone no longer exists.";
        return;
    }
    std::error_code ec;
    const std::filesystem::path src(op->SourcePath);
    if (!std::filesystem::is_regular_file(src, ec)) {
        status = "Acoustic import: not a file: " + op->SourcePath;
        return;
    }
    std::ifstream inf(op->SourcePath, std::ios::binary);
    if (!inf) {
        status = "Acoustic import: cannot read file.";
        return;
    }
    inf.seekg(0, std::ios::end);
    const auto sz = inf.tellg();
    inf.seekg(0);
    if (sz < 0) {
        status = "Acoustic import: invalid file size.";
        return;
    }
    std::vector<std::byte> bytes(static_cast<size_t>(sz));
    if (sz > 0) {
        inf.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(sz));
    }
    if (!inf) {
        status = "Acoustic import: read failed.";
        return;
    }
    const uint64_t h = JhHashBytesFNV1a(std::span<const std::byte>(bytes.data(), bytes.size()));

    const std::filesystem::path mapDir = std::filesystem::path(*currentPath).parent_path();
    const std::string fileName = src.filename().string();
    const std::filesystem::path destDir = mapDir / "assets" / "audio";
    std::filesystem::create_directories(destDir, ec);
    if (ec) {
        status = "Acoustic import: could not create assets/audio: " + ec.message();
        return;
    }
    const std::filesystem::path dest = destDir / fileName;
    std::filesystem::copy_file(src, dest, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        status = "Acoustic import: copy failed: " + ec.message();
        return;
    }

    std::string rel = std::string("assets/audio/") + fileName;
    for (char& c : rel) {
        if (c == '\\') {
            c = '/';
        }
    }

    PushMapUndoSnapshot(map);
    JhEnsurePathTableRow(map, rel, h);
    SmfAcousticZone& z = map.AcousticZones[static_cast<size_t>(op->ZoneIndex)];
    if (op->IsMusic) {
        z.MusicPath = rel;
    } else {
        z.AmbiencePath = rel;
    }
    dirty = true;
    status = std::string("Imported zone ") + (op->IsMusic ? "music" : "ambience") + ": " + rel;
}

static const LibUI::FileDialogs::FileFilter kSmfFilters[] = {
    {"Solstice Map", "smf"},
    {"All", "*"},
};

static const LibUI::FileDialogs::FileFilter kRelicFilters[] = {
    {"RELIC archive", "relic"},
    {"All", "*"},
};

static const LibUI::FileDialogs::FileFilter kGltfFilters[] = {
    {"glTF asset", "gltf;glb"},
    {"All", "*"},
};

static const LibUI::FileDialogs::FileFilter kAudioImportFilters[] = {
    {"Audio", "wav;ogg;flac;mp3"},
    {"All", "*"},
};

static const LibUI::FileDialogs::FileFilter kSmatFilters[] = {
    {"Solstice material", "smat"},
    {"All", "*"},
};

// Path table hashes are the manifest AssetHash for RELIC export; keep them in sync with engine path resolution.
static bool JackhammerExportPathTableToRelic(const SmfMap& map, const std::filesystem::path& mapBaseDir,
    const std::filesystem::path& destRelic, std::string& errOut) {
    using Solstice::Core::Relic::AssetTypeTag;
    using Solstice::Core::Relic::CompressionType;
    using Solstice::Core::Relic::RelicWriteInput;
    using Solstice::Core::Relic::RelicWriteOptions;
    using Solstice::Core::Relic::WriteRelic;

    std::vector<RelicWriteInput> inputs;
    for (const auto& row : map.PathTable) {
        if (row.first.empty()) {
            continue;
        }
        const std::filesystem::path filePath = std::filesystem::path(mapBaseDir) / row.first;
        std::error_code ec;
        if (!std::filesystem::is_regular_file(filePath, ec)) {
            errOut = std::string("Not a file: ") + filePath.string();
            return false;
        }
        std::ifstream inf(filePath, std::ios::binary);
        if (!inf) {
            errOut = std::string("Cannot read: ") + filePath.string();
            return false;
        }
        inf.seekg(0, std::ios::end);
        const auto sz = inf.tellg();
        inf.seekg(0);
        if (sz < 0 || static_cast<uint64_t>(sz) > 0xFFFFFFFFull) {
            errOut = "File too large for RELIC export.";
            return false;
        }
        std::vector<std::byte> bytes(static_cast<size_t>(sz));
        if (sz > 0) {
            inf.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(sz));
        }
        if (!inf) {
            errOut = std::string("Read failed: ") + filePath.string();
            return false;
        }
        RelicWriteInput in{};
        in.Hash = row.second;
        in.TypeTag = AssetTypeTag::Unknown;
        in.ClusterId = 0;
        in.Compression = CompressionType::LZ4;
        in.Uncompressed = std::move(bytes);
        in.LogicalPath = row.first;
        inputs.push_back(std::move(in));
    }
    if (inputs.empty()) {
        errOut = "Add at least one non-empty path in the path table (relative to the .smf directory).";
        return false;
    }
    RelicWriteOptions opt{};
    if (!WriteRelic(destRelic, std::move(inputs), opt)) {
        errOut = "WriteRelic failed.";
        return false;
    }
    return true;
}

static bool JackhammerImportRelicIntoPathTable(SmfMap& map, const std::filesystem::path& relicPath, std::string& errOut) {
    using Solstice::Core::Relic::OpenRelic;
    using Solstice::Core::Relic::ParsePathTableBlob;

    auto container = OpenRelic(relicPath);
    if (!container) {
        errOut = "OpenRelic failed (missing or invalid .relic).";
        return false;
    }

    std::unordered_set<uint64_t> seen;
    seen.reserve(map.PathTable.size() + 32);
    for (const auto& r : map.PathTable) {
        seen.insert(r.second);
    }

    if (!container->PathTableBlob.empty()) {
        std::vector<std::pair<std::string, Solstice::Core::Relic::AssetHash>> rows;
        if (!ParsePathTableBlob(std::span<const std::byte>(container->PathTableBlob.data(), container->PathTableBlob.size()),
                rows)) {
            errOut = "Invalid RELIC path table blob.";
            return false;
        }
        for (auto& pr : rows) {
            if (!seen.count(pr.second)) {
                map.PathTable.push_back(std::move(pr));
                seen.insert(map.PathTable.back().second);
            }
        }
    }

    for (const auto& e : container->Manifest) {
        if (!seen.count(e.AssetHash)) {
            map.PathTable.push_back({"", e.AssetHash});
            seen.insert(e.AssetHash);
        }
    }

    return true;
}

#ifdef _WIN32
using JackhammerEngineSmfFn = SolsticeV1_ResultCode (*)(const void*, size_t, char*, size_t);
namespace {
SolsticeV1_ResultCode JackhammerCallEngineSmfProc(JackhammerEngineSmfFn fn, const void* bytes, size_t byteSize, char* err,
    size_t errCap) noexcept {
    SolsticeV1_ResultCode r = SolsticeV1_ResultFailure;
    __try {
        r = fn(bytes, byteSize, err, errCap);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (err && errCap > 0) {
            const unsigned long code = GetExceptionCode();
            std::snprintf(err, errCap, "native exception 0x%08lX in engine DLL", code);
        }
        return SolsticeV1_ResultFailure;
    }
    return r;
}
} // namespace

static std::string EngineDllSmfValidateSummary(const std::vector<std::byte>& bytes) {
    HMODULE mod = LoadLibraryA("SolsticeEngine.dll");
    if (!mod) {
        return "Engine DLL not found next to LevelEditor (optional check skipped).";
    }
    auto fn = reinterpret_cast<JackhammerEngineSmfFn>(GetProcAddress(mod, "SolsticeV1_SmfValidateBinary"));
    if (!fn) {
        FreeLibrary(mod);
        return "SolsticeV1_SmfValidateBinary not exported.";
    }
    char err[512];
    err[0] = '\0';
    SolsticeV1_ResultCode r = JackhammerCallEngineSmfProc(fn, bytes.data(), bytes.size(), err, sizeof(err));
    FreeLibrary(mod);
    if (r == SolsticeV1_ResultSuccess) {
        return "Engine SolsticeV1_SmfValidateBinary: OK.";
    }
    return std::string("Engine validate: ") + (err[0] ? err : "failed");
}
#else
static std::string EngineDllSmfValidateSummary(const std::vector<std::byte>&) {
    return "Engine DLL cross-check is Windows-only in this build.";
}
#endif

#ifdef _WIN32
static std::string EngineDllSmfApplyGameplaySummary(const std::vector<std::byte>& bytes) {
    HMODULE mod = LoadLibraryA("SolsticeEngine.dll");
    if (!mod) {
        return "Engine DLL not found next to LevelEditor (optional check skipped).";
    }
    auto fn = reinterpret_cast<JackhammerEngineSmfFn>(GetProcAddress(mod, "SolsticeV1_SmfApplyGameplay"));
    if (!fn) {
        FreeLibrary(mod);
        return "SolsticeV1_SmfApplyGameplay not exported.";
    }
    char err[512];
    err[0] = '\0';
    SolsticeV1_ResultCode r = JackhammerCallEngineSmfProc(fn, bytes.data(), bytes.size(), err, sizeof(err));
    FreeLibrary(mod);
    if (r == SolsticeV1_ResultSuccess) {
        return "Engine SolsticeV1_SmfApplyGameplay: OK.";
    }
    return std::string("Engine apply gameplay: ") + (err[0] ? err : "failed");
}
#else
static std::string EngineDllSmfApplyGameplaySummary(const std::vector<std::byte>&) {
    return "Engine apply gameplay DLL is Windows-only in this build.";
}
#endif

static void ApplyNewMapTemplate(SmfMap& map, std::optional<std::string>& currentPath, SmfFileHeader& lastHeader, int& selectedEntity,
    bool& dirty, std::string& status) {
    PushMapUndoSnapshot(map);
    map.Clear();
    SmfEntity ws = SmfMakeEntity("worldspawn", "WorldSettings");
    ws.Properties.push_back({"gravity", SmfAttributeType::Float, SmfValue{9.81f}});
    map.Entities.push_back(std::move(ws));
    SmfEntity lamp = SmfMakeEntity("light_0", "Light");
    lamp.Properties.push_back({"origin", SmfAttributeType::Vec3, SmfValue{SmfVec3{0.f, 3.5f, 0.f}}});
    lamp.Properties.push_back({"style", SmfAttributeType::String, SmfValue{std::string("point")}});
    map.Entities.push_back(std::move(lamp));
    SmfEntity prop = SmfMakeEntity("prop_0", "Prop");
    prop.Properties.push_back({"origin", SmfAttributeType::Vec3, SmfValue{SmfVec3{0.f, 0.5f, 0.f}}});
    map.Entities.push_back(std::move(prop));
    currentPath.reset();
    lastHeader = {};
    selectedEntity = 2;
    dirty = true;
    status = "New map from template.";
    SyncSmfGameplayToEngine(map);
}

static bool JhSameMapPathUtf8(const std::string& watchPath, const std::optional<std::string>& currentPath) {
    if (!currentPath || currentPath->empty() || watchPath.empty()) {
        return false;
    }
    if (watchPath == *currentPath) {
        return true;
    }
    // Lexical normalization only — avoid std::filesystem::equivalent here (driver/path edge cases on Windows).
    std::error_code ecw, ecc;
    const std::filesystem::path cw = std::filesystem::weakly_canonical(watchPath, ecw);
    const std::filesystem::path cc = std::filesystem::weakly_canonical(*currentPath, ecc);
    if (!ecw && !ecc) {
        return cw == cc;
    }
    return false;
}

void UpdateWindowTitle(SDL_Window* window, const std::optional<std::string>& currentPath, bool dirty) {
    std::string title = std::string("Jackhammer — Solstice Level Editor — ") + Solstice::Utilities::kVersionDisplaySuffix;
    if (currentPath) {
        title += " — ";
        title += *currentPath;
    }
    if (dirty) {
        title += " *";
    }
    SDL_SetWindowTitle(window, title.c_str());
}

void DrainPendingFileOps(SmfMap& map, std::optional<std::string>& currentPath, SmfFileHeader& hdr, std::string& status,
    bool& dirty, int& selectedEntity, bool compressSmf, std::string* errorBanner, std::string* viewportBanner,
    Solstice::FileWatch::FileWatcher* mapWatchResync) {
    std::optional<std::string> openPath;
    std::optional<std::string> savePath;
    {
        std::lock_guard<std::mutex> lock(g_FileOpMutex);
        openPath = std::move(g_PendingOpenPath);
        savePath = std::move(g_PendingSavePath);
    }

    if (openPath) {
        if (openPath->empty()) {
            openPath.reset();
        }
    }

    if (openPath) {
        Solstice::Smf::SmfError err = Solstice::Smf::SmfError::None;
        if (!Solstice::Smf::LoadSmfFromFile(std::filesystem::path(*openPath), map, &hdr, &err)) {
            status = std::string("Open failed: ") + SmfErrorMessage(err);
            if (errorBanner) {
                *errorBanner = status;
            }
        } else {
            if (errorBanner) {
                errorBanner->clear();
            }
            if (viewportBanner) {
                viewportBanner->clear();
            }
            currentPath = *openPath;
            selectedEntity = map.Entities.empty() ? -1 : 0;
            dirty = false;
            status = "Loaded " + *openPath;
            LibUI::Core::RecentPathPush(openPath->c_str());
            g_mapUndo.Clear();
            SyncSmfGameplayToEngine(map);
            if (mapWatchResync) {
                mapWatchResync->ResyncPath(*openPath);
            }
        }
    }

    if (savePath) {
        Solstice::Smf::SmfError err = Solstice::Smf::SmfError::None;
        if (!Solstice::Smf::SaveSmfToFile(std::filesystem::path(*savePath), map, &err, compressSmf)) {
            status = std::string("Save failed: ") + SmfErrorMessage(err);
            if (errorBanner) {
                *errorBanner = status;
            }
        } else {
            if (errorBanner) {
                errorBanner->clear();
            }
            if (viewportBanner) {
                viewportBanner->clear();
            }
            currentPath = *savePath;
            dirty = false;
            LibUI::Core::RecentPathPush(savePath->c_str());
            std::vector<std::byte> bytes;
            if (Solstice::Smf::SaveSmfToBytes(map, bytes, &err, compressSmf) &&
                Solstice::Smf::LoadSmfFromBytes(map, bytes, &hdr, &err)) {
                status = "Saved " + *savePath;
                SyncSmfGameplayToEngine(map);
            } else {
                status = "Saved file but failed to refresh header view.";
            }
            if (mapWatchResync) {
                mapWatchResync->ResyncPath(*savePath);
            }
        }
    }
}

void DrainPendingRelicOps(SmfMap& map, const std::optional<std::string>& currentPath, std::string& status, bool& dirty,
    std::string* errorBanner, std::string* viewportBanner) {
    std::optional<std::string> imp;
    std::optional<std::string> exp;
    {
        std::lock_guard<std::mutex> lock(g_FileOpMutex);
        imp = std::move(g_PendingRelicImportPath);
        exp = std::move(g_PendingRelicExportPath);
    }

    if (imp) {
        PushMapUndoSnapshot(map);
        std::string err;
        if (JackhammerImportRelicIntoPathTable(map, std::filesystem::path(*imp), err)) {
            dirty = true;
            status = "Imported RELIC into path table: " + *imp;
            if (errorBanner) {
                errorBanner->clear();
            }
            if (viewportBanner) {
                viewportBanner->clear();
            }
        } else {
            status = err;
            if (errorBanner) {
                *errorBanner = err;
            }
        }
    }

    if (exp) {
        if (!currentPath) {
            const char* msg = "Save the .smf first so RELIC export can resolve paths beside the map.";
            status = msg;
            if (errorBanner) {
                *errorBanner = msg;
            }
        } else {
            std::string err;
            const std::filesystem::path base = std::filesystem::path(*currentPath).parent_path();
            if (JackhammerExportPathTableToRelic(map, base, std::filesystem::path(*exp), err)) {
                status = "Exported RELIC: " + *exp;
                if (errorBanner) {
                    errorBanner->clear();
                }
                if (viewportBanner) {
                    viewportBanner->clear();
                }
            } else {
                status = err;
                if (errorBanner) {
                    *errorBanner = err;
                }
            }
        }
    }
}

void DrainPendingGltfOps(SmfMap& map, const std::optional<std::string>& currentPath, int selectedEntity, std::string& status,
    bool& dirty, std::string* errorBanner, std::string* viewportBanner) {
    std::optional<std::string> imp;
    std::optional<std::string> exp;
    {
        std::lock_guard<std::mutex> lock(g_FileOpMutex);
        imp = std::move(g_PendingGltfImportPath);
        exp = std::move(g_PendingGltfExportPath);
    }

    if (imp) {
        if (selectedEntity < 0 || selectedEntity >= static_cast<int>(map.Entities.size())) {
            const char* msg = "Select an entity before importing a glTF asset.";
            status = msg;
            if (errorBanner) {
                *errorBanner = msg;
            }
        } else {
            PushMapUndoSnapshot(map);
            SmfEntity& ent = map.Entities[static_cast<size_t>(selectedEntity)];
            ent.ClassName = "Mesh";
            SetEntityModelAssetPath(ent, ToMapRelativePathIfPossible(*imp, currentPath));
            dirty = true;
            status = "Assigned glTF asset to selected entity: " + *imp;
            if (errorBanner) {
                errorBanner->clear();
            }
            if (viewportBanner) {
                viewportBanner->clear();
            }
        }
    }

    if (exp) {
        if (selectedEntity < 0 || selectedEntity >= static_cast<int>(map.Entities.size())) {
            const char* msg = "Select an entity before exporting a glTF asset.";
            status = msg;
            if (errorBanner) {
                *errorBanner = msg;
            }
            return;
        }
        const SmfEntity& ent = map.Entities[static_cast<size_t>(selectedEntity)];
        const char* srcModel = TryGetEntityModelAssetPath(ent);
        if (!srcModel || srcModel[0] == '\0') {
            const char* msg = "Selected entity has no external model path (modelPath / meshPath / gltfPath).";
            status = msg;
            if (errorBanner) {
                *errorBanner = msg;
            }
            return;
        }
        std::error_code ec;
        std::filesystem::path srcPath(srcModel);
        if (currentPath && srcPath.is_relative()) {
            srcPath = std::filesystem::path(*currentPath).parent_path() / srcPath;
        }
        srcPath = std::filesystem::weakly_canonical(srcPath, ec);
        if (ec || !std::filesystem::exists(srcPath)) {
            status = "Export source asset not found: " + std::string(srcModel);
            if (errorBanner) {
                *errorBanner = status;
            }
            return;
        }
        const std::filesystem::path dstPath(*exp);
        std::filesystem::create_directories(dstPath.parent_path(), ec);
        ec.clear();
        std::filesystem::copy_file(srcPath, dstPath, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) {
            status = std::string("Failed to export glTF asset: ") + ec.message();
            if (errorBanner) {
                *errorBanner = status;
            }
            return;
        }
        status = "Exported glTF asset: " + dstPath.string();
        if (errorBanner) {
            errorBanner->clear();
        }
        if (viewportBanner) {
            viewportBanner->clear();
        }
    }
}

void EditSmfPropertyValue(SmfProperty& prop, bool& dirty) {
    ImGui::PushID(static_cast<int>(reinterpret_cast<uintptr_t>(&prop)));

    switch (prop.Type) {
    case SmfAttributeType::Bool: {
        bool b = std::get_if<bool>(&prop.Value) ? *std::get_if<bool>(&prop.Value) : false;
        if (ImGui::Checkbox("##val", &b)) {
            prop.Value = b;
            dirty = true;
        }
        break;
    }
    case SmfAttributeType::Int32: {
        int v = std::get_if<int32_t>(&prop.Value) ? *std::get_if<int32_t>(&prop.Value) : 0;
        if (ImGui::InputInt("##val", &v)) {
            prop.Value = static_cast<int32_t>(v);
            dirty = true;
        }
        break;
    }
    case SmfAttributeType::Int64: {
        long long v = std::get_if<int64_t>(&prop.Value) ? static_cast<long long>(*std::get_if<int64_t>(&prop.Value)) : 0LL;
        if (ImGui::InputScalar("##val", ImGuiDataType_S64, &v)) {
            prop.Value = static_cast<int64_t>(v);
            dirty = true;
        }
        break;
    }
    case SmfAttributeType::Float: {
        float v = std::get_if<float>(&prop.Value) ? *std::get_if<float>(&prop.Value) : 0.f;
        if (ImGui::DragFloat("##val", &v, 0.01f)) {
            prop.Value = v;
            dirty = true;
        }
        break;
    }
    case SmfAttributeType::Double: {
        double v = std::get_if<double>(&prop.Value) ? *std::get_if<double>(&prop.Value) : 0.0;
        if (ImGui::InputDouble("##val", &v)) {
            prop.Value = v;
            dirty = true;
        }
        break;
    }
    case SmfAttributeType::Vec2: {
        SmfVec2 a = std::get_if<SmfVec2>(&prop.Value) ? *std::get_if<SmfVec2>(&prop.Value) : SmfVec2{};
        float f[2] = {a.x, a.y};
        if (ImGui::DragFloat2("##val", f, 0.01f)) {
            prop.Value = SmfVec2{f[0], f[1]};
            dirty = true;
        }
        break;
    }
    case SmfAttributeType::Vec3: {
        SmfVec3 a = std::get_if<SmfVec3>(&prop.Value) ? *std::get_if<SmfVec3>(&prop.Value) : SmfVec3{};
        float f[3] = {a.x, a.y, a.z};
        if (ImGui::DragFloat3("##val", f, 0.01f)) {
            prop.Value = SmfVec3{f[0], f[1], f[2]};
            dirty = true;
        }
        break;
    }
    case SmfAttributeType::Vec4:
    case SmfAttributeType::ColorRGBA: {
        SmfVec4 a = std::get_if<SmfVec4>(&prop.Value) ? *std::get_if<SmfVec4>(&prop.Value) : SmfVec4{};
        float f[4] = {a.x, a.y, a.z, a.w};
        if (ImGui::DragFloat4("##val", f, 0.01f)) {
            prop.Value = SmfVec4{f[0], f[1], f[2], f[3]};
            dirty = true;
        }
        break;
    }
    case SmfAttributeType::Quaternion: {
        SmfQuaternion q = std::get_if<SmfQuaternion>(&prop.Value) ? *std::get_if<SmfQuaternion>(&prop.Value)
                                                                    : SmfQuaternion{};
        float f[4] = {q.x, q.y, q.z, q.w};
        if (ImGui::DragFloat4("##val", f, 0.001f)) {
            prop.Value = SmfQuaternion{f[0], f[1], f[2], f[3]};
            dirty = true;
        }
        break;
    }
    case SmfAttributeType::Matrix4: {
        SmfMatrix4 m = std::get_if<SmfMatrix4>(&prop.Value) ? *std::get_if<SmfMatrix4>(&prop.Value) : SmfMatrix4{};
        bool changed = false;
        if (ImGui::TreeNode("matrix")) {
            for (int r = 0; r < 4; ++r) {
                float row[4] = {m.m[static_cast<size_t>(r)][0], m.m[static_cast<size_t>(r)][1],
                    m.m[static_cast<size_t>(r)][2], m.m[static_cast<size_t>(r)][3]};
                if (ImGui::DragFloat4(("##r" + std::to_string(r)).c_str(), row, 0.01f)) {
                    for (int c = 0; c < 4; ++c) {
                        m.m[static_cast<size_t>(r)][static_cast<size_t>(c)] = row[c];
                    }
                    changed = true;
                }
            }
            ImGui::TreePop();
        }
        if (changed) {
            prop.Value = m;
            dirty = true;
        }
        break;
    }
    case SmfAttributeType::String: {
        std::string s = std::get_if<std::string>(&prop.Value) ? *std::get_if<std::string>(&prop.Value) : std::string{};
        char sbuf[2048];
        std::snprintf(sbuf, sizeof(sbuf), "%s", s.c_str());
        if (ImGui::InputText("##val", sbuf, sizeof(sbuf))) {
            prop.Value = std::string(sbuf);
            dirty = true;
        }
        break;
    }
    case SmfAttributeType::AssetHash:
    case SmfAttributeType::ArzachelSeed: {
        unsigned long long h = std::get_if<uint64_t>(&prop.Value) ? *std::get_if<uint64_t>(&prop.Value) : 0ull;
        if (ImGui::InputScalar("##val", ImGuiDataType_U64, &h, nullptr, nullptr, "%016llX")) {
            prop.Value = h;
            dirty = true;
        }
        break;
    }
    case SmfAttributeType::BlobOpaque:
    case SmfAttributeType::SkeletonPose: {
        const auto* blob = std::get_if<std::vector<std::byte>>(&prop.Value);
        const size_t n = blob ? blob->size() : 0;
        ImGui::Text("%zu bytes (raw)", n);
        if (ImGui::SmallButton("Clear##blob")) {
            prop.Value = std::vector<std::byte>{};
            dirty = true;
        }
        break;
    }
    case SmfAttributeType::ElementRef: {
        uint32_t r = std::get_if<uint32_t>(&prop.Value) ? *std::get_if<uint32_t>(&prop.Value) : 0u;
        int ir = static_cast<int>(r);
        if (ImGui::InputInt("##val", &ir)) {
            prop.Value = static_cast<uint32_t>(std::max(0, ir));
            dirty = true;
        }
        break;
    }
    case SmfAttributeType::EasingType:
    case SmfAttributeType::TransitionBlendMode: {
        int u = std::get_if<uint8_t>(&prop.Value) ? static_cast<int>(*std::get_if<uint8_t>(&prop.Value)) : 0;
        if (ImGui::SliderInt("##val", &u, 0, 255)) {
            prop.Value = static_cast<uint8_t>(u);
            dirty = true;
        }
        break;
    }
    default:
        ImGui::TextUnformatted("(unsupported)");
        break;
    }

    ImGui::PopID();
}

const SmfVec3* TryGetEntityOriginVec3(const SmfEntity& ent) {
    for (const auto& pr : ent.Properties) {
        if ((pr.Key == "origin" || pr.Key == "position") && pr.Type == SmfAttributeType::Vec3) {
            if (auto* v = std::get_if<SmfVec3>(&pr.Value)) {
                return v;
            }
        }
    }
    return nullptr;
}

static bool JhComputeEntityOriginAabb(const SmfMap& map, SmfVec3& outMin, SmfVec3& outMax) {
    bool any = false;
    for (const auto& ent : map.Entities) {
        const SmfVec3* o = TryGetEntityOriginVec3(ent);
        if (!o) {
            continue;
        }
        if (!any) {
            outMin = outMax = *o;
            any = true;
        } else {
            outMin.x = std::min(outMin.x, o->x);
            outMin.y = std::min(outMin.y, o->y);
            outMin.z = std::min(outMin.z, o->z);
            outMax.x = std::max(outMax.x, o->x);
            outMax.y = std::max(outMax.y, o->y);
            outMax.z = std::max(outMax.z, o->z);
        }
    }
    return any;
}

bool SetEntityOriginVec3(SmfEntity& ent, const SmfVec3& pos) {
    for (auto& pr : ent.Properties) {
        if ((pr.Key == "origin" || pr.Key == "position") && pr.Type == SmfAttributeType::Vec3) {
            pr.Value = SmfValue{pos};
            return true;
        }
    }
    ent.Properties.push_back({"origin", SmfAttributeType::Vec3, SmfValue{pos}});
    return true;
}

static const SmfVec3* TryGetEntityVec3ByKey(const SmfEntity& ent, const char* key) {
    for (const auto& pr : ent.Properties) {
        if (pr.Key == key && pr.Type == SmfAttributeType::Vec3) {
            if (auto* v = std::get_if<SmfVec3>(&pr.Value)) {
                return v;
            }
        }
    }
    return nullptr;
}

static float TryGetEntityFloatByKey(const SmfEntity& ent, const char* key, float defaultVal) {
    for (const auto& pr : ent.Properties) {
        if (pr.Key == key && pr.Type == SmfAttributeType::Float) {
            if (auto* f = std::get_if<float>(&pr.Value)) {
                return *f;
            }
        }
    }
    return defaultVal;
}

static void SetEntityVec3ByKey(SmfEntity& ent, const char* key, const SmfVec3& v) {
    for (auto& pr : ent.Properties) {
        if (pr.Key == key && pr.Type == SmfAttributeType::Vec3) {
            pr.Value = SmfValue{v};
            return;
        }
    }
    ent.Properties.push_back({key, SmfAttributeType::Vec3, SmfValue{v}});
}

static void SetEntityFloatByKey(SmfEntity& ent, const char* key, float f) {
    for (auto& pr : ent.Properties) {
        if (pr.Key == key && pr.Type == SmfAttributeType::Float) {
            pr.Value = SmfValue{f};
            return;
        }
    }
    ent.Properties.push_back({key, SmfAttributeType::Float, SmfValue{f}});
}

} // namespace

namespace Jackhammer {

int RunApp(int argc, char** argv) {
    if (!LibUI::Shell::InitUtilitySdlVideo()) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return -1;
    }

    LibUI::Shell::GlWindow gw{};
    const std::string jhWinTitle =
        std::string("Jackhammer — ") + Solstice::Utilities::kVersionDisplaySuffix;
    if (!LibUI::Shell::CreateUtilityGlWindow(gw, jhWinTitle.c_str(), 1280, 720,
            static_cast<SDL_WindowFlags>(SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED), 1)) {
        std::cerr << "Failed to create OpenGL window: " << SDL_GetError() << std::endl;
        LibUI::Shell::DestroyUtilityGlWindow(gw);
        LibUI::Shell::ShutdownUtilitySdlVideo();
        return -1;
    }
    SDL_Window* window = gw.window;
    SDL_GLContext glContext = gw.glContext;

    LoadLevelEditorPlugins();

    // Plugins load native DLLs; ensure our window's GL context is still current before ImGui init.
    if (!LibUI::Shell::UtilityGlWindowMakeCurrent(gw)) {
        std::cerr << "SDL_GL_MakeCurrent failed after plugins: " << SDL_GetError() << std::endl;
        LibUI::Shell::DestroyUtilityGlWindow(gw);
        LibUI::Shell::ShutdownUtilitySdlVideo();
        return -1;
    }
    if (!LibUI::Core::Initialize(window)) {
        std::cerr << "LibUI init failed" << std::endl;
        LibUI::Shell::DestroyUtilityGlWindow(gw);
        LibUI::Shell::ShutdownUtilitySdlVideo();
        return -1;
    }
    (void)Solstice::EditorAudio::Init();
    LibUI::Graphics::PreviewTextureRgba jackhammerEnginePreviewTex{};

    const char* sdlBaseJackhammer = SDL_GetBasePath();
    Solstice::SettingsStore::Store jackhammerSettings(
        Solstice::SettingsStore::PathNextToExecutable(sdlBaseJackhammer, "jackhammer"));
    jackhammerSettings.Load();

    SmfMap map;
    std::optional<std::string> currentPath;
    SmfFileHeader lastHeader{};
    std::string status = "Ready.";
    std::string jhBannerFile;
    std::string jhBannerViewport;
    bool dirty = false;
    int selectedEntity = -1;
    int spatialBspSel = 0;
    int spatialOctSel = 0;
    int acousticZoneSel = -1;
    int authoringLightSel = -1;
    int fluidVolSel = -1;
    std::string lastValidateCodec = "(not run yet)";
    std::string lastValidateEngine = "(not run yet)";
    std::string lastApplyGameplayEngine = "(not run yet)";
    std::vector<std::string> lastValidateStructure;
    char entityNameFilter[256] = "";
    char entityClassFilter[256] = "";
    bool compressSmf = false;
    bool showPluginsPanel = false;
    bool showAboutPanel = false;
    bool showShortcutsPanel = false;
    bool showReloadFromDiskModal = false;

    enum class UnsavedPromptKind { None, QuitApp, NewMap, OpenMap };
    UnsavedPromptKind unsavedPrompt = UnsavedPromptKind::None;
    std::optional<std::string> unsavedOpenPathPending;

    Solstice::FileWatch::FileWatcher mapFileWatch(std::chrono::milliseconds(400));
    std::optional<std::string> lastWatchedMapPath;
    bool showDiskReloadModal = false;
    bool watchMapFile = true;
    if (auto c = jackhammerSettings.GetBool(Solstice::SettingsStore::kKeyCompressSmf)) {
        compressSmf = *c;
    }
    if (auto w = jackhammerSettings.GetBool(Solstice::SettingsStore::kKeyWatchMapFile)) {
        watchMapFile = *w;
    }
    std::string jhSdlBasePathUtf8;
    if (const char* jhB = SDL_GetBasePath()) {
        jhSdlBasePathUtf8 = jhB;
        SDL_free((void*)jhB);
    }
    const std::filesystem::path jhRecoveryDir = Solstice::EditorAudio::FileRecovery::RecoveryDir(
        jhSdlBasePathUtf8.empty() ? nullptr : jhSdlBasePathUtf8.c_str(), "jackhammer");
    bool jhRecoveryModalOpen = !Solstice::EditorAudio::FileRecovery::List(jhRecoveryDir, "smf").empty();
    double jhRecoveryAccumSec = 0.0;
    uint32_t jhRecoveryIntervalSecU32 = 60;
    if (const auto jhRi = jackhammerSettings.GetInt64("jhRecoveryIntervalSec")) {
        jhRecoveryIntervalSecU32 = static_cast<uint32_t>(
            std::clamp(*jhRi, static_cast<std::int64_t>(10), static_cast<std::int64_t>(3600)));
    }
    mapFileWatch.SetCallback([&](const std::string& p) {
        if (!JhSameMapPathUtf8(p, currentPath)) {
            return;
        }
        if (!dirty) {
            Solstice::Smf::SmfError err = Solstice::Smf::SmfError::None;
            if (Solstice::Smf::LoadSmfFromFile(std::filesystem::path(p), map, &lastHeader, &err)) {
                selectedEntity = map.Entities.empty() ? -1 : 0;
                status = "Reloaded map (file changed on disk).";
                jhBannerFile.clear();
                g_mapUndo.Clear();
                SyncSmfGameplayToEngine(map);
            } else {
                status = std::string("Auto-reload failed: ") + SmfErrorMessage(err);
                jhBannerFile = status;
            }
        } else {
            showDiskReloadModal = true;
        }
    });

    if (argc >= 2) {
        Solstice::Smf::SmfError err = Solstice::Smf::SmfError::None;
        std::filesystem::path p(argv[1]);
        if (Solstice::Smf::LoadSmfFromFile(p, map, &lastHeader, &err)) {
            currentPath = p.string();
            selectedEntity = map.Entities.empty() ? -1 : 0;
            status = "Loaded " + currentPath.value();
            jhBannerFile.clear();
            LibUI::Core::RecentPathPush(p.string().c_str());
            g_mapUndo.Clear();
            SyncSmfGameplayToEngine(map);
        } else {
            status = std::string("Failed to open argument: ") + SmfErrorMessage(err);
            jhBannerFile = status;
        }
    }

    auto doNewCommit = [&]() {
        map.Clear();
        currentPath.reset();
        lastHeader = {};
        selectedEntity = -1;
        dirty = false;
        status = "New map.";
        g_mapUndo.Clear();
        SyncSmfGameplayToEngine(map);
    };

    auto doNew = [&]() {
        PushMapUndoSnapshot(map);
        map.Clear();
        currentPath.reset();
        lastHeader = {};
        selectedEntity = -1;
        dirty = false;
        status = "New map.";
        g_mapUndo.Clear();
        SyncSmfGameplayToEngine(map);
    };

    auto doSaveToDisk = [&](const std::string& pathStr) -> bool {
        Solstice::Smf::SmfError err = Solstice::Smf::SmfError::None;
        if (!Solstice::Smf::SaveSmfToFile(std::filesystem::path(pathStr), map, &err, compressSmf)) {
            status = std::string("Save failed: ") + SmfErrorMessage(err);
            jhBannerFile = status;
            return false;
        }
        jhBannerFile.clear();
        mapFileWatch.ResyncPath(pathStr);
        currentPath = pathStr;
        dirty = false;
        LibUI::Core::RecentPathPush(pathStr.c_str());
        std::vector<std::byte> bytes;
        if (Solstice::Smf::SaveSmfToBytes(map, bytes, &err, compressSmf)) {
            Solstice::Smf::LoadSmfFromBytes(map, bytes, &lastHeader, &err);
            SyncSmfGameplayToEngine(map);
        }
        status = "Saved " + pathStr;
        return true;
    };

    auto runValidate = [&]() {
        try {
            Solstice::Smf::SmfValidationReport rep;
            if (!Solstice::Smf::ValidateSmfMap(map, rep, compressSmf)) {
                lastValidateCodec = rep.loadStageNote.empty() ? std::string("Validation failed.") : rep.loadStageNote;
                lastValidateEngine = "(skipped — no serialized bytes)";
                lastValidateStructure.clear();
                status = lastValidateCodec;
                return;
            }
            if (!rep.roundTripOk) {
                lastValidateCodec =
                    rep.roundTripStageNote.empty() ? std::string("Round-trip failed.") : rep.roundTripStageNote;
                lastValidateEngine = "(skipped — LibSmf round-trip did not succeed)";
                lastValidateStructure.clear();
                for (const auto& msg : rep.structure) {
                    const char* tag = (msg.Level == Solstice::Smf::SmfMapValidationMessage::Severity::Error) ? "[error] "
                                                                                                              : "[warn] ";
                    lastValidateStructure.push_back(tag + msg.Text);
                }
                status = lastValidateCodec + " " + lastValidateEngine;
                return;
            }
            lastValidateCodec = rep.roundTripStageNote;
            std::vector<std::byte> bytes;
            Solstice::Smf::SmfError verr = Solstice::Smf::SmfError::None;
            if (Solstice::Smf::SaveSmfToBytes(map, bytes, &verr, compressSmf)) {
                lastValidateEngine = EngineDllSmfValidateSummary(bytes);
            } else {
                lastValidateEngine = "(skipped — serialize for engine failed)";
            }
            lastValidateStructure.clear();
            lastValidateStructure.reserve(rep.structure.size());
            for (const auto& msg : rep.structure) {
                const char* tag =
                    (msg.Level == Solstice::Smf::SmfMapValidationMessage::Severity::Error) ? "[error] " : "[warn] ";
                lastValidateStructure.push_back(tag + msg.Text);
            }
            status = lastValidateCodec + " " + lastValidateEngine;
        } catch (const std::exception& ex) {
            lastValidateEngine = std::string("(validate threw: ") + ex.what() + ")";
            lastValidateStructure.clear();
            status = lastValidateCodec + " " + lastValidateEngine;
        } catch (...) {
            lastValidateEngine = "(validate threw: non-std exception)";
            lastValidateStructure.clear();
            status = lastValidateCodec + " " + lastValidateEngine;
        }
    };

    auto runApplyGameplay = [&]() {
        try {
            std::vector<std::byte> bytes;
            Solstice::Smf::SmfError verr = Solstice::Smf::SmfError::None;
            if (!Solstice::Smf::SaveSmfToBytes(map, bytes, &verr, compressSmf)) {
                lastApplyGameplayEngine = "(skipped — serialize failed)";
                status = lastApplyGameplayEngine;
                return;
            }
            lastApplyGameplayEngine = EngineDllSmfApplyGameplaySummary(bytes);
            status = std::string("Apply gameplay DLL: ") + lastApplyGameplayEngine;
        } catch (const std::exception& ex) {
            lastApplyGameplayEngine = std::string("(apply threw: ") + ex.what() + ")";
            status = lastApplyGameplayEngine;
        } catch (...) {
            lastApplyGameplayEngine = "(apply threw: non-std exception)";
            status = lastApplyGameplayEngine;
        }
    };

    auto tryWriteJhRecoverySnapshot = [&]() {
        std::vector<std::byte> bytes;
        Solstice::Smf::SmfError werr = Solstice::Smf::SmfError::None;
        if (!Solstice::Smf::SaveSmfToBytes(map, bytes, &werr, compressSmf) || bytes.empty()) {
            return;
        }
        std::string wre;
        (void)Solstice::EditorAudio::FileRecovery::WriteSnapshot(
            jhRecoveryDir, "smf", std::span<const std::byte>(bytes.data(), bytes.size()), &wre);
        jhRecoveryModalOpen = !Solstice::EditorAudio::FileRecovery::List(jhRecoveryDir, "smf").empty();
        if (!wre.empty()) {
            status = "Recovery: " + wre;
        } else {
            status = "Recovery snapshot written to " + jhRecoveryDir.string();
        }
    };

    auto restoreJhMapFromRecoveryBytes = [&](std::span<const std::byte> bytes) -> bool {
        Solstice::Smf::SmfError rerr = Solstice::Smf::SmfError::None;
        if (!Solstice::Smf::LoadSmfFromBytes(map, bytes, &lastHeader, &rerr)) {
            status = std::string("Autosave restore failed: ") + SmfErrorMessage(rerr);
            return false;
        }
        currentPath.reset();
        selectedEntity = map.Entities.empty() ? -1 : 0;
        dirty = true;
        g_mapUndo.Clear();
        SyncSmfGameplayToEngine(map);
        jhBannerFile.clear();
        status = "Restored map from recovery autosave.";
        return true;
    };

    auto requestNew = [&]() {
        if (dirty) {
            unsavedPrompt = UnsavedPromptKind::NewMap;
        } else {
            doNew();
        }
    };

    auto requestOpen = [&](std::optional<std::string> path) {
        if (!path || path->empty()) {
            return;
        }
        if (dirty) {
            unsavedOpenPathPending = std::move(*path);
            unsavedPrompt = UnsavedPromptKind::OpenMap;
        } else {
            QueueOpenPath(std::move(*path));
        }
    };

    bool running = true;
    // SDL can briefly report 0x0 pixels during resize/maximize; ImGui requires Render() every frame after NewFrame().
    int jhLastDrawableW = 1280;
    int jhLastDrawableH = 720;
    while (running) {
        static std::unordered_map<std::string, Solstice::Math::Vec3> s_jhTextureTintCache;
        static LibUI::Graphics::PreviewTextureRgba s_jhDiffuseTexPreview{};
        static std::string s_jhDiffuseTexPreviewPath;
        static LibUI::Viewport::OrbitPanZoomState s_engineViewportNav{};
        static bool s_showAllEntityMarkers = true;
        static float s_placeGridSnap = 0.f;
        static float s_jhGridCellWorld = 1.f;
        static int s_jhGridHalfCount = 24;
        static bool s_jhOverlayBsp = true;
        static bool s_jhOverlayOct = true;
        static bool s_jhOverlayLights = true;
        static bool s_jhOverlayParticles = true;
        static int s_jhBspOverlayMaxDepth = 12;
        static int s_jhOctOverlayMaxDepth = 8;
        static bool s_jhEngineCaptureFailBannerShown = false;
        static bool s_jhPreviewUseSmat = false;
        static bool s_jhPreviewSmatSelectedOnly = false;
        static bool s_jhPreviewBindMaterialMaps = false;
        static char s_jhPreviewSmatBuf[768]{};
        static char s_jhPreviewMapAlbedo[768]{};
        static char s_jhPreviewMapNormal[768]{};
        static char s_jhPreviewMapRough[768]{};

        const bool compressBeforeFrame = compressSmf;
        const bool watchMapBeforeFrame = watchMapFile;
        const uint32_t jhRecoveryIntervalBeforeU32 = jhRecoveryIntervalSecU32;
        std::vector<std::string> jhFrameDrops;
        jhFrameDrops.reserve(8);
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            LibUI::Shell::CollectDropFilePathsFromEvent(event, jhFrameDrops);
            LibUI::Core::ProcessEvent(&event);
            if (event.type == SDL_EVENT_DROP_FILE && event.drop.data) {
                SDL_free((void*)event.drop.data);
            }
            if (event.type == SDL_EVENT_QUIT) {
                if (dirty) {
                    unsavedPrompt = UnsavedPromptKind::QuitApp;
                } else {
                    running = false;
                }
            }
        }

        for (const std::string& dropPath : jhFrameDrops) {
            std::filesystem::path p(dropPath);
            std::string ext = p.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (ext == ".smf") {
                requestOpen(std::optional<std::string>(dropPath));
            } else if (ext == ".gltf" || ext == ".glb") {
                PushMapUndoSnapshot(map);
                if (selectedEntity < 0 || selectedEntity >= static_cast<int>(map.Entities.size())) {
                    SmfEntity e = SmfMakeEntity(MakeUniqueEntityName(map, "mesh_drop"), "Mesh");
                    SetEntityOriginVec3(e, SmfVec3{0.f, 0.f, 0.f});
                    map.Entities.push_back(std::move(e));
                    selectedEntity = static_cast<int>(map.Entities.size()) - 1;
                }
                SmfEntity& ent = map.Entities[static_cast<size_t>(selectedEntity)];
                ent.ClassName = "Mesh";
                SetEntityModelAssetPath(ent, ToMapRelativePathIfPossible(dropPath, currentPath));
                dirty = true;
                status = "Assigned glTF from drop: " + dropPath;
                jhBannerFile.clear();
                jhBannerViewport.clear();
                SyncSmfGameplayToEngine(map);
            }
        }

        DrainPendingFileOps(map, currentPath, lastHeader, status, dirty, selectedEntity, compressSmf, &jhBannerFile,
            &jhBannerViewport, &mapFileWatch);
        DrainPendingRelicOps(map, currentPath, status, dirty, &jhBannerFile, &jhBannerViewport);
        DrainPendingGltfOps(map, currentPath, selectedEntity, status, dirty, &jhBannerFile, &jhBannerViewport);
        DrainPendingAcousticAudioImport(map, currentPath, status, dirty);

        if (watchMapFile) {
            mapFileWatch.SetEnabled(true);
            if (currentPath != lastWatchedMapPath) {
                mapFileWatch.ClearPaths();
                if (currentPath) {
                    mapFileWatch.AddPath(*currentPath);
                }
                lastWatchedMapPath = currentPath;
            }
            mapFileWatch.Poll();
        } else {
            mapFileWatch.SetEnabled(false);
        }

        UpdateWindowTitle(window, currentPath, dirty);

        // ImGui OpenGL3 requires this window's GL context current for the whole frame (engine preview runs bgfx off
        // another window and may leave the main thread without a current context).
        if (!SDL_GL_MakeCurrent(window, glContext)) {
            jhBannerFile = std::string("SDL_GL_MakeCurrent failed before NewFrame: ") + SDL_GetError();
        }

        LibUI::Shell::BeginUtilityImGuiFrame(window, jhLastDrawableW, jhLastDrawableH);

        ImGuiIO& io = ImGui::GetIO();
        jhRecoveryAccumSec += static_cast<double>(io.DeltaTime);
        if (dirty && jhRecoveryAccumSec >= static_cast<double>(jhRecoveryIntervalSecU32)) {
            jhRecoveryAccumSec = 0.0;
            tryWriteJhRecoverySnapshot();
        }
        const bool allowShortcuts = !io.WantTextInput;
        if (allowShortcuts) {
            if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_N)) {
                requestNew();
            }
            if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Z) && !io.KeyShift) {
                UndoMap(map, selectedEntity, dirty);
            }
            if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Y)
                || ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z)) {
                RedoMap(map, selectedEntity, dirty);
            }
            if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_O)) {
                LibUI::FileDialogs::ShowOpenFile(
                    window, "Open map", [&](std::optional<std::string> path) { requestOpen(std::move(path)); },
                    kSmfFilters);
            }
            if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S)) {
                if (currentPath) {
                    doSaveToDisk(*currentPath);
                } else {
                    LibUI::FileDialogs::ShowSaveFile(
                        window, "Save map as", [](std::optional<std::string> path) {
                            if (path) {
                                QueueSavePath(std::move(*path));
                            }
                        },
                        kSmfFilters);
                }
            }
            if (ImGui::IsKeyPressed(ImGuiKey_F7, false)) {
                runValidate();
            }
            if (ImGui::IsKeyPressed(ImGuiKey_F8, false)) {
                runApplyGameplay();
            }
            if (!map.Entities.empty()) {
                if (ImGui::IsKeyPressed(ImGuiKey_PageUp, false)) {
                    if (selectedEntity < 0) {
                        selectedEntity = 0;
                    } else {
                        selectedEntity = std::max(0, selectedEntity - 1);
                    }
                }
                if (ImGui::IsKeyPressed(ImGuiKey_PageDown, false)) {
                    const int n = static_cast<int>(map.Entities.size()) - 1;
                    if (selectedEntity < 0) {
                        selectedEntity = 0;
                    } else {
                        selectedEntity = std::min(n, selectedEntity + 1);
                    }
                }
            }
        }

        LibUI::Shell::BeginMainHostWindow("JackhammerRoot", LibUI::Shell::MainHostFlags_MenuBarNoTitle());

        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (LibUI::Icons::MenuItemWithIcon(LibUI::Icons::Id::Validate, "Validate map", "F7")) {
                    runValidate();
                }
                if (LibUI::Icons::MenuItemWithIcon(LibUI::Icons::Id::Play, "Probe engine apply gameplay (DLL)", "F8")) {
                    runApplyGameplay();
                }
                ImGui::Separator();
                if (LibUI::Icons::MenuItemWithIcon(LibUI::Icons::Id::New, "New", "Ctrl+N")) {
                    requestNew();
                }
                if (LibUI::Icons::MenuItemWithIcon(LibUI::Icons::Id::Duplicate, "New from template")) {
                    ApplyNewMapTemplate(map, currentPath, lastHeader, selectedEntity, dirty, status);
                }
                if (LibUI::Icons::MenuItemWithIcon(LibUI::Icons::Id::Open, "Open…", "Ctrl+O")) {
                    LibUI::FileDialogs::ShowOpenFile(
                        window, "Open map", [&](std::optional<std::string> path) { requestOpen(std::move(path)); },
                        kSmfFilters);
                }
                if (LibUI::Icons::MenuItemWithIcon(
                        LibUI::Icons::Id::Reload, "Reload from disk", nullptr, false, currentPath.has_value())) {
                    if (currentPath) {
                        if (!dirty) {
                            Solstice::Smf::SmfError err = Solstice::Smf::SmfError::None;
                            if (Solstice::Smf::LoadSmfFromFile(std::filesystem::path(*currentPath), map, &lastHeader, &err)) {
                                selectedEntity = map.Entities.empty() ? -1 : 0;
                                g_mapUndo.Clear();
                                status = "Reloaded " + *currentPath;
                                jhBannerFile.clear();
                                SyncSmfGameplayToEngine(map);
                            } else {
                                status = std::string("Reload failed: ") + SmfErrorMessage(err);
                                jhBannerFile = status;
                            }
                        } else {
                            showReloadFromDiskModal = true;
                        }
                    }
                }
                if (LibUI::Icons::MenuItemWithIcon(LibUI::Icons::Id::Save, "Save", "Ctrl+S")) {
                    if (currentPath) {
                        doSaveToDisk(*currentPath);
                    } else {
                        LibUI::FileDialogs::ShowSaveFile(
                            window, "Save map as", [](std::optional<std::string> path) {
                                if (path) {
                                    QueueSavePath(std::move(*path));
                                }
                            },
                            kSmfFilters);
                    }
                }
                if (LibUI::Icons::MenuItemWithIcon(LibUI::Icons::Id::Export, "Save As…")) {
                    LibUI::FileDialogs::ShowSaveFile(
                        window, "Save map as", [](std::optional<std::string> path) {
                            if (path) {
                                QueueSavePath(std::move(*path));
                            }
                        },
                        kSmfFilters);
                }
                ImGui::Separator();
                if (LibUI::Icons::MenuItemWithIcon(LibUI::Icons::Id::Import, "Import RELIC into path table…")) {
                    LibUI::FileDialogs::ShowOpenFile(
                        window, "Import RELIC", [](std::optional<std::string> path) {
                            if (path) {
                                QueueRelicImportPath(std::move(*path));
                            }
                        },
                        kRelicFilters);
                }
                if (LibUI::Icons::MenuItemWithIcon(LibUI::Icons::Id::Export, "Export path table to RELIC…")) {
                    LibUI::FileDialogs::ShowSaveFile(
                        window, "Export RELIC", [](std::optional<std::string> path) {
                            if (path) {
                                QueueRelicExportPath(std::move(*path));
                            }
                        },
                        kRelicFilters);
                }
                if (LibUI::Icons::MenuItemWithIcon(LibUI::Icons::Id::Import, "Import glTF asset to selected entity…")) {
                    LibUI::FileDialogs::ShowOpenFile(
                        window, "Import glTF asset", [](std::optional<std::string> path) {
                            if (path) {
                                QueueGltfImportPath(std::move(*path));
                            }
                        },
                        kGltfFilters);
                }
                if (LibUI::Icons::MenuItemWithIcon(LibUI::Icons::Id::Export, "Export selected glTF asset…")) {
                    LibUI::FileDialogs::ShowSaveFile(
                        window, "Export selected glTF asset", [](std::optional<std::string> path) {
                            if (path) {
                                QueueGltfExportPath(std::move(*path));
                            }
                        },
                        kGltfFilters);
                }
                ImGui::Separator();
                if (LibUI::Icons::MenuItemWithIcon(LibUI::Icons::Id::Save, "Write recovery snapshot now")) {
                    tryWriteJhRecoverySnapshot();
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit")) {
                if (LibUI::Icons::MenuItemWithIcon(LibUI::Icons::Id::Prev, "Undo", "Ctrl+Z", false, g_mapUndo.CanUndo())) {
                    UndoMap(map, selectedEntity, dirty);
                }
                if (LibUI::Icons::MenuItemWithIcon(LibUI::Icons::Id::Next, "Redo", "Ctrl+Y", false, g_mapUndo.CanRedo())) {
                    RedoMap(map, selectedEntity, dirty);
                }
                ImGui::Separator();
                if (ImGui::BeginMenu("Add Arzachel prop to mesh workshop")) {
                    if (ImGui::MenuItem("Cube")) {
                        Jackhammer::ArzachelProps::PropBuildParams p;
                        p.uniformScale = std::max(0.01f, s_jhArzUniformScale);
                        Jackhammer::ArzachelProps::AddArzachelProp(Jackhammer::ArzachelProps::PropKind::ArzachelCube, p,
                            s_jhMeshWorkshop, s_jhMeshWorkshopLine, sizeof(s_jhMeshWorkshopLine));
                    }
                    if (ImGui::MenuItem("UV sphere")) {
                        Jackhammer::ArzachelProps::PropBuildParams p;
                        p.uniformScale = std::max(0.01f, s_jhArzUniformScale);
                        p.i0 = std::max(3, s_jhPrimSphLo);
                        Jackhammer::ArzachelProps::AddArzachelProp(
                            Jackhammer::ArzachelProps::PropKind::UvSphere, p, s_jhMeshWorkshop, s_jhMeshWorkshopLine,
                            sizeof(s_jhMeshWorkshopLine));
                    }
                    if (ImGui::MenuItem("Isosphere")) {
                        Jackhammer::ArzachelProps::PropBuildParams p;
                        p.uniformScale = std::max(0.01f, s_jhArzUniformScale);
                        p.i2 = std::max(0, s_jhArzIsoSubdiv);
                        Jackhammer::ArzachelProps::AddArzachelProp(
                            Jackhammer::ArzachelProps::PropKind::Icosphere, p, s_jhMeshWorkshop, s_jhMeshWorkshopLine,
                            sizeof(s_jhMeshWorkshopLine));
                    }
                    if (ImGui::MenuItem("Cylinder")) {
                        Jackhammer::ArzachelProps::PropBuildParams p;
                        p.uniformScale = std::max(0.01f, s_jhArzUniformScale);
                        p.i0 = std::max(3, s_jhPrimCylRad);
                        Jackhammer::ArzachelProps::AddArzachelProp(
                            Jackhammer::ArzachelProps::PropKind::Cylinder, p, s_jhMeshWorkshop, s_jhMeshWorkshopLine,
                            sizeof(s_jhMeshWorkshopLine));
                    }
                    if (ImGui::MenuItem("Torus")) {
                        Jackhammer::ArzachelProps::PropBuildParams p;
                        p.uniformScale = std::max(0.01f, s_jhArzUniformScale);
                        p.i0 = std::max(3, s_jhPrimTorMaj);
                        p.i1 = std::max(3, s_jhPrimTorMin);
                        Jackhammer::ArzachelProps::AddArzachelProp(
                            Jackhammer::ArzachelProps::PropKind::Torus, p, s_jhMeshWorkshop, s_jhMeshWorkshopLine,
                            sizeof(s_jhMeshWorkshopLine));
                    }
                    if (ImGui::MenuItem("Ground plane")) {
                        Jackhammer::ArzachelProps::PropBuildParams p;
                        p.uniformScale = std::max(0.01f, s_jhArzUniformScale);
                        Jackhammer::ArzachelProps::AddArzachelProp(
                            Jackhammer::ArzachelProps::PropKind::GroundPlane, p, s_jhMeshWorkshop, s_jhMeshWorkshopLine,
                            sizeof(s_jhMeshWorkshopLine));
                    }
                    if (ImGui::MenuItem("Tetrahedron")) {
                        Jackhammer::ArzachelProps::PropBuildParams p;
                        p.uniformScale = std::max(0.01f, s_jhArzUniformScale);
                        Jackhammer::ArzachelProps::AddArzachelProp(
                            Jackhammer::ArzachelProps::PropKind::Tetrahedron, p, s_jhMeshWorkshop, s_jhMeshWorkshopLine,
                            sizeof(s_jhMeshWorkshopLine));
                    }
                    if (ImGui::MenuItem("Square pyramid")) {
                        Jackhammer::ArzachelProps::PropBuildParams p;
                        p.uniformScale = std::max(0.01f, s_jhArzUniformScale);
                        Jackhammer::ArzachelProps::AddArzachelProp(
                            Jackhammer::ArzachelProps::PropKind::SquarePyramid, p, s_jhMeshWorkshop, s_jhMeshWorkshopLine,
                            sizeof(s_jhMeshWorkshopLine));
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                if (LibUI::Icons::MenuItemWithIcon(LibUI::Icons::Id::Plugins, "Plugins")) {
                    showPluginsPanel = true;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Help")) {
                if (LibUI::Icons::MenuItemWithIcon(LibUI::Icons::Id::Shortcuts, "Keyboard shortcuts…")) {
                    showShortcutsPanel = true;
                }
                if (LibUI::Icons::MenuItemWithIcon(LibUI::Icons::Id::About, "About Jackhammer")) {
                    showAboutPanel = true;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        if (showReloadFromDiskModal) {
            ImGui::OpenPopup("JH_ReloadFromDisk");
            showReloadFromDiskModal = false;
        }
        if (ImGui::BeginPopupModal("JH_ReloadFromDisk", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("Discard unsaved edits and reload the file from disk?");
            if (currentPath) {
                ImGui::TextWrapped("%s", currentPath->c_str());
            }
            if (ImGui::Button("Reload", ImVec2(120, 0)) && currentPath) {
                Solstice::Smf::SmfError err = Solstice::Smf::SmfError::None;
                if (Solstice::Smf::LoadSmfFromFile(std::filesystem::path(*currentPath), map, &lastHeader, &err)) {
                    selectedEntity = map.Entities.empty() ? -1 : 0;
                    dirty = false;
                    g_mapUndo.Clear();
                    status = "Reloaded " + *currentPath;
                    SyncSmfGameplayToEngine(map);
                } else {
                    status = std::string("Reload failed: ") + SmfErrorMessage(err);
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (jhRecoveryModalOpen) {
            ImGui::OpenPopup("JH_SmfRecovery");
        }
        if (ImGui::BeginPopupModal("JH_SmfRecovery", &jhRecoveryModalOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("A local .smf recovery snapshot is available. Restore it?");
            if (ImGui::Button("Restore", ImVec2(140, 0))) {
                std::vector<std::byte> buf;
                std::string re;
                if (Solstice::EditorAudio::FileRecovery::ReadLatest(jhRecoveryDir, "smf", buf, &re)) {
                    if (restoreJhMapFromRecoveryBytes(std::span<const std::byte>(buf.data(), buf.size()))) {
                        Solstice::EditorAudio::FileRecovery::ClearMatchingPrefix(jhRecoveryDir, "smf");
                        jhRecoveryModalOpen = false;
                        ImGui::CloseCurrentPopup();
                    }
                } else {
                    status = "Could not read recovery: " + re;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Dismiss", ImVec2(140, 0))) {
                jhRecoveryModalOpen = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (unsavedPrompt != UnsavedPromptKind::None) {
            ImGui::OpenPopup("JH_Unsaved");
        }
        if (ImGui::BeginPopupModal("JH_Unsaved", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            const char* msg = "Save changes before continuing?";
            if (unsavedPrompt == UnsavedPromptKind::QuitApp) {
                msg = "Save changes before quitting?";
            } else if (unsavedPrompt == UnsavedPromptKind::NewMap) {
                msg = "Save changes before creating a new map?";
            } else if (unsavedPrompt == UnsavedPromptKind::OpenMap) {
                msg = "Save changes before opening another file?";
            }
            ImGui::TextUnformatted(msg);
            auto finishUnsaved = [&](UnsavedPromptKind kind) {
                unsavedPrompt = UnsavedPromptKind::None;
                ImGui::CloseCurrentPopup();
                if (kind == UnsavedPromptKind::QuitApp) {
                    running = false;
                } else if (kind == UnsavedPromptKind::NewMap) {
                    doNewCommit();
                } else if (kind == UnsavedPromptKind::OpenMap) {
                    if (unsavedOpenPathPending) {
                        QueueOpenPath(std::move(*unsavedOpenPathPending));
                        unsavedOpenPathPending.reset();
                    }
                }
            };
            const LibUI::Tools::UnsavedModalResult um = LibUI::Tools::DrawUnsavedModalButtons();
            if (um == LibUI::Tools::UnsavedModalResult::Save) {
                const UnsavedPromptKind kind = unsavedPrompt;
                if (currentPath) {
                    if (doSaveToDisk(*currentPath)) {
                        finishUnsaved(kind);
                    }
                } else {
                    LibUI::FileDialogs::ShowSaveFile(
                        window, "Save map as",
                        [&](std::optional<std::string> path) {
                            if (!path) {
                                return;
                            }
                            if (doSaveToDisk(*path)) {
                                finishUnsaved(kind);
                            }
                        },
                        kSmfFilters);
                }
            } else if (um == LibUI::Tools::UnsavedModalResult::Discard) {
                const UnsavedPromptKind kind = unsavedPrompt;
                finishUnsaved(kind);
            } else if (um == LibUI::Tools::UnsavedModalResult::Cancel) {
                unsavedPrompt = UnsavedPromptKind::None;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        LibUI::Tools::DrawFileChangedOnDiskModal(
            "JH_DiskReload", &showDiskReloadModal, "The map file changed on disk.", "Reload from disk", "Keep editor copy",
            200.f,
            [&]() {
                if (currentPath) {
                    Solstice::Smf::SmfError err = Solstice::Smf::SmfError::None;
                    if (Solstice::Smf::LoadSmfFromFile(std::filesystem::path(*currentPath), map, &lastHeader, &err)) {
                        selectedEntity = map.Entities.empty() ? -1 : 0;
                        dirty = false;
                        status = "Reloaded from disk.";
                        jhBannerFile.clear();
                        g_mapUndo.Clear();
                        SyncSmfGameplayToEngine(map);
                    } else {
                        status = std::string("Reload failed: ") + SmfErrorMessage(err);
                        jhBannerFile = status;
                    }
                }
            },
            []() {},
            [&]() {
                mapFileWatch.ClearPaths();
                if (currentPath) {
                    mapFileWatch.AddPath(*currentPath);
                }
                lastWatchedMapPath = currentPath;
            });

        if (!jhBannerFile.empty() || !jhBannerViewport.empty()) {
            if (!jhBannerFile.empty()) {
                ImGui::TextColored(ImVec4(1.f, 0.35f, 0.35f, 1.f), "%s", jhBannerFile.c_str());
            }
            if (!jhBannerViewport.empty()) {
                ImGui::TextColored(ImVec4(1.f, 0.42f, 0.32f, 1.f), "%s", jhBannerViewport.c_str());
            }
            if (ImGui::SmallButton("Dismiss##jherr")) {
                jhBannerFile.clear();
                jhBannerViewport.clear();
                s_jhEngineCaptureFailBannerShown = false;
            }
            ImGui::Separator();
        }

        constexpr float kJhBottomBarH = 200.f;
        ImGui::BeginChild("jh_main_workspace", ImVec2(0.f, -kJhBottomBarH), false);
        const float jhLeftW = 272.f;
        const float jhRightW = 308.f;
        const float jhGap = ImGui::GetStyle().ItemSpacing.x;
        const float jhAvailW = ImGui::GetContentRegionAvail().x;
        float jhCenterW = jhAvailW - jhLeftW - jhRightW - jhGap * 2.f;
        if (jhCenterW < 120.f) {
            jhCenterW = std::max(80.f, jhAvailW * 0.28f);
        }

        ImGui::BeginChild("jh_left_col", ImVec2(jhLeftW, 0), true);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.f, 8.f));
        ImGui::TextUnformatted("Entities");
        ImGui::SetNextItemWidth(-1.f);
        ImGui::InputTextWithHint("##ent_filter_name", "Name filter", entityNameFilter, sizeof(entityNameFilter));
        ImGui::SetNextItemWidth(-1.f);
        ImGui::InputTextWithHint("##ent_filter_class", "Class filter", entityClassFilter, sizeof(entityClassFilter));
        ImGui::Text("Selected: %d / %zu", selectedEntity, map.Entities.size());
        auto jhTip = [](const char* tip) {
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
                ImGui::SetTooltip("%s", tip);
            }
        };
        if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::New, "")) {
            PushMapUndoSnapshot(map);
            const int n = static_cast<int>(map.Entities.size());
            map.Entities.push_back(SmfMakeEntity("entity_" + std::to_string(n), "Entity"));
            selectedEntity = static_cast<int>(map.Entities.size()) - 1;
            dirty = true;
        }
        jhTip("Add entity");
        ImGui::SameLine();
        if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::Duplicate, "") && selectedEntity >= 0 &&
            selectedEntity < static_cast<int>(map.Entities.size())) {
            PushMapUndoSnapshot(map);
            SmfEntity dup = map.Entities[static_cast<size_t>(selectedEntity)];
            const std::string baseName = dup.Name.empty() ? std::string("entity") : dup.Name;
            dup.Name = MakeUniqueEntityName(map, baseName + "_copy");
            map.Entities.push_back(std::move(dup));
            selectedEntity = static_cast<int>(map.Entities.size()) - 1;
            dirty = true;
        }
        jhTip("Duplicate selected entity");
        ImGui::SameLine();
        if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::Remove, "") && selectedEntity >= 0 &&
            selectedEntity < static_cast<int>(map.Entities.size())) {
            ImGui::OpenPopup("ConfirmDeleteEntity");
        }
        jhTip("Remove selected entity");
        ImGui::SameLine();
        if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::Prev, "") && !map.Entities.empty()) {
            if (selectedEntity < 0) {
                selectedEntity = 0;
            } else {
                selectedEntity = std::max(0, selectedEntity - 1);
            }
        }
        jhTip("Select previous entity");
        ImGui::SameLine();
        if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::Next, "") && !map.Entities.empty()) {
            const int last = static_cast<int>(map.Entities.size()) - 1;
            if (selectedEntity < 0) {
                selectedEntity = 0;
            } else {
                selectedEntity = std::min(last, selectedEntity + 1);
            }
        }
        jhTip("Select next entity");
        if (ImGui::SmallButton("Add particle emitter##jhaddpt")) {
            PushMapUndoSnapshot(map);
            SmfEntity e = SmfMakeEntity(
                MakeUniqueEntityName(map, "particle_emitter"), std::string(Jackhammer::Particles::kEmitterClassName));
            SetEntityOriginVec3(e, SmfVec3{0.f, 1.f, 0.f});
            SetEntityFloatByKey(e, "particleRate", 8.f);
            map.Entities.push_back(std::move(e));
            selectedEntity = static_cast<int>(map.Entities.size()) - 1;
            dirty = true;
        }
        jhTip("ParticleEmitter: viewport marker; fields are authoring-only until runtime consumes them.");

        if (ImGui::BeginPopupModal("ConfirmDeleteEntity", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            if (selectedEntity >= 0 && selectedEntity < static_cast<int>(map.Entities.size())) {
                const std::string& nm = map.Entities[static_cast<size_t>(selectedEntity)].Name;
                ImGui::Text("Remove entity '%s'?", nm.empty() ? "(unnamed)" : nm.c_str());
            } else {
                ImGui::TextUnformatted("No entity selected.");
            }
            if (ImGui::Button("Delete", ImVec2(120, 0)) && selectedEntity >= 0
                && selectedEntity < static_cast<int>(map.Entities.size())) {
                PushMapUndoSnapshot(map);
                map.Entities.erase(map.Entities.begin() + selectedEntity);
                if (map.Entities.empty()) {
                    selectedEntity = -1;
                } else {
                    selectedEntity = std::min(selectedEntity, static_cast<int>(map.Entities.size()) - 1);
                }
                dirty = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        static std::vector<int> s_jhEntListRows;
        static size_t s_jhEntListMapSz = ~static_cast<size_t>(0);
        static char s_jhEntListNameFilt[256] = {};
        static char s_jhEntListClassFilt[256] = {};
        const bool entListDirty = s_jhEntListMapSz != map.Entities.size()
            || std::memcmp(s_jhEntListNameFilt, entityNameFilter, sizeof(entityNameFilter)) != 0
            || std::memcmp(s_jhEntListClassFilt, entityClassFilter, sizeof(entityClassFilter)) != 0;
        if (entListDirty) {
            s_jhEntListRows.clear();
            s_jhEntListMapSz = map.Entities.size();
            std::memcpy(s_jhEntListNameFilt, entityNameFilter, sizeof(entityNameFilter));
            std::memcpy(s_jhEntListClassFilt, entityClassFilter, sizeof(entityClassFilter));

            auto nameMatches = [&](const SmfEntity& e) {
                const std::string lname = e.Name.empty() ? std::string("(unnamed)") : e.Name;
                if (entityNameFilter[0] == '\0') {
                    return true;
                }
                std::string lfilter(entityNameFilter);
                std::string ln = lname;
                std::transform(ln.begin(), ln.end(), ln.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                std::transform(lfilter.begin(), lfilter.end(), lfilter.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                return ln.find(lfilter) != std::string::npos;
            };
            auto classMatches = [&](const SmfEntity& e) {
                if (entityClassFilter[0] == '\0') {
                    return true;
                }
                std::string cfilter(entityClassFilter);
                std::string lc = e.ClassName.empty() ? "" : e.ClassName;
                std::transform(lc.begin(), lc.end(), lc.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                std::transform(cfilter.begin(), cfilter.end(), cfilter.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                return lc.find(cfilter) != std::string::npos;
            };

            const bool filtersEmpty = entityNameFilter[0] == '\0' && entityClassFilter[0] == '\0';
            if (filtersEmpty) {
                const size_t lim = std::min(map.Entities.size(), static_cast<size_t>(kJhEntityListMaxDisplayedRows));
                s_jhEntListRows.reserve(lim);
                for (size_t i = 0; i < lim; ++i) {
                    s_jhEntListRows.push_back(static_cast<int>(i));
                }
            } else {
                const size_t nScan = std::min(map.Entities.size(), kJhEntityListMaxScanEntities);
                for (size_t i = 0; i < nScan; ++i) {
                    const auto& e = map.Entities[i];
                    if (!nameMatches(e) || !classMatches(e)) {
                        continue;
                    }
                    s_jhEntListRows.push_back(static_cast<int>(i));
                    if (s_jhEntListRows.size() >= kJhEntityListMaxDisplayedRows) {
                        break;
                    }
                }
            }
        }

        if (map.Entities.size() > static_cast<size_t>(kJhEntityListMaxDisplayedRows) && entityNameFilter[0] == '\0'
            && entityClassFilter[0] == '\0') {
            ImGui::TextDisabled("Listing first %d entities (use filters to narrow).", kJhEntityListMaxDisplayedRows);
        }
        if ((entityNameFilter[0] != '\0' || entityClassFilter[0] != '\0')
            && map.Entities.size() > kJhEntityListMaxScanEntities) {
            ImGui::TextDisabled("Filter scans at most %zu entities on huge maps.", kJhEntityListMaxScanEntities);
        }

        ImGui::Separator();
        if (ImGui::BeginTable("entList", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Class", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(s_jhEntListRows.size()));
            while (clipper.Step()) {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                    const int i = s_jhEntListRows[static_cast<size_t>(row)];
                    const auto& e = map.Entities[static_cast<size_t>(i)];
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::PushID(i);
                    const bool sel = (selectedEntity == i);
                    if (ImGui::Selectable(e.Name.empty() ? "(unnamed)" : e.Name.c_str(), sel, ImGuiSelectableFlags_SpanAllColumns)) {
                        selectedEntity = i;
                    }
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(e.ClassName.empty() ? "?" : e.ClassName.c_str());
                    ImGui::PopID();
                }
            }
            ImGui::EndTable();
        }
        ImGui::PopStyleVar();
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("jh_center_col", ImVec2(jhCenterW, 0), false);
        ImGui::BeginChild("jh_vp_toolbar", ImVec2(0, 196), true);
        if (ImGui::Button("Persp##jhvp")) {
            s_engineViewportNav.projection = LibUI::Viewport::OrbitProjectionMode::Perspective;
        }
        ImGui::SameLine();
        if (ImGui::Button("Top##jhvp")) {
            s_engineViewportNav.projection = LibUI::Viewport::OrbitProjectionMode::OrthoTop;
        }
        ImGui::SameLine();
        if (ImGui::Button("Front##jhvp")) {
            s_engineViewportNav.projection = LibUI::Viewport::OrbitProjectionMode::OrthoFront;
        }
        ImGui::SameLine();
        if (ImGui::Button("Side##jhvp")) {
            s_engineViewportNav.projection = LibUI::Viewport::OrbitProjectionMode::OrthoSide;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Reset##jhvp")) {
            LibUI::Viewport::ResetOrbitPanZoom(s_engineViewportNav);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Focus##jhvp") && selectedEntity >= 0 &&
            static_cast<size_t>(selectedEntity) < map.Entities.size()) {
            const SmfVec3* o = TryGetEntityOriginVec3(map.Entities[static_cast<size_t>(selectedEntity)]);
            if (o) {
                LibUI::Viewport::FocusOrbitOnTarget(s_engineViewportNav, o->x, o->y, o->z, 0.f, 0.f, 0.f);
            }
        }
        ImGui::SameLine();
        ImGui::Checkbox("Origins##jhvp", &s_showAllEntityMarkers);
        ImGui::SameLine();
        ImGui::Checkbox("BSP##jhvp", &s_jhOverlayBsp);
        ImGui::SameLine();
        ImGui::Checkbox("Oct##jhvp", &s_jhOverlayOct);
        ImGui::SameLine();
        ImGui::Checkbox("Lights##jhvp", &s_jhOverlayLights);
        ImGui::SameLine();
        ImGui::Checkbox("Pt##jhvp", &s_jhOverlayParticles);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(44);
        ImGui::DragInt("Bd##jhbspd", &s_jhBspOverlayMaxDepth, 0.2f, 0, 24);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(44);
        ImGui::DragInt("Od##jhoctd", &s_jhOctOverlayMaxDepth, 0.2f, 0, 24);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(88);
        ImGui::DragFloat("Snap##jhgrid", &s_placeGridSnap, 0.05f, 0.f, 64.f);
        ImGui::SameLine();
        ImGui::TextDisabled("presets:");
        for (float sp : {1.f, 2.f, 4.f, 8.f, 16.f, 32.f}) {
            ImGui::SameLine();
            char lbl[24];
            std::snprintf(lbl, sizeof(lbl), "%g##jhsp", static_cast<double>(sp));
            if (ImGui::SmallButton(lbl)) {
                s_placeGridSnap = sp;
            }
        }
        ImGui::DragFloat("Grid cell##jhgridcell", &s_jhGridCellWorld, 0.05f, 0.0625f, 256.f);
        ImGui::SameLine();
        ImGui::DragInt("Grid lines##jhgridhalf", &s_jhGridHalfCount, 0.5f, 4, 128);
        ImGui::Separator();
        ImGui::TextUnformatted("Material preview (EditorEnginePreview)");
        ImGui::Checkbox("Preview .smat##jh", &s_jhPreviewUseSmat);
        ImGui::SameLine();
        ImGui::Checkbox("Selected entity only##jhsmatsel", &s_jhPreviewSmatSelectedOnly);
        ImGui::SameLine();
        ImGui::Checkbox("Optional maps##jhmaps", &s_jhPreviewBindMaterialMaps);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 88.f);
        ImGui::InputTextWithHint("##jhsmatpath", ".smat path (map-relative or absolute)", s_jhPreviewSmatBuf,
            sizeof(s_jhPreviewSmatBuf));
        ImGui::SameLine();
        if (ImGui::Button("Browse##jhsmat")) {
            LibUI::FileDialogs::ShowOpenFile(
                window, "Solstice material (.smat)",
                [&](std::optional<std::string> path) {
                    if (!path || path->empty()) {
                        return;
                    }
                    const std::string rel = ToMapRelativePathIfPossible(*path, currentPath);
                    std::snprintf(s_jhPreviewSmatBuf, sizeof(s_jhPreviewSmatBuf), "%s", rel.c_str());
                },
                kSmatFilters);
        }
        if (s_jhPreviewBindMaterialMaps) {
            ImGui::TextDisabled("Maps override slots for this preview pass (albedo / normal / roughness); paths like diffuse.");
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 72.f);
            ImGui::InputTextWithHint("##jhmapalb", "Albedo map (optional)", s_jhPreviewMapAlbedo, sizeof(s_jhPreviewMapAlbedo));
            ImGui::SameLine();
            if (ImGui::SmallButton("…##jhmalb")) {
                LibUI::FileDialogs::ShowOpenFile(
                    window, "Preview albedo map",
                    [&](std::optional<std::string> path) {
                        if (!path || path->empty()) {
                            return;
                        }
                        const std::string rel = ToMapRelativePathIfPossible(*path, currentPath);
                        std::snprintf(s_jhPreviewMapAlbedo, sizeof(s_jhPreviewMapAlbedo), "%s", rel.c_str());
                    },
                    kJhImageFileFilters);
            }
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 72.f);
            ImGui::InputTextWithHint("##jhmapnrm", "Normal map (optional)", s_jhPreviewMapNormal, sizeof(s_jhPreviewMapNormal));
            ImGui::SameLine();
            if (ImGui::SmallButton("…##jhmnrm")) {
                LibUI::FileDialogs::ShowOpenFile(
                    window, "Preview normal map",
                    [&](std::optional<std::string> path) {
                        if (!path || path->empty()) {
                            return;
                        }
                        const std::string rel = ToMapRelativePathIfPossible(*path, currentPath);
                        std::snprintf(s_jhPreviewMapNormal, sizeof(s_jhPreviewMapNormal), "%s", rel.c_str());
                    },
                    kJhImageFileFilters);
            }
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 72.f);
            ImGui::InputTextWithHint("##jhmaprgh", "Roughness map (optional)", s_jhPreviewMapRough, sizeof(s_jhPreviewMapRough));
            ImGui::SameLine();
            if (ImGui::SmallButton("…##jhmrgh")) {
                LibUI::FileDialogs::ShowOpenFile(
                    window, "Preview roughness map",
                    [&](std::optional<std::string> path) {
                        if (!path || path->empty()) {
                            return;
                        }
                        const std::string rel = ToMapRelativePathIfPossible(*path, currentPath);
                        std::snprintf(s_jhPreviewMapRough, sizeof(s_jhPreviewMapRough), "%s", rel.c_str());
                    },
                    kJhImageFileFilters);
            }
        }
        ImGui::EndChild();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::BeginChild("jh_vp_canvas", ImVec2(0, 0), false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        LibUI::Viewport::Frame engineVp{};
        if (LibUI::Viewport::BeginHost("jh_engine_viewport", ImVec2(-1, -1), false)) {
            // Always EndHost (EndChild) even if drawing throws — otherwise JackhammerRoot gets ImGui::End() vs EndChild mismatch.
            struct JhEngineViewportHostScope {
                ~JhEngineViewportHostScope() { LibUI::Viewport::EndHost(); }
            } jhEngineViewportHostScope;
            (void)jhEngineViewportHostScope;
            if (LibUI::Viewport::PollFrame(engineVp) && engineVp.draw_list) {
                const float aspect =
                    std::max(engineVp.size.y, 1.0f) > 0.f ? engineVp.size.x / std::max(engineVp.size.y, 1.0f) : 1.f;
                int engW = std::max(2, static_cast<int>(engineVp.size.x));
                int engH = std::max(2, static_cast<int>(engineVp.size.y));
                ClampJhEngineFramebuffer(engW, engH);
                size_t previewEligible = 0;
                std::vector<Solstice::EditorEnginePreview::PreviewEntity> engEnts;
                engEnts.reserve(std::min(map.Entities.size(), kJhMaxEnginePreviewEntities));
                for (size_t ei = 0; ei < map.Entities.size(); ++ei) {
                    if (map.Entities[ei].ClassName == "Light"
                        || map.Entities[ei].ClassName == Jackhammer::Particles::kEmitterClassName) {
                        continue;
                    }
                    const SmfVec3* o = TryGetEntityOriginVec3(map.Entities[ei]);
                    if (!o) {
                        continue;
                    }
                    ++previewEligible;
                    if (engEnts.size() >= kJhMaxEnginePreviewEntities) {
                        continue;
                    }
                    Solstice::EditorEnginePreview::PreviewEntity pe{};
                    pe.Position = Solstice::Math::Vec3(o->x, o->y, o->z);
                    const bool isSel = selectedEntity >= 0 && static_cast<size_t>(selectedEntity) == ei;
                    pe.Albedo =
                        isSel ? Solstice::Math::Vec3(1.f, 0.72f, 0.22f) : Solstice::Math::Vec3(0.52f, 0.56f, 0.72f);
                    pe.HalfExtent = isSel ? 0.42f : 0.3f;
                    if (const SmfVec3* sc = TryGetEntityVec3ByKey(map.Entities[ei], "scale")) {
                        pe.Scale = Solstice::Math::Vec3(sc->x, sc->y, sc->z);
                    }
                    pe.PitchDeg = TryGetEntityFloatByKey(map.Entities[ei], "pitch", 0.f);
                    pe.YawDeg = TryGetEntityFloatByKey(map.Entities[ei], "yaw", 0.f);
                    pe.RollDeg = TryGetEntityFloatByKey(map.Entities[ei], "roll", 0.f);
                    if (const char* tp = TryGetEntityDiffuseTexturePath(map.Entities[ei])) {
                        const std::string tkey(tp);
                        auto it = s_jhTextureTintCache.find(tkey);
                        if (it != s_jhTextureTintCache.end()) {
                            pe.Albedo = it->second;
                        } else {
                            try {
                                std::vector<std::byte> rgba;
                                int iw = 0;
                                int ih = 0;
                                if (LibUI::Tools::LoadImageFileToRgba8(tkey, rgba, iw, ih)) {
                                    float rgb[3]{};
                                    LibUI::Tools::AverageRgbFromRgba8(rgba.data(), iw, ih, rgb);
                                    pe.Albedo = Solstice::Math::Vec3(rgb[0], rgb[1], rgb[2]);
                                    s_jhTextureTintCache[tkey] = pe.Albedo;
                                    if (s_jhTextureTintCache.size() > kJhMaxTextureTintCacheEntries) {
                                        s_jhTextureTintCache.clear();
                                    }
                                }
                            } catch (const std::bad_alloc&) {
                                jhBannerViewport = "Viewport: texture decode OOM (skip tint).";
                            }
                        }
                    }
                    if (isSel) {
                        pe.Albedo = Solstice::Math::Vec3(1.f, 0.72f, 0.22f);
                    }
                    const bool previewMatThis = !s_jhPreviewSmatSelectedOnly || isSel;
                    if (s_jhPreviewUseSmat && previewMatThis) {
                        std::string smatResolved;
                        if (const char* entM = TryGetEntityMaterialPath(map.Entities[ei])) {
                            smatResolved = JhResolveMapAssetPath(currentPath, std::string(entM));
                        } else if (s_jhPreviewSmatBuf[0] != '\0') {
                            smatResolved = JhResolveMapAssetPath(currentPath, std::string(s_jhPreviewSmatBuf));
                        }
                        if (!smatResolved.empty()) {
                            std::strncpy(pe.PreviewSmatPath, smatResolved.c_str(), sizeof(pe.PreviewSmatPath) - 1);
                            pe.PreviewSmatPath[sizeof(pe.PreviewSmatPath) - 1] = '\0';
                        }
                    }
                    if (s_jhPreviewBindMaterialMaps && previewMatThis) {
                        auto bindPreviewMap = [&](char* dst, size_t dstSz, const char* mapBuf) {
                            if (!mapBuf || mapBuf[0] == '\0') {
                                return;
                            }
                            const std::string r = JhResolveMapAssetPath(currentPath, std::string(mapBuf));
                            if (r.empty()) {
                                return;
                            }
                            std::strncpy(dst, r.c_str(), dstSz - 1);
                            dst[dstSz - 1] = '\0';
                        };
                        bindPreviewMap(pe.PreviewAlbedoTexturePath, sizeof(pe.PreviewAlbedoTexturePath), s_jhPreviewMapAlbedo);
                        bindPreviewMap(pe.PreviewNormalTexturePath, sizeof(pe.PreviewNormalTexturePath), s_jhPreviewMapNormal);
                        bindPreviewMap(pe.PreviewRoughnessTexturePath, sizeof(pe.PreviewRoughnessTexturePath), s_jhPreviewMapRough);
                    }
                    engEnts.push_back(pe);
                }
                const bool engEntsTruncated = previewEligible > kJhMaxEnginePreviewEntities;
                SyncSmfGameplayToEngine(map);
                const std::vector<Solstice::Physics::LightSource>& engLights =
                    Solstice::Arzachel::MapSerializer::GetAuthoringLightsFromLastSmfApply();
                static Solstice::Physics::LightSource s_fallbackSun{};
                const Solstice::Physics::LightSource* lightPtr = nullptr;
                size_t lightCount = 0;
                if (!engLights.empty()) {
                    lightPtr = engLights.data();
                    lightCount = engLights.size();
                } else {
                    s_fallbackSun = {};
                    s_fallbackSun.Type = Solstice::Physics::LightSource::LightType::Directional;
                    s_fallbackSun.Position = Solstice::Math::Vec3(0.42f, 0.84f, 0.36f).Normalized();
                    s_fallbackSun.Color = Solstice::Math::Vec3(1.f, 0.96f, 0.88f);
                    s_fallbackSun.Intensity = 1.2f;
                    lightPtr = &s_fallbackSun;
                    lightCount = 1;
                }
                std::vector<std::byte> capRgba;
                int cw = 0;
                int ch = 0;
                try {
                    if (Solstice::EditorEnginePreview::CaptureOrbitRgb(s_engineViewportNav, 0.f, 0.f, 0.f, 55.f, aspect,
                            engW, engH, engEnts.data(), engEnts.size(), lightPtr, lightCount, capRgba, cw, ch)) {
                        s_jhEngineCaptureFailBannerShown = false;
                        // bgfx may leave the main thread without a current GL context; ImGui + preview texture need it.
                        if (!SDL_GL_MakeCurrent(window, glContext)) {
                            jhBannerViewport = std::string("Engine viewport: SDL_GL_MakeCurrent failed: ") + SDL_GetError();
                        } else if (!jackhammerEnginePreviewTex.SetSizeUpload(window, static_cast<uint32_t>(cw),
                                       static_cast<uint32_t>(ch), capRgba.data(), capRgba.size())) {
                            jhBannerViewport = "Engine viewport: preview texture upload failed or dimensions too large.";
                        } else {
                            jhBannerViewport.clear();
                        }
                    } else if (!s_jhEngineCaptureFailBannerShown) {
                        s_jhEngineCaptureFailBannerShown = true;
                        jhBannerViewport =
                            "Engine viewport: GPU capture timed out (offscreen readback). Try a smaller panel, or set "
                            "SOLSTICE_PREVIEW_POST_PRESENT_SYNC_FRAME=1 if your driver needs an extra sync frame "
                            "(may fault on some Intel iGPUs).";
                    }
                } catch (const std::bad_alloc&) {
                    jhBannerViewport = "Engine viewport: out of memory (reduce panel size).";
                } catch (const std::exception& ex) {
                    jhBannerViewport = std::string("Engine viewport: ") + ex.what();
                }
                // CaptureOrbitRgb runs bgfx (and may fail); always restore the main GL context for ImGui + user textures.
                if (!SDL_GL_MakeCurrent(window, glContext)) {
                    jhBannerViewport = std::string("Engine viewport: SDL_GL_MakeCurrent after preview failed: ") + SDL_GetError();
                }
                ImVec2 projMin = engineVp.min;
                ImVec2 projMax = engineVp.max;
                if (jackhammerEnginePreviewTex.Valid() && jackhammerEnginePreviewTex.width > 0
                    && jackhammerEnginePreviewTex.height > 0) {
                    LibUI::Viewport::ComputeLetterbox(engineVp.min, engineVp.max,
                        static_cast<float>(jackhammerEnginePreviewTex.width),
                        static_cast<float>(jackhammerEnginePreviewTex.height), projMin, projMax);
                }
                if (jackhammerEnginePreviewTex.Valid()) {
                    LibUI::Viewport::DrawTextureLetterboxed(engineVp.draw_list, jackhammerEnginePreviewTex.ImGuiTexId(),
                        engineVp.min, engineVp.max, static_cast<float>(jackhammerEnginePreviewTex.width),
                        static_cast<float>(jackhammerEnginePreviewTex.height));
                } else {
                    LibUI::Viewport::DrawCheckerboard(engineVp.draw_list, engineVp.min, engineVp.max, 14.f,
                        IM_COL32(32, 32, 42, 255), IM_COL32(24, 24, 30, 255));
                }
                LibUI::Viewport::Mat4Col viewM{};
                LibUI::Viewport::Mat4Col projM{};
                LibUI::Viewport::ComputeOrbitViewProjectionColMajor(s_engineViewportNav, 0.f, 0.f, 0.f, 55.f, aspect,
                    0.12f, 2048.f, viewM, projM);
                s_jhOrbitViewLast = viewM;
                s_jhOrbitViewValid = true;
                LibUI::Viewport::DrawXZGrid(engineVp.draw_list, projMin, projMax, viewM, projM,
                    std::max(s_jhGridCellWorld, 0.0625f), IM_COL32(72, 72, 92, 200),
                    std::clamp(s_jhGridHalfCount, 4, 128));
                if (s_jhGeoTool == 2) {
                    Jackhammer::ViewportGeo::DrawMeasureOverlay(
                        engineVp.draw_list, viewM, projM, projMin, projMax, s_jhMeasure);
                }
                if (s_jhGeoTool == 1 && s_jhBlockDragActive) {
                    const float bmnx = std::min(s_jhBlockDragX0, s_jhBlockDragX1);
                    const float bmxx = std::max(s_jhBlockDragX0, s_jhBlockDragX1);
                    const float bmnz = std::min(s_jhBlockDragZ0, s_jhBlockDragZ1);
                    const float bmxz = std::max(s_jhBlockDragZ0, s_jhBlockDragZ1);
                    LibUI::Viewport::DrawWorldAxisAlignedBoxWireframeImGui(engineVp.draw_list, projMin, projMax, viewM, projM,
                        bmnx, s_jhBlockBaseY, bmnz, bmxx, s_jhBlockBaseY + s_jhBlockHeight, bmxz, IM_COL32(120, 200, 255, 255), 1.5f);
                }
                if (s_jhOverlayOct && map.Octree.has_value() && !map.Octree->Nodes.empty()) {
                    const auto& oc = *map.Octree;
                    const int root = static_cast<int>(std::min(oc.RootIndex, static_cast<uint32_t>(oc.Nodes.size() - 1)));
                    Jackhammer::ViewportDraw::DrawOctreeTreeOverlay(engineVp.draw_list, projMin, projMax, viewM, projM, oc, root, 0,
                        s_jhOctOverlayMaxDepth, spatialOctSel);
                }
                if (s_jhOverlayBsp && map.Bsp.has_value() && !map.Bsp->Nodes.empty()) {
                    const auto& b = *map.Bsp;
                    const int root = static_cast<int>(std::min(b.RootIndex, static_cast<uint32_t>(b.Nodes.size() - 1)));
                    Jackhammer::ViewportDraw::DrawBspTreeOverlay(engineVp.draw_list, projMin, projMax, viewM, projM, b, root, 0,
                        s_jhBspOverlayMaxDepth, spatialBspSel, 2.5f);
                }
                if (s_jhOverlayLights) {
                    const size_t nLtDraw = std::min(map.AuthoringLights.size(), kJhMaxLightOverlayDraw);
                    for (size_t li = 0; li < nLtDraw; ++li) {
                        Jackhammer::ViewportDraw::DrawAuthoringLightOverlay(engineVp.draw_list, projMin, projMax, viewM, projM,
                            map.AuthoringLights[li],
                            authoringLightSel >= 0 && static_cast<size_t>(authoringLightSel) == li);
                    }
                }
                if (s_jhOverlayParticles) {
                    Jackhammer::ViewportDraw::DrawParticleEmitterMarkers(
                        engineVp.draw_list, projMin, projMax, viewM, projM, map);
                }
                if (s_showAllEntityMarkers) {
                    size_t markersDrawn = 0;
                    for (size_t ei = 0; ei < map.Entities.size(); ++ei) {
                        if (markersDrawn >= kJhMaxEntityOriginMarkers) {
                            break;
                        }
                        const SmfVec3* o = TryGetEntityOriginVec3(map.Entities[ei]);
                        if (!o) {
                            continue;
                        }
                        const bool isSel =
                            selectedEntity >= 0 && static_cast<size_t>(selectedEntity) == ei;
                        if (isSel) {
                            continue;
                        }
                        LibUI::Viewport::DrawWorldCrossXZ(engineVp.draw_list, projMin, projMax, viewM, projM,
                            o->x, o->y, o->z, 0.2f, IM_COL32(120, 120, 140, 180));
                        ++markersDrawn;
                    }
                }
                if (selectedEntity >= 0 && static_cast<size_t>(selectedEntity) < map.Entities.size()) {
                    const auto& ent = map.Entities[static_cast<size_t>(selectedEntity)];
                    const SmfVec3* origin = TryGetEntityOriginVec3(ent);
                    if (origin) {
                        LibUI::Viewport::DrawWorldCrossXZ(engineVp.draw_list, projMin, projMax, viewM, projM,
                            origin->x, origin->y, origin->z, 0.35f, IM_COL32(255, 200, 64, 255));
                        LibUI::Viewport::DrawWorldAxisAlignedBoxSelectionOutlineUniformImGui(engineVp.draw_list, projMin, projMax,
                            viewM, projM, origin->x, origin->y, origin->z, 0.42f, IM_COL32(255, 220, 90, 255), 2.0f, 2.4f,
                            IM_COL32(18, 16, 10, 250), 3.2f);
                    }
                }

                if (engineVp.hovered && ImGui::IsKeyPressed(ImGuiKey_F, false) && selectedEntity >= 0 &&
                    static_cast<size_t>(selectedEntity) < map.Entities.size()) {
                    const SmfVec3* fo = TryGetEntityOriginVec3(map.Entities[static_cast<size_t>(selectedEntity)]);
                    if (fo) {
                        LibUI::Viewport::FocusOrbitOnTarget(s_engineViewportNav, fo->x, fo->y, fo->z, 0.f, 0.f, 0.f);
                    }
                }

                if (engineVp.hovered && io.KeyCtrl && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && selectedEntity >= 0 &&
                    static_cast<size_t>(selectedEntity) < map.Entities.size() && s_jhGeoTool == 0) {
                    const ImVec2 drag = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
                    if (drag.x * drag.x + drag.y * drag.y < 25.f) {
                        float hitX = 0.f;
                        float hitZ = 0.f;
                        if (LibUI::Viewport::ScreenToXZPlane(viewM, projM, projMin, projMax,
                                ImGui::GetMousePos(), 0.f, hitX, hitZ)) {
                            if (s_placeGridSnap > 1e-6f) {
                                hitX = std::round(hitX / s_placeGridSnap) * s_placeGridSnap;
                                hitZ = std::round(hitZ / s_placeGridSnap) * s_placeGridSnap;
                            }
                            PushMapUndoSnapshot(map);
                            SmfEntity& ent = map.Entities[static_cast<size_t>(selectedEntity)];
                            const SmfVec3* cur = TryGetEntityOriginVec3(ent);
                            const float keepY = cur ? cur->y : 0.f;
                            SetEntityOriginVec3(ent, SmfVec3{hitX, keepY, hitZ});
                            dirty = true;
                        }
                    }
                }
                if (s_jhGeoTool == 1 && engineVp.hovered) {
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.KeyCtrl) {
                        float hx = 0.f;
                        float hz = 0.f;
                        if (LibUI::Viewport::ScreenToXZPlane(
                                viewM, projM, projMin, projMax, ImGui::GetMousePos(), s_jhBlockBaseY, hx, hz)) {
                            if (s_placeGridSnap > 1e-6f) {
                                hx = std::round(hx / s_placeGridSnap) * s_placeGridSnap;
                                hz = std::round(hz / s_placeGridSnap) * s_placeGridSnap;
                            }
                            s_jhBlockDragX0 = hx;
                            s_jhBlockDragZ0 = hz;
                            s_jhBlockDragX1 = hx;
                            s_jhBlockDragZ1 = hz;
                            s_jhBlockDragActive = true;
                        }
                    } else if (s_jhBlockDragActive && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                        float hx = 0.f;
                        float hz = 0.f;
                        if (LibUI::Viewport::ScreenToXZPlane(
                                viewM, projM, projMin, projMax, ImGui::GetMousePos(), s_jhBlockBaseY, hx, hz)) {
                            if (s_placeGridSnap > 1e-6f) {
                                hx = std::round(hx / s_placeGridSnap) * s_placeGridSnap;
                                hz = std::round(hz / s_placeGridSnap) * s_placeGridSnap;
                            }
                            s_jhBlockDragX1 = hx;
                            s_jhBlockDragZ1 = hz;
                        }
                    } else if (s_jhBlockDragActive && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                        s_jhBlockDragActive = false;
                        const float mnx = std::min(s_jhBlockDragX0, s_jhBlockDragX1);
                        const float mxx = std::max(s_jhBlockDragX0, s_jhBlockDragX1);
                        const float mnz = std::min(s_jhBlockDragZ0, s_jhBlockDragZ1);
                        const float mxz = std::max(s_jhBlockDragZ0, s_jhBlockDragZ1);
                        if (mxx - mnx > 0.01f && mxz - mnz > 0.01f) {
                            const float mny = s_jhBlockBaseY;
                            const float mxy = s_jhBlockBaseY + s_jhBlockHeight;
                            if (map.Bsp.has_value() && !map.Bsp->Nodes.empty()) {
                                PushMapUndoSnapshot(map);
                                auto& b = *map.Bsp;
                                const int nbi = static_cast<int>(b.Nodes.size());
                                spatialBspSel = std::clamp(spatialBspSel, 0, nbi - 1);
                                SmfAuthoringBspNode& bn = b.Nodes[static_cast<size_t>(spatialBspSel)];
                                bn.SlabMin = SmfVec3{mnx, mny, mnz};
                                bn.SlabMax = SmfVec3{mxx, mxy, mxz};
                                bn.SlabValid = true;
                                if (Jackhammer::Spatial::JhBspIsCanonicalBoxBrush(b)) {
                                    Jackhammer::Spatial::SyncJhBspBoxBrushFromAabb(b, bn.SlabMin, bn.SlabMax);
                                }
                                Jackhammer::ViewportDraw::ClearBspTextureTintCaches();
                                dirty = true;
                                status = "Block tool: set BSP node slab to drag box.";
                            } else {
                                Jackhammer::MeshOps::MakeAxisAlignedBox(
                                    s_jhMeshWorkshop, SmfVec3{mnx, mny, mnz}, SmfVec3{mxx, mxy, mxz});
                                std::snprintf(s_jhMeshWorkshopLine, sizeof(s_jhMeshWorkshopLine),
                                    "Block tool: axis-aligned box → mesh workshop (add BSP to target slab in-map).");
                            }
                        }
                    }
                }
                Jackhammer::ViewportGeo::ProcessMeasureClicks(
                    s_jhGeoTool, io, engineVp.hovered, viewM, projM, projMin, projMax, s_jhBlockBaseY, s_placeGridSnap,
                    s_jhMeasure, status);
                Jackhammer::ViewportGeo::ProcessTerrainSculpt(s_jhGeoTool, io, engineVp.hovered, viewM, projM, projMin,
                    projMax, s_jhBlockBaseY, s_placeGridSnap, s_jhTerrainBrushR, s_jhTerrainRaise, s_jhMeshWorkshop,
                    status);

                LibUI::Viewport::OrbitPanZoomParams jhOrbitOverride{};
                if (s_jhGeoTool == 1 || s_jhGeoTool == 2 || s_jhGeoTool == 3) {
                    jhOrbitOverride.orbit_mouse_button = ImGuiMouseButton_Right; // free LMB for block / measure / terrain
                }
                LibUI::Viewport::ApplyOrbitPanZoom(s_engineViewportNav, engineVp, jhOrbitOverride);
                if (engineVp.hovered && !io.WantTextInput && selectedEntity >= 0 &&
                    static_cast<size_t>(selectedEntity) < map.Entities.size()) {
                    SmfEntity& entN = map.Entities[static_cast<size_t>(selectedEntity)];
                    SmfVec3* originMut = nullptr;
                    for (auto& pr : entN.Properties) {
                        if ((pr.Key == "origin" || pr.Key == "position") && pr.Type == SmfAttributeType::Vec3) {
                            if (auto* v = std::get_if<SmfVec3>(&pr.Value)) {
                                originMut = v;
                                break;
                            }
                        }
                    }
                    if (originMut) {
                        const float step =
                            (s_placeGridSnap > 1e-6f) ? s_placeGridSnap : std::max(s_jhGridCellWorld, 0.0625f);
                        auto applyNudge = [&](float dx, float dz) {
                            PushMapUndoSnapshot(map);
                            originMut->x += dx;
                            originMut->z += dz;
                            dirty = true;
                        };
                        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false)) {
                            applyNudge(-step, 0.f);
                        }
                        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, false)) {
                            applyNudge(step, 0.f);
                        }
                        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, false)) {
                            applyNudge(0.f, step);
                        }
                        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, false)) {
                            applyNudge(0.f, -step);
                        }
                    }
                }
                const char* projName = "persp";
                switch (s_engineViewportNav.projection) {
                case LibUI::Viewport::OrbitProjectionMode::Perspective:
                    projName = "persp";
                    break;
                case LibUI::Viewport::OrbitProjectionMode::OrthoTop:
                    projName = "ortho top";
                    break;
                case LibUI::Viewport::OrbitProjectionMode::OrthoFront:
                    projName = "ortho front";
                    break;
                case LibUI::Viewport::OrbitProjectionMode::OrthoSide:
                    projName = "ortho side";
                    break;
                }
                char navBuf[192]{};
                std::snprintf(navBuf, sizeof(navBuf),
                    "%s | yaw %.2f | pitch %.2f | dist %.2f | pan %.1f, %.1f%s",
                    projName, static_cast<double>(s_engineViewportNav.yaw),
                    static_cast<double>(s_engineViewportNav.pitch), static_cast<double>(s_engineViewportNav.distance),
                    static_cast<double>(s_engineViewportNav.pan_x), static_cast<double>(s_engineViewportNav.pan_y),
                    engEntsTruncated ? " | mesh preview capped" : "");
                LibUI::Viewport::DrawViewportLabel(engineVp.draw_list, projMin, projMax, navBuf, ImVec2(0.0f, 0.0f));
                LibUI::Viewport::DrawViewportLabel(engineVp.draw_list, projMin, projMax,
                    s_jhGeoTool == 1
                        ? "Block tool: LMB drag AABB (XZ) | RMB orbit | Alt+LMB/MMB pan | wheel | F focus | arrows nudge"
                        : "LMB orbit | Alt+LMB/MMB pan | wheel zoom | F focus | Ctrl+LMB place XZ @ y=0 | arrows nudge",
                    ImVec2(0.0f, 1.0f));
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("jh_right_col", ImVec2(jhRightW, 0), true);
        if (selectedEntity >= 0 && selectedEntity < static_cast<int>(map.Entities.size())) {
            SmfEntity& ent = map.Entities[static_cast<size_t>(selectedEntity)];
            ImGui::Text("Entity %d", selectedEntity);
            char nameBuf[512];
            char classBuf[512];
            std::snprintf(nameBuf, sizeof(nameBuf), "%s", ent.Name.c_str());
            std::snprintf(classBuf, sizeof(classBuf), "%s", ent.ClassName.c_str());
            if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
                ent.Name = nameBuf;
                dirty = true;
            }
            if (ImGui::InputText("Class", classBuf, sizeof(classBuf))) {
                ent.ClassName = classBuf;
                dirty = true;
            }

            if (ent.ClassName == Jackhammer::Particles::kEmitterClassName) {
                ImGui::Separator();
                ImGui::TextUnformatted("Particle emitter");
                ImGui::TextWrapped("Viewport overlay only for now. ``particleRate`` is stored for future simulation.");
                float pr = TryGetEntityFloatByKey(ent, "particleRate", 4.f);
                if (ImGui::DragFloat("particleRate", &pr, 0.25f, 0.f, 1.0e6f)) {
                    PushMapUndoSnapshot(map);
                    SetEntityFloatByKey(ent, "particleRate", pr);
                    dirty = true;
                }
            }

            if (ent.ClassName == "Mesh" || TryGetEntityModelAssetPath(ent)) {
                ImGui::Separator();
                if (ImGui::CollapsingHeader("Mesh transform (preview cube)")) {
                    SmfVec3 sc = SmfVec3{1.f, 1.f, 1.f};
                    if (const SmfVec3* p = TryGetEntityVec3ByKey(ent, "scale")) {
                        sc = *p;
                    }
                    if (ImGui::DragFloat3("scale", &sc.x, 0.05f, 0.01f, 1.0e6f)) {
                        PushMapUndoSnapshot(map);
                        SetEntityVec3ByKey(ent, "scale", sc);
                        dirty = true;
                    }
                    float pitchDeg = TryGetEntityFloatByKey(ent, "pitch", 0.f);
                    if (ImGui::DragFloat("pitch (deg, +X)", &pitchDeg, 0.5f, -360.f, 360.f)) {
                        PushMapUndoSnapshot(map);
                        SetEntityFloatByKey(ent, "pitch", pitchDeg);
                        dirty = true;
                    }
                    float yawDeg = TryGetEntityFloatByKey(ent, "yaw", 0.f);
                    if (ImGui::DragFloat("yaw (deg, +Y)", &yawDeg, 0.5f, -360.f, 360.f)) {
                        PushMapUndoSnapshot(map);
                        SetEntityFloatByKey(ent, "yaw", yawDeg);
                        dirty = true;
                    }
                    float rollDeg = TryGetEntityFloatByKey(ent, "roll", 0.f);
                    if (ImGui::DragFloat("roll (deg, +Z)", &rollDeg, 0.5f, -360.f, 360.f)) {
                        PushMapUndoSnapshot(map);
                        SetEntityFloatByKey(ent, "roll", rollDeg);
                        dirty = true;
                    }
                }
                if (ImGui::CollapsingHeader("Mesh workshop (scratch buffer)")) {
                    ImGui::TextWrapped(
                        "In-memory indexed mesh for **JackhammerMeshOps** (sew / weld / subdivide / smooth). "
                        "Does not edit the map or glTF asset until you wire export yourself.");
                    ImGui::Separator();
                    ImGui::TextUnformatted("Arzachel prop (Edit → Add Arzachel prop) defaults");
                    ImGui::DragFloat("Uniform scale##jharz", &s_jhArzUniformScale, 0.05f, 0.01f, 1.0e6f);
                    ImGui::DragInt("Icosphere subdivision##jharz", &s_jhArzIsoSubdiv, 0.2f, 0, 8);
                    ImGui::TextDisabled("UV sphere / cylinder / torus also use the Parametric mesh sliders below.");
                    ImGui::Separator();
                    std::uint32_t mv = 0;
                    std::uint32_t mt = 0;
                    Jackhammer::MeshOps::MeshCounts(s_jhMeshWorkshop, mv, mt);
                    ImGui::Text("Scratch mesh: %u verts, %u tris", static_cast<unsigned>(mv), static_cast<unsigned>(mt));
                    if (s_jhMeshWorkshopLine[0] != '\0') {
                        ImGui::TextUnformatted(s_jhMeshWorkshopLine);
                    }
                    if (ImGui::Button("Load unit cube##jhmw")) {
                        Jackhammer::MeshOps::MakeUnitCube(s_jhMeshWorkshop);
                        std::snprintf(s_jhMeshWorkshopLine, sizeof(s_jhMeshWorkshopLine), "Mesh workshop: unit cube.");
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Load two stacked squares (sew demo)##jhmw")) {
                        Jackhammer::MeshOps::MakeTwoStackedSquaresDemo(s_jhMeshWorkshop);
                        std::snprintf(s_jhMeshWorkshopLine, sizeof(s_jhMeshWorkshopLine),
                            "Mesh workshop: two quads — use Sew demo to merge 0–3 with 4–7.");
                    }
                    ImGui::DragFloat("Vertex snap grid (0=off)##jhmwsn", &s_jhMwVertSnap, 0.01f, 0.f, 1.0e6f);
                    if (ImGui::Button("Snap all mesh vertices to grid##jhmwsa") && s_jhMwVertSnap > 1.0e-6f) {
                        Jackhammer::MeshOps::SnapAllVerticesToGrid(s_jhMeshWorkshop, s_jhMwVertSnap);
                        std::snprintf(s_jhMeshWorkshopLine, sizeof(s_jhMeshWorkshopLine), "Mesh workshop: all vertices snapped.");
                    }
                    static float s_jhMwWeldEps = 0.02f;
                    ImGui::DragFloat("Weld epsilon##jhmw", &s_jhMwWeldEps, 0.001f, 1.0e-6f, 10.f);
                    if (ImGui::Button("Weld vertices (distance)##jhmw")) {
                        const std::uint32_t rounds = Jackhammer::MeshOps::WeldVerticesByDistance(s_jhMeshWorkshop, s_jhMwWeldEps);
                        std::snprintf(s_jhMeshWorkshopLine, sizeof(s_jhMeshWorkshopLine),
                            "Mesh workshop: weld finished (%u merge rounds).", static_cast<unsigned>(rounds));
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Remove degenerate tris##jhmw")) {
                        Jackhammer::MeshOps::RemoveDegenerateTriangles(s_jhMeshWorkshop);
                        std::snprintf(s_jhMeshWorkshopLine, sizeof(s_jhMeshWorkshopLine), "Mesh workshop: removed degenerates.");
                    }
                    if (ImGui::Button("Flip winding##jhmw")) {
                        Jackhammer::MeshOps::FlipTriangleWinding(s_jhMeshWorkshop);
                        std::snprintf(s_jhMeshWorkshopLine, sizeof(s_jhMeshWorkshopLine), "Mesh workshop: flipped triangle winding.");
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Recalc normals##jhmw")) {
                        Jackhammer::MeshOps::RecalculateNormals(s_jhMeshWorkshop);
                        std::snprintf(s_jhMeshWorkshopLine, sizeof(s_jhMeshWorkshopLine), "Mesh workshop: normals recalculated.");
                    }
                    if (ImGui::Button("Subdivide (midpoint)##jhmw")) {
                        Jackhammer::MeshOps::SubdivideTrianglesMidpoint(s_jhMeshWorkshop);
                        std::snprintf(s_jhMeshWorkshopLine, sizeof(s_jhMeshWorkshopLine), "Mesh workshop: one midpoint subdiv step.");
                    }
                    static float s_jhMwScale = 1.f;
                    ImGui::DragFloat("Scale about centroid##jhmw", &s_jhMwScale, 0.02f, 0.01f, 100.f);
                    if (ImGui::Button("Apply scale##jhmw")) {
                        Jackhammer::MeshOps::ScaleMeshAboutCentroid(s_jhMeshWorkshop, s_jhMwScale);
                        std::snprintf(s_jhMeshWorkshopLine, sizeof(s_jhMeshWorkshopLine), "Mesh workshop: scaled about centroid.");
                    }
                    static float s_jhMwSmoothF = 0.25f;
                    static int s_jhMwSmoothIt = 2;
                    ImGui::DragFloat("Smooth factor##jhmw", &s_jhMwSmoothF, 0.01f, 0.f, 1.f);
                    ImGui::DragInt("Smooth iterations##jhmw", &s_jhMwSmoothIt, 1, 0, 32);
                    if (ImGui::Button("Laplacian smooth##jhmw")) {
                        Jackhammer::MeshOps::LaplacianSmoothUniform(
                            s_jhMeshWorkshop, s_jhMwSmoothF, std::max(0, s_jhMwSmoothIt));
                        std::snprintf(s_jhMeshWorkshopLine, sizeof(s_jhMeshWorkshopLine), "Mesh workshop: Laplacian smooth applied.");
                    }
                    ImGui::Separator();
                    if (ImGui::CollapsingHeader("Sub-object (vertex / edge / face)")) {
                        std::uint32_t mwNv = 0, mwNt = 0;
                        Jackhammer::MeshOps::MeshCounts(s_jhMeshWorkshop, mwNv, mwNt);
                        const int vMax = mwNv > 0 ? static_cast<int>(mwNv) - 1 : 0;
                        const int tMax = mwNt > 0 ? static_cast<int>(mwNt) - 1 : 0;
                        ImGui::Combo("Sub-object mode##jhmwsub", &s_jhMwSubMode, "Vertex\0Edge\0Face\0");
                        if (s_jhMwSubMode == 0) {
                            s_jhMwVIndex = std::clamp(s_jhMwVIndex, 0, vMax);
                            if (s_jhMwVIndex != s_jhMwVLast) {
                                if (mwNv > 0) {
                                    s_jhMwVPos = s_jhMeshWorkshop.positions[static_cast<size_t>(s_jhMwVIndex)];
                                }
                                s_jhMwVLast = s_jhMwVIndex;
                            }
                            if (ImGui::DragInt("Vertex index##jhmwvi", &s_jhMwVIndex, 0.2f, 0, vMax)) {
                                s_jhMwVLast = -1;
                            }
                            if (ImGui::Button("Reread from mesh##jhmwvr")) {
                                s_jhMwVLast = -1;
                            }
                            ImGui::SameLine();
                            ImGui::TextDisabled("edit position, then apply");
                            ImGui::DragFloat3("Position##jhmwvp", &s_jhMwVPos.x, 0.01f);
                            if (ImGui::Button("Apply to vertex##jhmwvp2") && mwNv > 0) {
                                SmfVec3 p = s_jhMwVPos;
                                if (s_jhMwVertSnap > 1.0e-6f) {
                                    const float g = s_jhMwVertSnap;
                                    p.x = std::round(p.x / g) * g;
                                    p.y = std::round(p.y / g) * g;
                                    p.z = std::round(p.z / g) * g;
                                }
                                Jackhammer::MeshOps::SetVertexPosition(
                                    s_jhMeshWorkshop, static_cast<std::uint32_t>(s_jhMwVIndex), p);
                                s_jhMwVPos = p;
                                Jackhammer::MeshOps::RecalculateNormals(s_jhMeshWorkshop);
                                std::snprintf(s_jhMeshWorkshopLine, sizeof(s_jhMeshWorkshopLine), "Vertex position updated.");
                            }
                        } else if (s_jhMwSubMode == 1) {
                            s_jhMwE0 = std::clamp(s_jhMwE0, 0, vMax);
                            s_jhMwE1 = std::clamp(s_jhMwE1, 0, vMax);
                            ImGui::DragInt("Edge v0##jhmwe0", &s_jhMwE0, 0.2f, 0, vMax);
                            ImGui::DragInt("Edge v1##jhmwe1", &s_jhMwE1, 0.2f, 0, vMax);
                            if (ImGui::Button("Collapse edge to midpoint##jhmwec")) {
                                std::string err;
                                if (Jackhammer::MeshOps::CollapseUndirectedEdge(
                                        s_jhMeshWorkshop, static_cast<std::uint32_t>(s_jhMwE0), static_cast<std::uint32_t>(s_jhMwE1), &err)) {
                                    Jackhammer::MeshOps::RecalculateNormals(s_jhMeshWorkshop);
                                    s_jhMwVLast = -1;
                                    std::snprintf(s_jhMeshWorkshopLine, sizeof(s_jhMeshWorkshopLine), "Edge collapsed.");
                                } else {
                                    std::snprintf(s_jhMeshWorkshopLine, sizeof(s_jhMeshWorkshopLine), "Collapse: %s", err.c_str());
                                }
                            }
                            if (ImGui::Button("Split edge (add midpoint)##jhmwes")) {
                                std::string err;
                                if (Jackhammer::MeshOps::SplitUndirectedEdge(
                                        s_jhMeshWorkshop, static_cast<std::uint32_t>(s_jhMwE0), static_cast<std::uint32_t>(s_jhMwE1), &err)) {
                                    s_jhMwVLast = -1;
                                    std::snprintf(s_jhMeshWorkshopLine, sizeof(s_jhMeshWorkshopLine), "Edge split (normals if present).");
                                } else {
                                    std::snprintf(s_jhMeshWorkshopLine, sizeof(s_jhMeshWorkshopLine), "Split: %s", err.c_str());
                                }
                            }
                        } else {
                            s_jhMwFaceTri = std::clamp(s_jhMwFaceTri, 0, tMax);
                            ImGui::DragInt("Triangle index##jhmwti", &s_jhMwFaceTri, 0.2f, 0, tMax);
                            if (ImGui::Button("Delete triangle##jhmwfd") && mwNt > 0) {
                                if (Jackhammer::MeshOps::DeleteTriangle(
                                        s_jhMeshWorkshop, static_cast<std::uint32_t>(s_jhMwFaceTri))) {
                                    s_jhMwVLast = -1;
                                    std::snprintf(s_jhMeshWorkshopLine, sizeof(s_jhMeshWorkshopLine), "Triangle removed (orphan verts remain).");
                                }
                            }
                        }
                    }
                    ImGui::Separator();
                    ImGui::TextUnformatted("Maya-style sew (boundary chains, same length)");
                    static int s_jhMwSewPolicy = 0; // 0=A 1=B 2=mid
                    ImGui::Combo("Sew position##jhmw", &s_jhMwSewPolicy, "Keep chain A\0Keep chain B\0Midpoint\0");
                    if (ImGui::Button("Sew demo chains (0–3 vs 4–7)##jhmw")) {
                        const Jackhammer::MeshOps::JhSewPositionPolicy pol = s_jhMwSewPolicy == 1
                            ? Jackhammer::MeshOps::JhSewPositionPolicy::KeepB
                            : (s_jhMwSewPolicy == 2 ? Jackhammer::MeshOps::JhSewPositionPolicy::Midpoint
                                                    : Jackhammer::MeshOps::JhSewPositionPolicy::KeepA);
                        const std::vector<uint32_t> ca = {0, 1, 2, 3};
                        const std::vector<uint32_t> cb = {4, 5, 6, 7};
                        std::string err;
                        if (!Jackhammer::MeshOps::SewBorderChains(s_jhMeshWorkshop, ca, cb, pol, &err)) {
                            std::snprintf(s_jhMeshWorkshopLine, sizeof(s_jhMeshWorkshopLine), "Sew: %s", err.c_str());
                        } else {
                            std::snprintf(s_jhMeshWorkshopLine, sizeof(s_jhMeshWorkshopLine),
                                "Mesh workshop: sewed demo chains (check verts/tris).");
                        }
                    }
                }
            }

            ImGui::Separator();
            if (ImGui::CollapsingHeader("External model (glTF)", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::TextWrapped(
                    "String property ``modelPath`` (or ``meshPath`` / ``gltfPath``): external .gltf/.glb mesh reference.");
                char modelBuf[1024]{};
                const char* curModel = TryGetEntityModelAssetPath(ent);
                std::snprintf(modelBuf, sizeof(modelBuf), "%s", curModel ? curModel : "");
                if (ImGui::InputText("Path##modelasset", modelBuf, sizeof(modelBuf))) {
                    PushMapUndoSnapshot(map);
                    SetEntityModelAssetPath(ent, modelBuf);
                    if (!ent.ClassName.empty() && ent.ClassName != "Mesh") {
                        ent.ClassName = "Mesh";
                    }
                    dirty = true;
                }
                if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::Import, "Browse…##modelasset")) {
                    LibUI::FileDialogs::ShowOpenFile(
                        window, "Assign glTF asset to entity",
                        [&](std::optional<std::string> path) {
                            if (!path || path->empty()) {
                                return;
                            }
                            PushMapUndoSnapshot(map);
                            SetEntityModelAssetPath(ent, ToMapRelativePathIfPossible(*path, currentPath));
                            if (!ent.ClassName.empty() && ent.ClassName != "Mesh") {
                                ent.ClassName = "Mesh";
                            }
                            dirty = true;
                        },
                        kGltfFilters);
                }
                ImGui::SameLine();
                if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::Export, "Export…##modelasset")) {
                    LibUI::FileDialogs::ShowSaveFile(
                        window, "Export selected glTF asset", [](std::optional<std::string> path) {
                            if (path) {
                                QueueGltfExportPath(std::move(*path));
                            }
                        },
                        kGltfFilters);
                }
                ImGui::SameLine();
                if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::Remove, "Clear##modelasset")) {
                    PushMapUndoSnapshot(map);
                    SetEntityModelAssetPath(ent, "");
                    dirty = true;
                }
            }

            ImGui::Separator();
            if (ImGui::CollapsingHeader("Diffuse texture", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::TextWrapped(
                    "String property ``diffuseTexture`` (or ``albedoTexture`` / ``texture``): asset path for preview tint "
                    "and ImGui thumbnail.");
                char texBuf[1024]{};
                const char* curTex = TryGetEntityDiffuseTexturePath(ent);
                std::snprintf(texBuf, sizeof(texBuf), "%s", curTex ? curTex : "");
                if (ImGui::InputText("Path##difftex", texBuf, sizeof(texBuf))) {
                    PushMapUndoSnapshot(map);
                    SetEntityDiffuseTexturePath(ent, texBuf);
                    dirty = true;
                    s_jhTextureTintCache.erase(std::string(curTex ? curTex : ""));
                }
                if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::Open, "Browse…##difftex")) {
                    LibUI::FileDialogs::ShowOpenFile(
                        window, "Diffuse texture",
                        [&](std::optional<std::string> path) {
                            if (!path || path->empty()) {
                                return;
                            }
                            PushMapUndoSnapshot(map);
                            SetEntityDiffuseTexturePath(ent, ToMapRelativePathIfPossible(*path, currentPath));
                            dirty = true;
                        },
                        std::span<const LibUI::FileDialogs::FileFilter>(kJhImageFileFilters));
                }
                ImGui::SameLine();
                if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::Remove, "Clear##difftex")) {
                    PushMapUndoSnapshot(map);
                    SetEntityDiffuseTexturePath(ent, "");
                    dirty = true;
                }
                const std::string pathForPreview = curTex ? std::string(curTex) : std::string();
                if (pathForPreview != s_jhDiffuseTexPreviewPath) {
                    s_jhDiffuseTexPreviewPath = pathForPreview;
                    if (!pathForPreview.empty()) {
                        std::vector<std::byte> rgba;
                        int tw = 0;
                        int th = 0;
                        if (LibUI::Tools::LoadImageFileToRgba8(pathForPreview, rgba, tw, th)) {
                            if (!s_jhDiffuseTexPreview.SetSizeUpload(window, static_cast<uint32_t>(tw),
                                    static_cast<uint32_t>(th), rgba.data(), rgba.size())) {
                                s_jhDiffuseTexPreview.Destroy();
                                jhBannerFile = "Diffuse preview: GL upload failed or image too large (see stderr).";
                            }
                        } else {
                            s_jhDiffuseTexPreview.Destroy();
                        }
                    } else {
                        s_jhDiffuseTexPreview.Destroy();
                    }
                }
                if (s_jhDiffuseTexPreview.Valid()) {
                    ImGui::Image(s_jhDiffuseTexPreview.ImGuiTexId(), ImVec2(160, 160));
                }
            }

            ImGui::Separator();
            if (ImGui::CollapsingHeader("Material (.smat)", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::TextWrapped(
                    "String ``materialPath`` (or ``smatPath``): Solstice Material v1 file, relative to the .smf. Used "
                    "for engine viewport preview when **Preview .smat** is on; per-entity path overrides the toolbar path.");
                char smatEntBuf[1024]{};
                const char* curSmat = TryGetEntityMaterialPath(ent);
                std::snprintf(smatEntBuf, sizeof(smatEntBuf), "%s", curSmat ? curSmat : "");
                if (ImGui::InputText("Path##smatent", smatEntBuf, sizeof(smatEntBuf))) {
                    PushMapUndoSnapshot(map);
                    SetEntityMaterialPath(ent, smatEntBuf);
                    dirty = true;
                }
                if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::Open, "Browse…##smatent")) {
                    LibUI::FileDialogs::ShowOpenFile(
                        window, "Entity material (.smat)",
                        [&](std::optional<std::string> path) {
                            if (!path || path->empty()) {
                                return;
                            }
                            PushMapUndoSnapshot(map);
                            SetEntityMaterialPath(ent, ToMapRelativePathIfPossible(*path, currentPath));
                            dirty = true;
                        },
                        kSmatFilters);
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("From preview toolbar##smatentpv")) {
                    if (s_jhPreviewSmatBuf[0] == '\0') {
                        status = "Material: set a preview .smat in the viewport toolbar first.";
                    } else {
                        PushMapUndoSnapshot(map);
                        SetEntityMaterialPath(ent, std::string(s_jhPreviewSmatBuf));
                        dirty = true;
                        status = "Material: applied toolbar preview .smat path to entity.";
                    }
                }
                ImGui::SameLine();
                if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::Remove, "Clear##smatent")) {
                    PushMapUndoSnapshot(map);
                    SetEntityMaterialPath(ent, "");
                    dirty = true;
                }
                ImGui::Separator();
                ImGui::TextUnformatted("PBR texture paths (optional)");
                char nrmBuf[1024]{};
                char rghBuf[1024]{};
                const char* curN = TryGetEntityNormalTexturePath(ent);
                const char* curR = TryGetEntityRoughnessTexturePath(ent);
                std::snprintf(nrmBuf, sizeof(nrmBuf), "%s", curN ? curN : "");
                std::snprintf(rghBuf, sizeof(rghBuf), "%s", curR ? curR : "");
                if (ImGui::InputText("Normal##nrment", nrmBuf, sizeof(nrmBuf))) {
                    PushMapUndoSnapshot(map);
                    SetEntityNormalTexturePath(ent, nrmBuf);
                    dirty = true;
                }
                ImGui::SameLine();
                if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::Open, "Browse##nrment")) {
                    LibUI::FileDialogs::ShowOpenFile(
                        window, "Normal texture",
                        [&](std::optional<std::string> path) {
                            if (!path || path->empty()) {
                                return;
                            }
                            PushMapUndoSnapshot(map);
                            SetEntityNormalTexturePath(ent, ToMapRelativePathIfPossible(*path, currentPath));
                            dirty = true;
                        },
                        kJhImageFileFilters);
                }
                if (ImGui::InputText("Roughness##rghent", rghBuf, sizeof(rghBuf))) {
                    PushMapUndoSnapshot(map);
                    SetEntityRoughnessTexturePath(ent, rghBuf);
                    dirty = true;
                }
                ImGui::SameLine();
                if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::Open, "Browse##rghent")) {
                    LibUI::FileDialogs::ShowOpenFile(
                        window, "Roughness texture",
                        [&](std::optional<std::string> path) {
                            if (!path || path->empty()) {
                                return;
                            }
                            PushMapUndoSnapshot(map);
                            SetEntityRoughnessTexturePath(ent, ToMapRelativePathIfPossible(*path, currentPath));
                            dirty = true;
                        },
                        kJhImageFileFilters);
                }
                if (ImGui::Button("Apply toolbar preview maps → entity##applypmaps")) {
                    PushMapUndoSnapshot(map);
                    if (s_jhPreviewMapAlbedo[0] != '\0') {
                        SetEntityDiffuseTexturePath(ent, std::string(s_jhPreviewMapAlbedo));
                    }
                    if (s_jhPreviewMapNormal[0] != '\0') {
                        SetEntityNormalTexturePath(ent, std::string(s_jhPreviewMapNormal));
                    }
                    if (s_jhPreviewMapRough[0] != '\0') {
                        SetEntityRoughnessTexturePath(ent, std::string(s_jhPreviewMapRough));
                    }
                    dirty = true;
                    status = "Entity: applied toolbar preview texture paths (non-empty fields only).";
                }
            }

            ImGui::Separator();
            ImGui::TextUnformatted("Properties");
            if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::New, "Add property")) {
                PushMapUndoSnapshot(map);
                ent.Properties.push_back({"newKey", SmfAttributeType::Float, SmfDefaultValueForType(SmfAttributeType::Float)});
                dirty = true;
            }

            for (size_t pi = 0; pi < ent.Properties.size(); ++pi) {
                SmfProperty& prop = ent.Properties[pi];
                ImGui::PushID(static_cast<int>(pi));
                char keyBuf[256];
                std::snprintf(keyBuf, sizeof(keyBuf), "%s", prop.Key.c_str());
                ImGui::InputText("Key", keyBuf, sizeof(keyBuf));
                if (std::string(keyBuf) != prop.Key) {
                    prop.Key = keyBuf;
                    dirty = true;
                }

                if (ImGui::BeginCombo("Type", SmfAttributeTypeLabel(prop.Type))) {
                    for (int ti = 0; ti <= 18; ++ti) {
                        const auto tt = static_cast<SmfAttributeType>(ti);
                        const bool typeMatch = (prop.Type == tt);
                        if (ImGui::Selectable(SmfAttributeTypeLabel(tt), typeMatch)) {
                            prop.Type = tt;
                            prop.Value = SmfDefaultValueForType(tt);
                            dirty = true;
                        }
                        if (typeMatch) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }

                ImGui::SameLine();
                if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::Remove, "Remove")) {
                    PushMapUndoSnapshot(map);
                    ent.Properties.erase(ent.Properties.begin() + static_cast<ptrdiff_t>(pi));
                    dirty = true;
                    ImGui::PopID();
                    break;
                }

                EditSmfPropertyValue(prop, dirty);
                ImGui::Separator();
                ImGui::PopID();
            }
        } else {
            ImGui::TextUnformatted("Select an entity or add one.");
        }

        ImGui::Separator();
        if (ImGui::CollapsingHeader("Spatial (BSP / Octree)", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::TextDisabled(
                "Viewport: toggle BSP / octree / light gizmos below. BSP textures/slab use optional BPEX tail (spatial v1).");
            if (ImGui::BeginTabBar("SpatialTabs", ImGuiTabBarFlags_None)) {
                if (ImGui::BeginTabItem("BSP")) {
                    if (ImGui::CollapsingHeader("Geometry tools (viewport + mesh workshop)")) {
                        ImGui::Combo("Active tool##jhggeom", &s_jhGeoTool,
                            "None\0"
                            "Block (LMB drag XZ, slab or workshop)\0"
                            "Measure (two XZ clicks)\0"
                            "Terrain sculpt (LMB, mesh workshop heightfield)\0");
                        ImGui::DragFloat("Block base Y / measure & terrain plane##jhgby", &s_jhBlockBaseY, 0.05f, -1.0e6f, 1.0e6f);
                        ImGui::DragFloat("Block height (Y)##jhbh", &s_jhBlockHeight, 0.05f, 0.01f, 1.0e6f);
                        if (s_jhGeoTool == 1) {
                            ImGui::TextUnformatted("LMB: drag a box on the ground plane. RMB: orbit (LMB is reserved).");
                            ImGui::TextUnformatted("With a BSP: sets the selected node slab to the box. Without BSP: mesh workshop AABB.");
                        } else if (s_jhGeoTool == 2) {
                            ImGui::TextUnformatted(
                                "Click twice on the XZ ground (Y = Block base) for end points. Third click overwrites A. **Reset** below clears.");
                            if (ImGui::Button("Reset measure A/B##jhmeasr")) {
                                s_jhMeasure = {};
                                status = "Measure: reset.";
                            }
                        } else if (s_jhGeoTool == 3) {
                            ImGui::DragFloat("Brush radius (world)##jhtbr", &s_jhTerrainBrushR, 0.02f, 0.1f, 1.0e6f);
                            ImGui::DragFloat("Raise / frame (world)##jhtrr", &s_jhTerrainRaise, 0.001f, -2.f, 2.f);
                            ImGui::TextUnformatted("LMB+drag on XZ: raise vertices in the **mesh workshop** within radius. RMB: orbit.");
                        }
                    }
                    if (ImGui::CollapsingHeader("Prefabs (`.smf` entity bundles)##jhpf")) {
                        static float s_jhPfOff[3] = {0.f, 0.f, 0.f};
                        ImGui::TextDisabled(
                            "Like SMM’s clone/duplicate workflow, but on **entities**: save a minimal `.smf` (entities only) and "
                            "append copies with an origin/position **Vec3** offset. Names are made unique on append.");
                        ImGui::DragFloat3("Append offset (added to `origin` / `position` Vec3)##jhpfo", s_jhPfOff, 0.05f);
                        if (ImGui::Button("Set offset from measure point A##jhpfms") && s_jhMeasure.hasA) {
                            s_jhPfOff[0] = s_jhMeasure.ax;
                            s_jhPfOff[1] = s_jhMeasure.ay;
                            s_jhPfOff[2] = s_jhMeasure.az;
                        }
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("Requires **Measure** tool with point A set (XZ clicks).");
                        }
                        if (ImGui::Button("Save **selected** entity to prefab .smf...##jhpfsv") && window) {
                            if (selectedEntity < 0 || static_cast<size_t>(selectedEntity) >= map.Entities.size()) {
                                status = "Prefabs: select an entity in the list first.";
                            } else {
                                LibUI::FileDialogs::ShowSaveFile(
                                    window, "Solstice map prefab (.smf)",
                                    [&](std::optional<std::string> path) {
                                        if (!path) {
                                            return;
                                        }
                                        std::string err;
                                        if (!Jackhammer::Prefabs::SaveEntitiesToSmfPrefabFile(
                                                map, {selectedEntity}, path->c_str(), err)) {
                                            status = err;
                                            return;
                                        }
                                        status = "Prefab saved: " + *path;
                                    },
                                    kSmfFilters);
                            }
                        }
                        if (ImGui::Button("Append prefab .smf into map##jhpfap") && window) {
                            LibUI::FileDialogs::ShowOpenFile(
                                window, "Open prefab (.smf) to append", [&](std::optional<std::string> path) {
                                    if (!path) {
                                        return;
                                    }
                                    std::string err;
                                    if (!Jackhammer::Prefabs::AppendPrefabSmfIntoMap(
                                            map, path->c_str(), SmfVec3{s_jhPfOff[0], s_jhPfOff[1], s_jhPfOff[2]}, err)) {
                                        status = err;
                                        return;
                                    }
                                    PushMapUndoSnapshot(map);
                                    dirty = true;
                                    status = "Prefab appended: " + *path;
                                },
                                kSmfFilters);
                        }
                    }
                    if (ImGui::CollapsingHeader("Parametric mesh primitives (→ mesh workshop)")) {
                        ImGui::TextUnformatted("Fills the mesh workshop buffer; use **Sub-object** to edit.");
                        if (ImGui::Button("Axis unit cube (±1)##jhgm")) {
                            Jackhammer::MeshOps::MakeUnitCube(s_jhMeshWorkshop);
                            std::snprintf(s_jhMeshWorkshopLine, sizeof(s_jhMeshWorkshopLine), "Primitive: unit cube.");
                        }
                        ImGui::DragFloat3("AABB min##jhabmin", &s_jhPrimAabbMin.x, 0.05f, -1.0e6f, 1.0e6f);
                        ImGui::DragFloat3("AABB max##jhabmax", &s_jhPrimAabbMax.x, 0.05f, -1.0e6f, 1.0e6f);
                        if (ImGui::Button("AABB from min/max##jhgaabb")) {
                            Jackhammer::MeshOps::MakeAxisAlignedBox(s_jhMeshWorkshop, s_jhPrimAabbMin, s_jhPrimAabbMax);
                            std::snprintf(s_jhMeshWorkshopLine, sizeof(s_jhMeshWorkshopLine), "Primitive: axis-aligned box.");
                        }
                        ImGui::DragFloat("Cylinder radius", &s_jhPrimCylR, 0.02f, 0.01f, 1.0e6f);
                        ImGui::DragFloat("Cylinder height", &s_jhPrimCylH, 0.02f, 0.01f, 1.0e6f);
                        ImGui::DragInt("Cyl. radial segs", &s_jhPrimCylRad, 0.2f, 3, 256);
                        ImGui::DragInt("Cyl. height segs", &s_jhPrimCylRing, 0.2f, 1, 64);
                        if (ImGui::Button("Build cylinder##jhgcyl")) {
                            Jackhammer::MeshOps::MakeCylinder(
                                s_jhMeshWorkshop, s_jhPrimCylR, s_jhPrimCylH, s_jhPrimCylRad, s_jhPrimCylRing);
                            std::snprintf(s_jhMeshWorkshopLine, sizeof(s_jhMeshWorkshopLine), "Primitive: cylinder.");
                        }
                        ImGui::DragFloat("Sphere radius", &s_jhPrimSphR, 0.02f, 0.01f, 1.0e6f);
                        ImGui::DragInt("Sphere lat segs", &s_jhPrimSphLa, 0.2f, 2, 256);
                        ImGui::DragInt("Sphere long segs", &s_jhPrimSphLo, 0.2f, 3, 256);
                        if (ImGui::Button("Build sphere##jhgsph")) {
                            Jackhammer::MeshOps::MakeSphere(s_jhMeshWorkshop, s_jhPrimSphR, s_jhPrimSphLa, s_jhPrimSphLo);
                            std::snprintf(s_jhMeshWorkshopLine, sizeof(s_jhMeshWorkshopLine), "Primitive: UV sphere.");
                        }
                        ImGui::DragFloat("Torus major R", &s_jhPrimTorMajor, 0.02f, 0.01f, 1.0e6f);
                        ImGui::DragFloat("Torus minor r", &s_jhPrimTorMinor, 0.02f, 0.01f, 1.0e6f);
                        ImGui::DragInt("Torus major segs", &s_jhPrimTorMaj, 0.2f, 3, 256);
                        ImGui::DragInt("Torus minor segs", &s_jhPrimTorMin, 0.2f, 3, 256);
                        if (ImGui::Button("Build torus##jhgtor")) {
                            Jackhammer::MeshOps::MakeTorus(
                                s_jhMeshWorkshop, s_jhPrimTorMajor, s_jhPrimTorMinor, s_jhPrimTorMaj, s_jhPrimTorMin);
                            std::snprintf(s_jhMeshWorkshopLine, sizeof(s_jhMeshWorkshopLine), "Primitive: torus.");
                        }
                        ImGui::DragFloat("Arch width (chord X)", &s_jhPrimArchW, 0.02f, 0.01f, 1.0e6f);
                        ImGui::DragFloat("Arch height (sagitta Y)", &s_jhPrimArchH, 0.02f, 0.01f, 1.0e6f);
                        ImGui::DragFloat("Arch depth (Z extrude)", &s_jhPrimArchD, 0.02f, 0.01f, 1.0e6f);
                        ImGui::DragInt("Arch segments (along curve)", &s_jhPrimArchSeg, 0.2f, 3, 256);
                        ImGui::DragFloat("Arch central angle (deg)", &s_jhPrimArchCurve, 0.5f, 5.f, 179.f);
                        if (ImGui::Button("Build arch (extruded)##jhgarch")) {
                            Jackhammer::MeshOps::MakeArch(s_jhMeshWorkshop, s_jhPrimArchW, s_jhPrimArchH, s_jhPrimArchD,
                                s_jhPrimArchSeg, s_jhPrimArchCurve);
                            std::snprintf(s_jhMeshWorkshopLine, sizeof(s_jhMeshWorkshopLine), "Primitive: arch (extrusion).");
                        }
                        ImGui::Separator();
                        ImGui::TextUnformatted("Heightfield (Hammer-style terrain scaffold)");
                        ImGui::DragFloat("Grid origin X##jhtox", &s_jhHfOriginX, 0.5f, -1.0e6f, 1.0e6f);
                        ImGui::DragFloat("Grid origin Z##jhtoz", &s_jhHfOriginZ, 0.5f, -1.0e6f, 1.0e6f);
                        ImGui::DragFloat("Size X##jhtsx", &s_jhHfSizeX, 0.5f, 0.1f, 1.0e6f);
                        ImGui::DragFloat("Size Z##jhtsz", &s_jhHfSizeZ, 0.5f, 0.1f, 1.0e6f);
                        ImGui::DragInt("Cells X##jhtcx", &s_jhHfCellX, 0.2f, 1, 512);
                        ImGui::DragInt("Cells Z##jhtcz", &s_jhHfCellZ, 0.2f, 1, 512);
                        ImGui::DragFloat("Base Y##jhtby", &s_jhHfBaseY, 0.05f, -1.0e6f, 1.0e6f);
                        if (ImGui::Button("Build heightfield → workshop##jhtbld")) {
                            s_jhHfCellX = std::max(1, s_jhHfCellX);
                            s_jhHfCellZ = std::max(1, s_jhHfCellZ);
                            Jackhammer::MeshOps::MakeHeightfieldGrid(s_jhMeshWorkshop, s_jhHfOriginX, s_jhHfOriginZ, s_jhHfSizeX, s_jhHfSizeZ, s_jhHfCellX, s_jhHfCellZ,
                                s_jhHfBaseY);
                            std::snprintf(s_jhMeshWorkshopLine, sizeof(s_jhMeshWorkshopLine),
                                "Heightfield: %d×%d quads — use **Terrain** sculpt or vertex tools.", s_jhHfCellX, s_jhHfCellZ);
                        }
                        ImGui::Separator();
                        ImGui::TextUnformatted("Displacement patch (2^p segments per side, Hammer-style count)");
                        ImGui::DragInt("Power p (1‥4)##jhdisp", &s_jhDispPower, 0.1f, 1, 4);
                        s_jhDispPower = std::clamp(s_jhDispPower, 1, 4);
                        if (ImGui::Button("Build displacement patch → workshop##jhdsp")) {
                            Jackhammer::MeshOps::MakeHammerDisplacementPatch(
                                s_jhMeshWorkshop, s_jhHfOriginX, s_jhHfOriginZ, s_jhHfSizeX, s_jhHfSizeZ, s_jhHfBaseY, s_jhDispPower);
                            const int segs = 1 << s_jhDispPower;
                            const int vs = segs + 1;
                            std::snprintf(s_jhMeshWorkshopLine, sizeof(s_jhMeshWorkshopLine),
                                "Displacement: %d×%d quads (%d×%d verts, power %d) — **Terrain** or vertex tools.", segs, segs,
                                vs, vs, s_jhDispPower);
                        }
                    }
                    if (!map.Bsp.has_value()) {
                        if (ImGui::Button("Create BSP")) {
                            PushMapUndoSnapshot(map);
                            SmfAuthoringBsp b;
                            SmfAuthoringBspNode root{};
                            root.PlaneNormal = SmfVec3{0.f, 1.f, 0.f};
                            root.PlaneD = 0.f;
                            root.FrontChild = -1;
                            root.BackChild = -1;
                            root.LeafId = 0xFFFFFFFFu;
                            b.Nodes.push_back(root);
                            b.RootIndex = 0;
                            map.Bsp = std::move(b);
                            dirty = true;
                            spatialBspSel = 0;
                        }
                    } else {
                        auto& b = *map.Bsp;
                        ImGui::Text("Nodes: %zu  Root: %u", b.Nodes.size(), b.RootIndex);
                        {
                            const int nbn = static_cast<int>(b.Nodes.size());
                            if (nbn > 0) {
                                const int rootIdx = static_cast<int>(
                                    std::min(b.RootIndex, static_cast<uint32_t>(std::max(nbn - 1, 0))));
                                const int hTree = Jackhammer::Spatial::JhBspSubtreeHeight(b, rootIdx, 6144);
                                ImGui::Text("Approx. tree height from root: %d", hTree);
                                if (Jackhammer::Spatial::JhBspGraphContainsDirectedCycle(b)) {
                                    ImGui::TextColored(ImVec4(1.f, 0.4f, 0.3f, 1.f),
                                        "Directed cycle in Front/Back links — graph is not a tree.");
                                }
                            }
                        }
                        if (ImGui::Button("Add node")) {
                            PushMapUndoSnapshot(map);
                            SmfAuthoringBspNode nd{};
                            nd.PlaneNormal = SmfVec3{1.f, 0.f, 0.f};
                            b.Nodes.push_back(nd);
                            dirty = true;
                            spatialBspSel = static_cast<int>(b.Nodes.size()) - 1;
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Duplicate##bspdup") && !b.Nodes.empty()) {
                            PushMapUndoSnapshot(map);
                            spatialBspSel = std::clamp(spatialBspSel, 0, static_cast<int>(b.Nodes.size()) - 1);
                            b.Nodes.push_back(b.Nodes[static_cast<size_t>(spatialBspSel)]);
                            spatialBspSel = static_cast<int>(b.Nodes.size()) - 1;
                            dirty = true;
                            status = "BSP: duplicated selected node (review child indices).";
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Remove node") && !b.Nodes.empty()) {
                            PushMapUndoSnapshot(map);
                            b.Nodes.erase(b.Nodes.begin() + static_cast<size_t>(std::clamp(spatialBspSel, 0,
                                static_cast<int>(b.Nodes.size()) - 1)));
                            if (b.Nodes.empty()) {
                                map.Bsp.reset();
                                dirty = true;
                            } else {
                                b.RootIndex = 0;
                                spatialBspSel = 0;
                                dirty = true;
                            }
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Clear BSP")) {
                            PushMapUndoSnapshot(map);
                            map.Bsp.reset();
                            dirty = true;
                        }
                        if (map.Bsp.has_value() && !b.Nodes.empty()) {
                            spatialBspSel = std::clamp(spatialBspSel, 0, static_cast<int>(b.Nodes.size()) - 1);
                            ImGui::SliderInt("Selected node", &spatialBspSel, 0, static_cast<int>(b.Nodes.size()) - 1);
                            {
                                static int s_jhBspJumpIdx = 0;
                                ImGui::InputInt("Jump to index##bspjmp", &s_jhBspJumpIdx);
                                ImGui::SameLine();
                                if (ImGui::SmallButton("Go##bspjgo")) {
                                    spatialBspSel =
                                        std::clamp(s_jhBspJumpIdx, 0, static_cast<int>(b.Nodes.size()) - 1);
                                    s_jhBspJumpIdx = spatialBspSel;
                                }
                                char idxBuf[48];
                                std::snprintf(idxBuf, sizeof(idxBuf), "%d", spatialBspSel);
                                if (LibUI::Tools::CopyTextButton("jh_bsp_idx", idxBuf, "Copy index")) {
                                    status = "BSP: copied selected node index to clipboard.";
                                }
                            }
                            ImGui::Separator();
                            ImGui::TextUnformatted("BSP box brush");
                            ImGui::TextDisabled(
                                "Builds six linked planes from an AABB. The selected slab is preserved on every face.");
                            static float s_jhBspBrushHalfExtent = 4.f;
                            ImGui::DragFloat("Box half-extent##jhboxbrush", &s_jhBspBrushHalfExtent, 0.05f, 0.01f,
                                1.0e6f);
                            if (ImGui::SmallButton("Replace BSP with origin box brush##jhboxorigin")) {
                                PushMapUndoSnapshot(map);
                                const float h = std::max(s_jhBspBrushHalfExtent, 0.01f);
                                Jackhammer::Spatial::BuildJhBspBoxBrush(b, SmfVec3{-h, -h, -h}, SmfVec3{h, h, h}, std::string(), std::string());
                                spatialBspSel = 0;
                                Jackhammer::ViewportDraw::ClearBspTextureTintCaches();
                                Jackhammer::ViewportDraw::ClearBspTextureTintCaches();
                                dirty = true;
                                status = "BSP: replaced tree with a six-plane origin box brush.";
                            }
                            ImGui::SameLine();
                            if (ImGui::SmallButton("Replace BSP with selected slab brush##jhboxslab")) {
                                const SmfAuthoringBspNode slabSrc = b.Nodes[static_cast<size_t>(spatialBspSel)];
                                if (!slabSrc.SlabValid) {
                                    status = "BSP brush: selected node needs Slab valid.";
                                } else {
                                    PushMapUndoSnapshot(map);
                                    Jackhammer::Spatial::BuildJhBspBoxBrush(b, slabSrc.SlabMin, slabSrc.SlabMax, slabSrc.FrontTexturePath,
                                        slabSrc.BackTexturePath);
                                    spatialBspSel = 0;
                                    Jackhammer::ViewportDraw::ClearBspTextureTintCaches();
                                    Jackhammer::ViewportDraw::ClearBspTextureTintCaches();
                                    dirty = true;
                                    status = "BSP: replaced tree with a six-plane brush from the selected slab.";
                                }
                            }
                            ImGui::Separator();
                            SmfAuthoringBspNode& nd = b.Nodes[static_cast<size_t>(spatialBspSel)];
                            const bool bspIsBoxBrush = Jackhammer::Spatial::JhBspIsCanonicalBoxBrush(b);
                            ImGui::Text("Selected face/node: %s", Jackhammer::Spatial::JhBspFaceLabel(nd));
                            if (bspIsBoxBrush) {
                                ImGui::SameLine();
                                ImGui::TextDisabled("(box brush)");
                            }
                            {
                                const int pNav = Jackhammer::Spatial::FindBspParentIndex(b, spatialBspSel);
                                ImGui::BeginDisabled(pNav < 0);
                                if (ImGui::SmallButton("Go to parent##bsppar")) {
                                    spatialBspSel = pNav;
                                }
                                ImGui::EndDisabled();
                                ImGui::SameLine();
                                ImGui::BeginDisabled(nd.FrontChild < 0
                                    || nd.FrontChild >= static_cast<int>(b.Nodes.size()));
                                if (ImGui::SmallButton("Front child##bspfch")) {
                                    spatialBspSel = nd.FrontChild;
                                }
                                ImGui::EndDisabled();
                                ImGui::SameLine();
                                ImGui::BeginDisabled(
                                    nd.BackChild < 0 || nd.BackChild >= static_cast<int>(b.Nodes.size()));
                                if (ImGui::SmallButton("Back child##bspbch")) {
                                    spatialBspSel = nd.BackChild;
                                }
                                ImGui::EndDisabled();
                            }
                            if (ImGui::DragFloat3("Plane N", &nd.PlaneNormal.x, 0.01f)) {
                                dirty = true;
                            }
                            ImGui::SameLine();
                            if (ImGui::SmallButton("Normalize N##bspnorm")) {
                                PushMapUndoSnapshot(map);
                                nd.PlaneNormal = Jackhammer::Spatial::NormalizeSmfVec3(nd.PlaneNormal);
                                dirty = true;
                            }
                            if (ImGui::DragFloat("Plane D", &nd.PlaneD, 0.01f)) {
                                dirty = true;
                            }
                            if (ImGui::SmallButton("Flip plane (+ swap front/back tex)##bspflip")) {
                                PushMapUndoSnapshot(map);
                                nd.PlaneNormal.x *= -1.f;
                                nd.PlaneNormal.y *= -1.f;
                                nd.PlaneNormal.z *= -1.f;
                                nd.PlaneD *= -1.f;
                                std::swap(nd.FrontTexturePath, nd.BackTexturePath);
                                Jackhammer::ViewportDraw::ClearBspTextureTintCaches();
                                Jackhammer::ViewportDraw::ClearBspTextureTintCaches();
                                dirty = true;
                                status = "BSP: plane flipped; front/back textures swapped.";
                            }
                            static float s_jhBspPlaneDnudge = 0.25f;
                            ImGui::DragFloat("Plane D nudge step##bspdnud", &s_jhBspPlaneDnudge, 0.01f, 0.001f, 256.f);
                            ImGui::BeginDisabled(bspIsBoxBrush);
                            if (ImGui::SmallButton("D −##bspdm")) {
                                PushMapUndoSnapshot(map);
                                nd.PlaneD -= std::max(s_jhBspPlaneDnudge, 0.f);
                                dirty = true;
                                status = "BSP: nudged plane D.";
                            }
                            ImGui::SameLine();
                            if (ImGui::SmallButton("D +##bspdp")) {
                                PushMapUndoSnapshot(map);
                                nd.PlaneD += std::max(s_jhBspPlaneDnudge, 0.f);
                                dirty = true;
                                status = "BSP: nudged plane D.";
                            }
                            ImGui::SameLine();
                            if (ImGui::SmallButton("Rot N +90° Y##bspny")) {
                                PushMapUndoSnapshot(map);
                                const float nx = nd.PlaneNormal.x;
                                const float nz = nd.PlaneNormal.z;
                                nd.PlaneNormal.x = -nz;
                                nd.PlaneNormal.z = nx;
                                nd.PlaneNormal = Jackhammer::Spatial::NormalizeSmfVec3(nd.PlaneNormal);
                                dirty = true;
                                status = "BSP: rotated plane normal +90° about Y.";
                            }
                            ImGui::SameLine();
                            if (ImGui::SmallButton("Rot N +90° X##bspnx")) {
                                PushMapUndoSnapshot(map);
                                const float ny = nd.PlaneNormal.y;
                                const float nz = nd.PlaneNormal.z;
                                nd.PlaneNormal.y = -nz;
                                nd.PlaneNormal.z = ny;
                                nd.PlaneNormal = Jackhammer::Spatial::NormalizeSmfVec3(nd.PlaneNormal);
                                dirty = true;
                                status = "BSP: rotated plane normal +90° about X.";
                            }
                            ImGui::EndDisabled();
                            if (bspIsBoxBrush) {
                                ImGui::TextDisabled(
                                    "D nudge / normal rotate: disabled for canonical box brush (keeps opposing faces aligned).");
                                static float s_jhBoxFaceDelta = 0.25f;
                                ImGui::DragFloat("Push box face by##jhboxfd", &s_jhBoxFaceDelta, 0.01f, 0.001f, 256.f);
                                auto doPush = [&](int faceIdx) {
                                    PushMapUndoSnapshot(map);
                                    if (Jackhammer::Spatial::PushCanonicalBoxBrushFace(b, faceIdx, s_jhBoxFaceDelta)) {
                                        dirty = true;
                                        Jackhammer::ViewportDraw::ClearBspTextureTintCaches();
                                        status = "BSP: moved canonical box face.";
                                    } else {
                                        status = "BSP: face push rejected (delta too large or not a box brush?).";
                                    }
                                };
                                if (ImGui::SmallButton("+X##jhbf0")) {
                                    doPush(0);
                                }
                                ImGui::SameLine();
                                if (ImGui::SmallButton("-X##jhbf1")) {
                                    doPush(1);
                                }
                                ImGui::SameLine();
                                if (ImGui::SmallButton("+Y##jhbf2")) {
                                    doPush(2);
                                }
                                ImGui::SameLine();
                                if (ImGui::SmallButton("-Y##jhbf3")) {
                                    doPush(3);
                                }
                                ImGui::SameLine();
                                if (ImGui::SmallButton("+Z##jhbf4")) {
                                    doPush(4);
                                }
                                ImGui::SameLine();
                                if (ImGui::SmallButton("-Z##jhbf5")) {
                                    doPush(5);
                                }
                            }
                            if (ImGui::DragInt("Front child", &nd.FrontChild, 1, -1, static_cast<int>(b.Nodes.size()) - 1)) {
                                dirty = true;
                            }
                            if (ImGui::DragInt("Back child", &nd.BackChild, 1, -1, static_cast<int>(b.Nodes.size()) - 1)) {
                                dirty = true;
                            }
                            if (ImGui::SmallButton("Swap front/back child indices##bspchsw")) {
                                PushMapUndoSnapshot(map);
                                std::swap(nd.FrontChild, nd.BackChild);
                                dirty = true;
                            }
                            if (nd.FrontChild < 0 && nd.BackChild < 0) {
                                if (ImGui::Button("Create front & back child nodes##bspnewch")) {
                                    PushMapUndoSnapshot(map);
                                    const int parentSel = spatialBspSel;
                                    SmfAuthoringBspNode f{};
                                    f.PlaneNormal = SmfVec3{1.f, 0.f, 0.f};
                                    f.PlaneD = 0.f;
                                    f.FrontChild = -1;
                                    f.BackChild = -1;
                                    f.LeafId = 0xFFFFFFFFu;
                                    b.Nodes.push_back(f);
                                    SmfAuthoringBspNode bk{};
                                    bk.PlaneNormal = SmfVec3{1.f, 0.f, 0.f};
                                    bk.PlaneD = 0.f;
                                    bk.FrontChild = -1;
                                    bk.BackChild = -1;
                                    bk.LeafId = 0xFFFFFFFFu;
                                    b.Nodes.push_back(bk);
                                    const int fi = static_cast<int>(b.Nodes.size()) - 2;
                                    const int bi = static_cast<int>(b.Nodes.size()) - 1;
                                    b.Nodes[static_cast<size_t>(parentSel)].FrontChild = fi;
                                    b.Nodes[static_cast<size_t>(parentSel)].BackChild = bi;
                                    spatialBspSel = fi;
                                    dirty = true;
                                    status = "BSP: empty child nodes allocated; edit planes as needed.";
                                }
                            } else {
                                ImGui::TextDisabled("Set both child indices to -1 to allocate new child nodes.");
                            }
                            int leaf = static_cast<int>(nd.LeafId);
                            if (ImGui::DragInt("Leaf id (use -1 for none)", &leaf, 1, -1, 1 << 30)) {
                                nd.LeafId = leaf < 0 ? 0xFFFFFFFFu : static_cast<uint32_t>(leaf);
                                dirty = true;
                            }
                            if (ImGui::Button("Copy plane + slab as text##bspcopy")) {
                                const std::string txt = Jackhammer::Spatial::JhExportBspNodeText(nd, spatialBspSel);
                                ImGui::SetClipboardText(txt.c_str());
                                status = "BSP: plane/slab export copied to clipboard.";
                            }
                            ImGui::Separator();
                            ImGui::TextUnformatted("Textures & Hammer-style plane tools");
                            char bspFrontBuf[2048];
                            char bspBackBuf[2048];
                            std::snprintf(bspFrontBuf, sizeof(bspFrontBuf), "%s", nd.FrontTexturePath.c_str());
                            std::snprintf(bspBackBuf, sizeof(bspBackBuf), "%s", nd.BackTexturePath.c_str());
                            if (ImGui::InputText("Front (+N half-space)", bspFrontBuf, sizeof(bspFrontBuf))) {
                                nd.FrontTexturePath = bspFrontBuf;
                                Jackhammer::ViewportDraw::ClearBspTextureTintCaches();
                                dirty = true;
                            }
                            ImGui::SameLine();
                            if (ImGui::SmallButton("Browse##bspft")) {
                                const int bspSelSnap = spatialBspSel;
                                LibUI::FileDialogs::ShowOpenFile(
                                    window, "Front BSP texture", [&, bspSelSnap](std::optional<std::string> path) {
                                        if (!path || !map.Bsp.has_value()) {
                                            return;
                                        }
                                        PushMapUndoSnapshot(map);
                                        map.Bsp->Nodes[static_cast<size_t>(bspSelSnap)].FrontTexturePath =
                                            ToMapRelativePathIfPossible(*path, currentPath);
                                        Jackhammer::ViewportDraw::ClearBspTextureTintCaches();
                                        dirty = true;
                                    },
                                    kJhImageFileFilters);
                            }
                            if (ImGui::InputText("Back (−N half-space)", bspBackBuf, sizeof(bspBackBuf))) {
                                nd.BackTexturePath = bspBackBuf;
                                Jackhammer::ViewportDraw::ClearBspTextureTintCaches();
                                dirty = true;
                            }
                            ImGui::SameLine();
                            if (ImGui::SmallButton("Browse##bspbt")) {
                                const int bspSelSnap = spatialBspSel;
                                LibUI::FileDialogs::ShowOpenFile(
                                    window, "Back BSP texture", [&, bspSelSnap](std::optional<std::string> path) {
                                        if (!path || !map.Bsp.has_value()) {
                                            return;
                                        }
                                        PushMapUndoSnapshot(map);
                                        map.Bsp->Nodes[static_cast<size_t>(bspSelSnap)].BackTexturePath =
                                            ToMapRelativePathIfPossible(*path, currentPath);
                                        Jackhammer::ViewportDraw::ClearBspTextureTintCaches();
                                        dirty = true;
                                    },
                                    kJhImageFileFilters);
                            }
                            static bool s_jhApplyFaceImageToBackSide = false;
                            ImGui::Checkbox("Also set back side##jhfaceimgback", &s_jhApplyFaceImageToBackSide);
                            if (ImGui::SmallButton("Apply arbitrary image to selected face##jhfaceimg")) {
                                const int bspSelSnap = spatialBspSel;
                                const bool alsoBack = s_jhApplyFaceImageToBackSide;
                                LibUI::FileDialogs::ShowOpenFile(
                                    window, "Apply image to selected BSP face", [&, bspSelSnap, alsoBack](std::optional<std::string> path) {
                                        if (!path || !map.Bsp.has_value() || bspSelSnap < 0
                                            || static_cast<size_t>(bspSelSnap) >= map.Bsp->Nodes.size()) {
                                            return;
                                        }
                                        PushMapUndoSnapshot(map);
                                        const std::string tex = ToMapRelativePathIfPossible(*path, currentPath);
                                        SmfAuthoringBspNode& face = map.Bsp->Nodes[static_cast<size_t>(bspSelSnap)];
                                        face.FrontTexturePath = tex;
                                        if (alsoBack) {
                                            face.BackTexturePath = tex;
                                        }
                                        Jackhammer::ViewportDraw::ClearBspTextureTintCaches();
                                        Jackhammer::ViewportDraw::ClearBspTextureTintCaches();
                                        dirty = true;
                                    },
                                    kJhImageFileFilters);
                            }
                            ImGui::SameLine();
                            if (ImGui::SmallButton("Apply image to ALL faces##jhfaceimgall")) {
                                const bool alsoBack = s_jhApplyFaceImageToBackSide;
                                LibUI::FileDialogs::ShowOpenFile(
                                    window, "Apply image to all BSP faces", [&, alsoBack](std::optional<std::string> path) {
                                        if (!path || !map.Bsp.has_value()) {
                                            return;
                                        }
                                        PushMapUndoSnapshot(map);
                                        const std::string tex = ToMapRelativePathIfPossible(*path, currentPath);
                                        for (auto& face : map.Bsp->Nodes) {
                                            face.FrontTexturePath = tex;
                                            if (alsoBack) {
                                                face.BackTexturePath = tex;
                                            }
                                        }
                                        Jackhammer::ViewportDraw::ClearBspTextureTintCaches();
                                        Jackhammer::ViewportDraw::ClearBspTextureTintCaches();
                                        dirty = true;
                                        status = "BSP: image applied to all faces.";
                                    },
                                    kJhImageFileFilters);
                            }
                            if (ImGui::SmallButton("Front ← selected entity diffuse")) {
                                if (selectedEntity >= 0 && static_cast<size_t>(selectedEntity) < map.Entities.size()) {
                                    if (const char* dp = TryGetEntityDiffuseTexturePath(
                                            map.Entities[static_cast<size_t>(selectedEntity)])) {
                                        PushMapUndoSnapshot(map);
                                        nd.FrontTexturePath = dp;
                                        Jackhammer::ViewportDraw::ClearBspTextureTintCaches();
                                        dirty = true;
                                    }
                                }
                            }
                            if (ImGui::SmallButton("Both sides ← entity diffuse##bspboth")) {
                                if (selectedEntity >= 0 && static_cast<size_t>(selectedEntity) < map.Entities.size()) {
                                    if (const char* dp = TryGetEntityDiffuseTexturePath(
                                            map.Entities[static_cast<size_t>(selectedEntity)])) {
                                        PushMapUndoSnapshot(map);
                                        nd.FrontTexturePath = dp;
                                        nd.BackTexturePath = dp;
                                        Jackhammer::ViewportDraw::ClearBspTextureTintCaches();
                                        Jackhammer::ViewportDraw::ClearBspTextureTintCaches();
                                        dirty = true;
                                    }
                                }
                            }
                            if (ImGui::SmallButton("Swap front/back##bspsw")) {
                                PushMapUndoSnapshot(map);
                                std::swap(nd.FrontTexturePath, nd.BackTexturePath);
                                Jackhammer::ViewportDraw::ClearBspTextureTintCaches();
                                Jackhammer::ViewportDraw::ClearBspTextureTintCaches();
                                dirty = true;
                            }
                            ImGui::SameLine();
                            if (ImGui::SmallButton("Copy front → back##bspcfb")) {
                                PushMapUndoSnapshot(map);
                                nd.BackTexturePath = nd.FrontTexturePath;
                                Jackhammer::ViewportDraw::ClearBspTextureTintCaches();
                                dirty = true;
                            }
                            ImGui::SameLine();
                            if (ImGui::SmallButton("Copy back → front##bspcbf")) {
                                PushMapUndoSnapshot(map);
                                nd.FrontTexturePath = nd.BackTexturePath;
                                Jackhammer::ViewportDraw::ClearBspTextureTintCaches();
                                dirty = true;
                            }
                            if (ImGui::Button("Set ALL nodes' front texture to this node's front##bspallf")) {
                                PushMapUndoSnapshot(map);
                                const std::string t = nd.FrontTexturePath;
                                for (auto& node : b.Nodes) {
                                    node.FrontTexturePath = t;
                                }
                                Jackhammer::ViewportDraw::ClearBspTextureTintCaches();
                                dirty = true;
                                status = "BSP: front texture copied to every node.";
                            }
                            if (ImGui::CollapsingHeader("Texture alignment (fit / center / rotate)##jhbtexalign")) {
                                static int s_jhTexAlignSide = 0;
                                ImGui::Combo("Face##jhtaside", &s_jhTexAlignSide, "Front (+N)\0Back (-N)\0");
                                Solstice::Smf::SmfBspFaceTextureXform* xf = s_jhTexAlignSide == 0
                                    ? &nd.FrontTextureXform
                                    : &nd.BackTextureXform;
                                bool* xfOn = s_jhTexAlignSide == 0 ? &nd.HasFrontTextureXform : &nd.HasBackTextureXform;
                                bool* xfLock = s_jhTexAlignSide == 0 ? &nd.FrontTextureXformLocked : &nd.BackTextureXformLocked;
                                if (ImGui::Checkbox("Lock this face’s UV / scale (authoring)##jhtxlk", xfLock)) {
                                    PushMapUndoSnapshot(map);
                                    dirty = true;
                                }
                                if (ImGui::DragFloat2("Shift UV##jhtash", &xf->ShiftU, 0.005f, -8.f, 8.f)) {
                                    PushMapUndoSnapshot(map);
                                    *xfOn = true;
                                    dirty = true;
                                }
                                if (ImGui::DragFloat2("Scale UV##jhtasc", &xf->ScaleU, 0.01f, 0.001f, 1024.f)) {
                                    PushMapUndoSnapshot(map);
                                    *xfOn = true;
                                    dirty = true;
                                }
                                if (ImGui::DragFloat("Rotate (deg)##jhtarot", &xf->RotateDeg, 0.25f, -360.f, 360.f)) {
                                    PushMapUndoSnapshot(map);
                                    *xfOn = true;
                                    dirty = true;
                                }
                                const bool xformLocked = *xfLock;
                                if (xformLocked) {
                                    ImGui::TextUnformatted(
                                        "UV lock is on: manual sliders still apply; one-click fit / auto-aligns are disabled.");
                                }
                                static float s_jhTexUniformScaleMul = 1.f;
                                ImGui::DragFloat("Uniform scale multiply (one click)##jhtum", &s_jhTexUniformScaleMul, 0.02f, 0.01f, 100.f);
                                if (xformLocked) {
                                    ImGui::BeginDisabled();
                                }
                                if (ImGui::Button("Apply scale multiply##jhtumbtn")) {
                                    PushMapUndoSnapshot(map);
                                    const float f = std::max(0.01f, s_jhTexUniformScaleMul);
                                    xf->ScaleU *= f;
                                    xf->ScaleV *= f;
                                    *xfOn = true;
                                    dirty = true;
                                }
                                static int s_jhTexWorldAlignAxis = 0;
                                ImGui::Combo("World align axis##jhtwax", &s_jhTexWorldAlignAxis, "+X\0+Y\0+Z\0");
                                if (ImGui::Button("Align to world (project axis)##jhtaworld")) {
                                    PushMapUndoSnapshot(map);
                                    SmfVec3 ax{1.f, 0.f, 0.f};
                                    if (s_jhTexWorldAlignAxis == 1) {
                                        ax = {0.f, 1.f, 0.f};
                                    } else if (s_jhTexWorldAlignAxis == 2) {
                                        ax = {0.f, 0.f, 1.f};
                                    }
                                    Jackhammer::BspTex::ApplyTextureAlignToWorld(*xf, nd, s_jhTexAlignSide, ax);
                                    *xfOn = true;
                                    dirty = true;
                                }
                                if (ImGui::IsItemHovered()) {
                                    ImGui::SetTooltip("Sets UV rotation so the selected world axis, projected into the face plane, is "
                                                      "“horizontal” in texture space (see Face combo).");
                                }
                                if (ImGui::Button("Align to default / face (centered unit)##jhtaface")) {
                                    PushMapUndoSnapshot(map);
                                    Jackhammer::BspTex::ApplyTextureAlignToDefaultFace(*xf);
                                    *xfOn = true;
                                    dirty = true;
                                }
                                if (s_jhOrbitViewValid) {
                                    if (ImGui::Button("Align to current engine view##jhtavw")) {
                                        PushMapUndoSnapshot(map);
                                        Jackhammer::BspTex::ApplyTextureAlignToView(
                                            *xf, nd, s_jhTexAlignSide, s_jhOrbitViewLast.data());
                                        *xfOn = true;
                                        dirty = true;
                                    }
                                } else {
                                    ImGui::TextDisabled("Render the engine view once to enable “align to view”.");
                                }
                                if (xformLocked) {
                                    ImGui::EndDisabled();
                                }
                                static float s_jhTexWorldPerRepeat = 64.f;
                                ImGui::DragFloat("World units / texture repeat (fit)##jhtawr", &s_jhTexWorldPerRepeat, 0.25f, 1.0e-3f,
                                    1.0e6f);
                                if (xformLocked) {
                                    ImGui::BeginDisabled();
                                }
                                if (ImGui::Button("Fit scale to slab face##jhtafit")) {
                                    float fw = 0.f;
                                    float fh = 0.f;
                                    if (!Jackhammer::Spatial::JhBspSlabFaceWorldSize(nd, fw, fh)) {
                                        status = "BSP texture: need valid slab + axis-aligned plane for fit.";
                                    } else {
                                        PushMapUndoSnapshot(map);
                                        const float wpr = std::max(s_jhTexWorldPerRepeat, 1.0e-6f);
                                        xf->ScaleU = fw / wpr;
                                        xf->ScaleV = fh / wpr;
                                        *xfOn = true;
                                        dirty = true;
                                        status = "BSP texture: scale set from slab face size.";
                                    }
                                }
                                ImGui::SameLine();
                                if (ImGui::SmallButton("Center shift##jhtacent")) {
                                    PushMapUndoSnapshot(map);
                                    xf->ShiftU = 0.5f;
                                    xf->ShiftV = 0.5f;
                                    *xfOn = true;
                                    dirty = true;
                                    status = "BSP texture: shift centered (0.5, 0.5).";
                                }
                                ImGui::SameLine();
                                if (ImGui::SmallButton("+90°##jhta90")) {
                                    PushMapUndoSnapshot(map);
                                    xf->RotateDeg = std::fmod(xf->RotateDeg + 90.f, 360.f);
                                    *xfOn = true;
                                    dirty = true;
                                }
                                if (ImGui::SmallButton("Reset alignment##jhtaz")) {
                                    PushMapUndoSnapshot(map);
                                    *xf = Solstice::Smf::SmfBspFaceTextureXform{};
                                    *xfOn = false;
                                    dirty = true;
                                    status = "BSP texture: alignment reset.";
                                }
                                if (xformLocked) {
                                    ImGui::EndDisabled();
                                }
                                ImGui::TextDisabled(
                                    "Stored per node in .smf (BXT1). Runtime preview may ignore UV until the engine reads it.");
                            }
                            if (ImGui::CollapsingHeader("Baked lightmap (selected BSP face)##jhblm")) {
                                static int s_jhLmRes = 256;
                                if (ImGui::InputInt("Output resolution (square, pixels)##jhlmres", &s_jhLmRes)) {
                                    s_jhLmRes = std::clamp(s_jhLmRes, 4, 2048);
                                }
                                s_jhLmRes = std::clamp(s_jhLmRes, 4, 2048);
                                ImGui::ColorEdit3("Ambient (linear, added before encode)##jhlmamb", s_jhLmAmb);
                                ImGui::Combo("Face##jhlmfs", &s_jhLmFaceSide, "Front (+N / plane outward)\0Back (−N)\0");
                                ImGui::Checkbox("Model spot light cones (inner/outer)##jhlmspot", &s_jhLmModelSpot);
                                {
                                    char lmPathBuf[1024]{};
                                    std::snprintf(lmPathBuf, sizeof(lmPathBuf), "%s", map.BakedLightmapPath.c_str());
                                    if (ImGui::InputText("Map-relative path stored in .smf (BKM1)##jhlmp", lmPathBuf,
                                            sizeof(lmPathBuf))) {
                                        PushMapUndoSnapshot(map);
                                        map.BakedLightmapPath = lmPathBuf;
                                        dirty = true;
                                    }
                                }
                                ImGui::TextDisabled(
                                    "Bakes the selected node’s slab in world u×v: N·L (face side above), point falloff, optional "
                                    "spot cones, plus ambient. Axis-aligned only. Use “Bake…” to pick a .png; path is stored "
                                    "relative to the .smf when possible.");
                                if (ImGui::Button("Bake selected face to PNG and set BKM1 path…##jhlmgo")) {
                                    const int bakeRes = s_jhLmRes;
                                    Jackhammer::Lightmap::LightmapBakeOptions lmOpt{};
                                    lmOpt.ambientR = s_jhLmAmb[0];
                                    lmOpt.ambientG = s_jhLmAmb[1];
                                    lmOpt.ambientB = s_jhLmAmb[2];
                                    lmOpt.faceSide = s_jhLmFaceSide;
                                    lmOpt.modelSpotCones = s_jhLmModelSpot;
                                    LibUI::FileDialogs::ShowSaveFile(
                                        window, "Save baked lightmap (.png)",
                                        [&, bakeRes, lmOpt](std::optional<std::string> path) {
                                            if (!path || path->empty()) {
                                                return;
                                            }
                                            std::string err;
                                            if (!Jackhammer::Lightmap::BakeSimpleBspFaceLightmapPng(
                                                    map, spatialBspSel, bakeRes, *path, err, &lmOpt)) {
                                                status = err;
                                                return;
                                            }
                                            PushMapUndoSnapshot(map);
                                            map.BakedLightmapPath = ToMapRelativePathIfPossible(*path, currentPath);
                                            dirty = true;
                                            status = "Baked lightmap: " + map.BakedLightmapPath;
                                        },
                                        kJhImageFileFilters);
                                }
                            }
                            if (ImGui::CollapsingHeader("Face texture paint (2D)##jhtpaint")) {
                                static Jackhammer::TexturePaint::RgbaCanvas s_jhFpCanvas{};
                                static int s_jhFpNode = -1000000000;
                                static int s_jhFpSide = -1;
                                static int s_jhFpTarget = 0;
                                static LibUI::Graphics::PreviewTextureRgba s_jhFpGpu{};
                                static float s_jhFpBrushRadius = 0.04f;
                                static float s_jhFpHardness = 0.55f;
                                static float s_jhFpColor[4] = {0.85f, 0.2f, 0.15f, 1.f};
                                static bool s_jhFpErase = false;
                                ImGui::Combo("Paint target##jhfptgt", &s_jhFpTarget, "Front (+N)\0Back (-N)\0");
                                const std::string relTex =
                                    s_jhFpTarget == 0 ? nd.FrontTexturePath : nd.BackTexturePath;
                                const std::string resolvedTex = JhResolveMapAssetPath(currentPath, relTex);
                                const bool needReload =
                                    spatialBspSel != s_jhFpNode || s_jhFpTarget != s_jhFpSide;
                                if (needReload) {
                                    s_jhFpNode = spatialBspSel;
                                    s_jhFpSide = s_jhFpTarget;
                                    std::string err;
                                    (void)Jackhammer::TexturePaint::LoadOrCreateCanvas(s_jhFpCanvas, resolvedTex, 256, err);
                                    if (!err.empty()) {
                                        status = err;
                                    }
                                    s_jhFpGpu.Destroy();
                                    if (s_jhFpCanvas.width > 0 && s_jhFpCanvas.height > 0
                                        && !s_jhFpCanvas.rgba.empty()) {
                                        (void)s_jhFpGpu.SetSizeUpload(window, static_cast<uint32_t>(s_jhFpCanvas.width),
                                            static_cast<uint32_t>(s_jhFpCanvas.height), s_jhFpCanvas.rgba.data(),
                                            s_jhFpCanvas.rgba.size());
                                    }
                                }
                                ImGui::SliderFloat("Brush radius", &s_jhFpBrushRadius, 0.005f, 0.45f, "%.3f");
                                ImGui::SliderFloat("Hardness", &s_jhFpHardness, 0.f, 1.f, "%.2f");
                                ImGui::ColorEdit4("Brush color", s_jhFpColor,
                                    ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf);
                                ImGui::Checkbox("Erase (reduce alpha)", &s_jhFpErase);
                                if (ImGui::Button("Reload from disk##jhfpre")) {
                                    std::string err;
                                    (void)Jackhammer::TexturePaint::LoadOrCreateCanvas(s_jhFpCanvas, resolvedTex, 256, err);
                                    if (!err.empty()) {
                                        status = err;
                                    }
                                    s_jhFpGpu.Destroy();
                                    if (s_jhFpCanvas.width > 0 && !s_jhFpCanvas.rgba.empty()) {
                                        (void)s_jhFpGpu.SetSizeUpload(window, static_cast<uint32_t>(s_jhFpCanvas.width),
                                            static_cast<uint32_t>(s_jhFpCanvas.height), s_jhFpCanvas.rgba.data(),
                                            s_jhFpCanvas.rgba.size());
                                    }
                                }
                                ImGui::SameLine();
                                if (ImGui::Button("New blank 256²##jhfpnw")) {
                                    s_jhFpCanvas.rgba.assign(256u * 256u * 4u, std::byte{255});
                                    for (int y = 0; y < 256; ++y) {
                                        for (int x = 0; x < 256; ++x) {
                                            const size_t i = (static_cast<size_t>(y) * 256u + static_cast<size_t>(x)) * 4u;
                                            s_jhFpCanvas.rgba[i + 0] = std::byte{200};
                                            s_jhFpCanvas.rgba[i + 1] = std::byte{200};
                                            s_jhFpCanvas.rgba[i + 2] = std::byte{205};
                                            s_jhFpCanvas.rgba[i + 3] = std::byte{255};
                                        }
                                    }
                                    s_jhFpCanvas.width = s_jhFpCanvas.height = 256;
                                    s_jhFpGpu.Destroy();
                                    (void)s_jhFpGpu.SetSizeUpload(window, 256, 256, s_jhFpCanvas.rgba.data(),
                                        s_jhFpCanvas.rgba.size());
                                }
                                ImGui::SameLine();
                                if (ImGui::Button("Fill brush color##jhfpfl")) {
                                    Jackhammer::TexturePaint::FillSolid(s_jhFpCanvas,
                                        static_cast<uint8_t>(std::clamp(s_jhFpColor[0], 0.f, 1.f) * 255.f),
                                        static_cast<uint8_t>(std::clamp(s_jhFpColor[1], 0.f, 1.f) * 255.f),
                                        static_cast<uint8_t>(std::clamp(s_jhFpColor[2], 0.f, 1.f) * 255.f),
                                        static_cast<uint8_t>(std::clamp(s_jhFpColor[3], 0.f, 1.f) * 255.f));
                                    (void)s_jhFpGpu.SetSizeUpload(window, static_cast<uint32_t>(s_jhFpCanvas.width),
                                        static_cast<uint32_t>(s_jhFpCanvas.height), s_jhFpCanvas.rgba.data(),
                                        s_jhFpCanvas.rgba.size());
                                }
                                const ImVec2 canvasSz(320.f, 320.f);
                                const ImVec2 canvas0 = ImGui::GetCursorScreenPos();
                                ImGui::InvisibleButton("jhfpcan", canvasSz);
                                const ImVec2 canvas1 = ImVec2(canvas0.x + canvasSz.x, canvas0.y + canvasSz.y);
                                ImDrawList* fdl = ImGui::GetWindowDrawList();
                                if (s_jhFpGpu.Valid()) {
                                    fdl->AddImage(s_jhFpGpu.ImGuiTexId(), canvas0, canvas1, ImVec2(0.f, 0.f), ImVec2(1.f, 1.f));
                                } else {
                                    fdl->AddRectFilled(canvas0, canvas1, IM_COL32(40, 40, 48, 255));
                                }
                                fdl->AddRect(canvas0, canvas1, IM_COL32(200, 200, 220, 255));
                                if (ImGui::IsItemHovered() && s_jhFpCanvas.width > 0) {
                                    const ImVec2 mp = ImGui::GetIO().MousePos;
                                    const float u = (mp.x - canvas0.x) / std::max(canvasSz.x, 1.f);
                                    const float v = (mp.y - canvas0.y) / std::max(canvasSz.y, 1.f);
                                    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                                        Jackhammer::TexturePaint::PaintDisc(s_jhFpCanvas, u, v, s_jhFpBrushRadius,
                                            s_jhFpHardness,
                                            static_cast<uint8_t>(std::clamp(s_jhFpColor[0], 0.f, 1.f) * 255.f),
                                            static_cast<uint8_t>(std::clamp(s_jhFpColor[1], 0.f, 1.f) * 255.f),
                                            static_cast<uint8_t>(std::clamp(s_jhFpColor[2], 0.f, 1.f) * 255.f),
                                            static_cast<uint8_t>(std::clamp(s_jhFpColor[3], 0.f, 1.f) * 255.f), s_jhFpErase);
                                        (void)s_jhFpGpu.SetSizeUpload(window, static_cast<uint32_t>(s_jhFpCanvas.width),
                                            static_cast<uint32_t>(s_jhFpCanvas.height), s_jhFpCanvas.rgba.data(),
                                            s_jhFpCanvas.rgba.size());
                                        Jackhammer::ViewportDraw::ClearBspTextureTintCaches();
                                    }
                                }
                                if (ImGui::Button("Save PNG to face path##jhfpsv")) {
                                    if (resolvedTex.empty()) {
                                        status = "Face paint: set a texture path (or Save as…).";
                                    } else if (LibUI::Tools::SaveRgba8ToPngFile(resolvedTex, s_jhFpCanvas.rgba.data(),
                                                 s_jhFpCanvas.width, s_jhFpCanvas.height)) {
                                        Jackhammer::ViewportDraw::ClearBspTextureTintCaches();
                                        dirty = true;
                                        status = "Face paint: wrote PNG.";
                                    } else {
                                        status = "Face paint: PNG save failed (path / permissions).";
                                    }
                                }
                                ImGui::SameLine();
                                if (ImGui::Button("Save as…##jhfpsa")) {
                                    const int fpBspSel = spatialBspSel;
                                    const int fpSideSnap = s_jhFpTarget;
                                    LibUI::FileDialogs::ShowSaveFile(
                                        window, "Save painted texture as PNG",
                                        [&, fpBspSel, fpSideSnap](std::optional<std::string> path) {
                                            if (!path || path->empty() || !map.Bsp.has_value()) {
                                                return;
                                            }
                                            if (!LibUI::Tools::SaveRgba8ToPngFile(*path, s_jhFpCanvas.rgba.data(),
                                                    s_jhFpCanvas.width, s_jhFpCanvas.height)) {
                                                status = "Face paint: save failed.";
                                                return;
                                            }
                                            PushMapUndoSnapshot(map);
                                            SmfAuthoringBspNode& n2 = map.Bsp->Nodes[static_cast<size_t>(fpBspSel)];
                                            const std::string rel = ToMapRelativePathIfPossible(*path, currentPath);
                                            if (fpSideSnap == 0) {
                                                n2.FrontTexturePath = rel;
                                            } else {
                                                n2.BackTexturePath = rel;
                                            }
                                            Jackhammer::ViewportDraw::ClearBspTextureTintCaches();
                                            dirty = true;
                                            status = "Face paint: saved and assigned to face.";
                                            s_jhFpNode = -1000000000;
                                        },
                                        kJhImageFileFilters);
                                }
                                ImGui::TextDisabled(
                                    "Paints the raster for the selected BSP face. Save writes the resolved path; "
                                    "Save as assigns a map-relative path on this node.");
                            }
                            if (ImGui::SmallButton("Preset: floor (XZ, Y up)")) {
                                PushMapUndoSnapshot(map);
                                nd.PlaneNormal = Jackhammer::Spatial::NormalizeSmfVec3(SmfVec3{0.f, 1.f, 0.f});
                                nd.PlaneD = 0.f;
                                dirty = true;
                            }
                            ImGui::SameLine();
                            if (ImGui::SmallButton("Preset: wall +X")) {
                                PushMapUndoSnapshot(map);
                                nd.PlaneNormal = Jackhammer::Spatial::NormalizeSmfVec3(SmfVec3{1.f, 0.f, 0.f});
                                dirty = true;
                            }
                            ImGui::SameLine();
                            if (ImGui::SmallButton("Preset: wall +Z")) {
                                PushMapUndoSnapshot(map);
                                nd.PlaneNormal = Jackhammer::Spatial::NormalizeSmfVec3(SmfVec3{0.f, 0.f, 1.f});
                                dirty = true;
                            }
                            if (ImGui::SmallButton("Preset: wall −X")) {
                                PushMapUndoSnapshot(map);
                                nd.PlaneNormal = Jackhammer::Spatial::NormalizeSmfVec3(SmfVec3{-1.f, 0.f, 0.f});
                                dirty = true;
                            }
                            ImGui::SameLine();
                            if (ImGui::SmallButton("Preset: wall −Y")) {
                                PushMapUndoSnapshot(map);
                                nd.PlaneNormal = Jackhammer::Spatial::NormalizeSmfVec3(SmfVec3{0.f, -1.f, 0.f});
                                dirty = true;
                            }
                            ImGui::SameLine();
                            if (ImGui::SmallButton("Preset: wall −Z")) {
                                PushMapUndoSnapshot(map);
                                nd.PlaneNormal = Jackhammer::Spatial::NormalizeSmfVec3(SmfVec3{0.f, 0.f, -1.f});
                                dirty = true;
                            }
                            static float s_bspPlaneDSnapGrid = 1.f;
                            ImGui::DragFloat("Snap D grid", &s_bspPlaneDSnapGrid, 0.05f, 0.001f, 256.f);
                            if (ImGui::SmallButton("Snap plane D to grid")) {
                                PushMapUndoSnapshot(map);
                                const float g = std::max(s_bspPlaneDSnapGrid, 1e-6f);
                                nd.PlaneD = std::round(nd.PlaneD / g) * g;
                                dirty = true;
                            }
                            ImGui::Separator();
                            ImGui::TextUnformatted("Finite slab (AABB) — bounds the BSP overlay in the engine view");
                            if (ImGui::Checkbox("Slab valid", &nd.SlabValid)) {
                                dirty = true;
                            }
                            if (nd.SlabValid) {
                                if (ImGui::DragFloat3("Slab min", &nd.SlabMin.x, 0.05f)) {
                                    dirty = true;
                                    if (bspIsBoxBrush) {
                                        Jackhammer::Spatial::SyncJhBspBoxBrushFromAabb(b, nd.SlabMin, nd.SlabMax);
                                    }
                                }
                                if (ImGui::DragFloat3("Slab max", &nd.SlabMax.x, 0.05f)) {
                                    dirty = true;
                                    if (bspIsBoxBrush) {
                                        Jackhammer::Spatial::SyncJhBspBoxBrushFromAabb(b, nd.SlabMin, nd.SlabMax);
                                    }
                                }
                                ImGui::Separator();
                                ImGui::TextUnformatted("Vertex manipulation (AABB corners)");
                                static int s_jhBspVertexIndex = 0;
                                const char* kCornerNames[8] = {
                                    "000 min/min/min",
                                    "100 max/min/min",
                                    "010 min/max/min",
                                    "110 max/max/min",
                                    "001 min/min/max",
                                    "101 max/min/max",
                                    "011 min/max/max",
                                    "111 max/max/max",
                                };
                                if (ImGui::BeginCombo("Corner##jhbspvert", kCornerNames[std::clamp(s_jhBspVertexIndex, 0, 7)])) {
                                    for (int ci = 0; ci < 8; ++ci) {
                                        const bool isSel = s_jhBspVertexIndex == ci;
                                        if (ImGui::Selectable(kCornerNames[ci], isSel)) {
                                            s_jhBspVertexIndex = ci;
                                        }
                                        if (isSel) {
                                            ImGui::SetItemDefaultFocus();
                                        }
                                    }
                                    ImGui::EndCombo();
                                }
                                s_jhBspVertexIndex = std::clamp(s_jhBspVertexIndex, 0, 7);
                                SmfVec3 corner = Jackhammer::Spatial::JhAabbCorner(nd.SlabMin, nd.SlabMax, s_jhBspVertexIndex);
                                if (ImGui::DragFloat3("Corner position##jhbspvertpos", &corner.x, 0.05f)) {
                                    Jackhammer::Spatial::JhSetAabbCorner(nd.SlabMin, nd.SlabMax, s_jhBspVertexIndex, corner);
                                    dirty = true;
                                    if (bspIsBoxBrush) {
                                        Jackhammer::Spatial::SyncJhBspBoxBrushFromAabb(b, nd.SlabMin, nd.SlabMax);
                                    }
                                }
                                ImGui::TextDisabled(
                                    "Corners move the finite slab AABB (brush footprint). On a generated box brush, all six "
                                    "face planes stay in sync; on arbitrary BSP nodes, edit corners then sync plane D if the "
                                    "split is axis-aligned.");
                                if (ImGui::Button("Sync axis-aligned plane D from slab##jhbsppdsync")) {
                                    PushMapUndoSnapshot(map);
                                    Jackhammer::Spatial::JhSyncAxisAlignedPlaneDFromSlab(nd);
                                    dirty = true;
                                    status = "BSP: plane D synced from slab (axis-aligned normals only).";
                                }
                                if (bspIsBoxBrush && ImGui::SmallButton("Sync all box faces from selected slab##jhbspvertsync")) {
                                    PushMapUndoSnapshot(map);
                                    Jackhammer::Spatial::SyncJhBspBoxBrushFromAabb(b, nd.SlabMin, nd.SlabMax);
                                    dirty = true;
                                    status = "BSP brush: synced all face planes/slabs from selected slab.";
                                }
                                if (ImGui::SmallButton("Slab: ±4 m cube around origin")) {
                                    PushMapUndoSnapshot(map);
                                    nd.SlabMin = SmfVec3{-4.f, -4.f, -4.f};
                                    nd.SlabMax = SmfVec3{4.f, 4.f, 4.f};
                                    nd.SlabValid = true;
                                    dirty = true;
                                    if (bspIsBoxBrush) {
                                        Jackhammer::Spatial::SyncJhBspBoxBrushFromAabb(b, nd.SlabMin, nd.SlabMax);
                                    }
                                }
                                ImGui::SameLine();
                                if (ImGui::SmallButton("±1 m##jhsl1")) {
                                    PushMapUndoSnapshot(map);
                                    nd.SlabMin = SmfVec3{-1.f, -1.f, -1.f};
                                    nd.SlabMax = SmfVec3{1.f, 1.f, 1.f};
                                    nd.SlabValid = true;
                                    dirty = true;
                                    if (bspIsBoxBrush) {
                                        Jackhammer::Spatial::SyncJhBspBoxBrushFromAabb(b, nd.SlabMin, nd.SlabMax);
                                    }
                                }
                                ImGui::SameLine();
                                if (ImGui::SmallButton("±2 m##jhsl2")) {
                                    PushMapUndoSnapshot(map);
                                    nd.SlabMin = SmfVec3{-2.f, -2.f, -2.f};
                                    nd.SlabMax = SmfVec3{2.f, 2.f, 2.f};
                                    nd.SlabValid = true;
                                    dirty = true;
                                    if (bspIsBoxBrush) {
                                        Jackhammer::Spatial::SyncJhBspBoxBrushFromAabb(b, nd.SlabMin, nd.SlabMax);
                                    }
                                }
                                ImGui::SameLine();
                                if (ImGui::SmallButton("±8 m##jhsl8")) {
                                    PushMapUndoSnapshot(map);
                                    nd.SlabMin = SmfVec3{-8.f, -8.f, -8.f};
                                    nd.SlabMax = SmfVec3{8.f, 8.f, 8.f};
                                    nd.SlabValid = true;
                                    dirty = true;
                                    if (bspIsBoxBrush) {
                                        Jackhammer::Spatial::SyncJhBspBoxBrushFromAabb(b, nd.SlabMin, nd.SlabMax);
                                    }
                                }
                                static float s_jhFitAllOriginsPad = 0.25f;
                                ImGui::DragFloat("Padding (fit all origins)", &s_jhFitAllOriginsPad, 0.01f, 0.f, 1.0e6f);
                                if (ImGui::Button("Slab fit ALL entity origins##jhallo")) {
                                    SmfVec3 lo, hi;
                                    if (!JhComputeEntityOriginAabb(map, lo, hi)) {
                                        status = "BSP: no entity with origin / position.";
                                    } else {
                                        PushMapUndoSnapshot(map);
                                        const float p = std::max(s_jhFitAllOriginsPad, 0.f);
                                        nd.SlabMin = SmfVec3{lo.x - p, lo.y - p, lo.z - p};
                                        nd.SlabMax = SmfVec3{hi.x + p, hi.y + p, hi.z + p};
                                        nd.SlabValid = true;
                                        dirty = true;
                                        if (bspIsBoxBrush) {
                                            Jackhammer::Spatial::SyncJhBspBoxBrushFromAabb(b, nd.SlabMin, nd.SlabMax);
                                        }
                                        status = "BSP: slab fitted to AABB of all entity origins.";
                                    }
                                }
                                static float s_jhSlabSnapGrid = 0.25f;
                                ImGui::DragFloat("Slab snap grid", &s_jhSlabSnapGrid, 0.01f, 0.001f, 256.f);
                                if (ImGui::SmallButton("Snap slab min/max to grid##jhssg")) {
                                    PushMapUndoSnapshot(map);
                                    Jackhammer::Spatial::SnapSmfAabbToGrid(nd.SlabMin, nd.SlabMax, s_jhSlabSnapGrid);
                                    dirty = true;
                                    if (bspIsBoxBrush) {
                                        Jackhammer::Spatial::SyncJhBspBoxBrushFromAabb(b, nd.SlabMin, nd.SlabMax);
                                    }
                                    status = "BSP: slab snapped to grid.";
                                }
                                static float s_jhSlabOriginHalf = 1.f;
                                ImGui::DragFloat("Slab cube half-extent (entity)", &s_jhSlabOriginHalf, 0.05f, 0.01f, 1.0e6f);
                                if (ImGui::SmallButton("Slab cube around selected entity origin##jhorg")) {
                                    if (selectedEntity >= 0 && static_cast<size_t>(selectedEntity) < map.Entities.size()) {
                                        if (const SmfVec3* o = TryGetEntityOriginVec3(map.Entities[static_cast<size_t>(
                                                selectedEntity)])) {
                                            PushMapUndoSnapshot(map);
                                            const float h = std::max(s_jhSlabOriginHalf, 1e-3f);
                                            nd.SlabMin = SmfVec3{o->x - h, o->y - h, o->z - h};
                                            nd.SlabMax = SmfVec3{o->x + h, o->y + h, o->z + h};
                                            nd.SlabValid = true;
                                            dirty = true;
                                            if (bspIsBoxBrush) {
                                                Jackhammer::Spatial::SyncJhBspBoxBrushFromAabb(b, nd.SlabMin, nd.SlabMax);
                                            }
                                            status = "BSP: slab set around entity origin.";
                                        } else {
                                            status = "BSP: selected entity has no origin.";
                                        }
                                    } else {
                                        status = "BSP: select an entity with an origin.";
                                    }
                                }
                            }
                            ImGui::Separator();
                            ImGui::TextUnformatted("CSG-style slab (axis-aligned AABB brushes)");
                            ImGui::TextDisabled(
                                "Node A = selected. **All** ops are **axis-aligned AABBs** (box brushes). Arbitrary triangle "
                                "meshes are not CSG operands here — use mesh workshop AABB as a stand-in. Hull union = single "
                                "bounding AABB. Exact union/subtract/XOR fill the piece buffer. Intersect = single AABB.");
                            static int s_jhBspCsgOtherNode = 1;
                            static float s_jhBspSlabExpandMargin = 0.25f;
                            static float s_jhBspSlabInsetMargin = 0.125f;
                            const int nBspNodes = static_cast<int>(b.Nodes.size());
                            ImGui::DragInt("Node B (operand)", &s_jhBspCsgOtherNode, 1.f, 0, std::max(0, nBspNodes - 1));
                            s_jhBspCsgOtherNode = std::clamp(s_jhBspCsgOtherNode, 0, std::max(0, nBspNodes - 1));
                            const int iB = s_jhBspCsgOtherNode;
                            const SmfAuthoringBspNode& nb = b.Nodes[static_cast<size_t>(iB)];
                            if (ImGui::Button("Union A∪B (hull AABB)##jhbsuhull")) {
                                if (iB == spatialBspSel) {
                                    status = "BSP CSG: pick a different node for B.";
                                } else if (!nd.SlabValid || !nb.SlabValid) {
                                    status = "BSP CSG: both nodes need Slab valid.";
                                } else {
                                    PushMapUndoSnapshot(map);
                                    SmfVec3 umin, umax;
                                    Jackhammer::Spatial::SmfAabbUnion(umin, umax, nd.SlabMin, nd.SlabMax, nb.SlabMin, nb.SlabMax);
                                    nd.SlabMin = umin;
                                    nd.SlabMax = umax;
                                    nd.SlabValid = true;
                                    dirty = true;
                                    if (bspIsBoxBrush) {
                                        Jackhammer::Spatial::SyncJhBspBoxBrushFromAabb(b, nd.SlabMin, nd.SlabMax);
                                    }
                                    status = "BSP CSG: hull union (single bounding AABB) applied to A.";
                                }
                            }
                            ImGui::SameLine();
                            if (ImGui::Button("Union A∪B (exact → buffer)##jhbsuex")) {
                                if (iB == spatialBspSel) {
                                    status = "BSP CSG: pick a different node for B.";
                                } else if (!nd.SlabValid || !nb.SlabValid) {
                                    status = "BSP CSG: both nodes need Slab valid.";
                                } else {
                                    Jackhammer::Spatial::SmfAabbBooleanUnionAllPieces(s_jhBspCsgPieces, nd.SlabMin, nd.SlabMax,
                                        nb.SlabMin, nb.SlabMax);
                                    char csgBuf[160];
                                    std::snprintf(csgBuf, sizeof(csgBuf),
                                        "BSP CSG: %zu union piece(s) in buffer — apply one to slab below.",
                                        s_jhBspCsgPieces.size());
                                    status = csgBuf;
                                }
                            }
                            ImGui::SameLine();
                            if (ImGui::Button("Symmetric diff A⊕B (XOR → buffer)##jhbsxor")) {
                                if (iB == spatialBspSel) {
                                    status = "BSP CSG: pick a different node for B.";
                                } else if (!nd.SlabValid || !nb.SlabValid) {
                                    status = "BSP CSG: both nodes need Slab valid.";
                                } else {
                                    Jackhammer::Spatial::SmfAabbBooleanXorAllPieces(
                                        s_jhBspCsgPieces, nd.SlabMin, nd.SlabMax, nb.SlabMin, nb.SlabMax);
                                    char csgBuf[128];
                                    std::snprintf(csgBuf, sizeof(csgBuf), "BSP CSG: %zu XOR piece(s) in buffer.",
                                        s_jhBspCsgPieces.size());
                                    status = csgBuf;
                                }
                            }
                            if (ImGui::Button("Mesh workshop AABB → single piece in buffer##jhbsmwa")) {
                                SmfVec3 mnm{}, mxm{};
                                if (!Jackhammer::MeshOps::ComputeAxisAlignedBounds(s_jhMeshWorkshop, mnm, mxm)) {
                                    status = "BSP CSG: mesh workshop is empty (no AABB).";
                                } else {
                                    s_jhBspCsgPieces.clear();
                                    s_jhBspCsgPieces.push_back({mnm, mxm});
                                    status = "BSP CSG: mesh workshop bounds pushed to piece buffer.";
                                }
                            }
                            if (ImGui::IsItemHovered()) {
                                ImGui::SetTooltip("Use after building geometry in the **mesh workshop** (or block tool → workshop).");
                            }
                            if (ImGui::Button("Intersect A∩B (exact)##jhbsi")) {
                                if (iB == spatialBspSel) {
                                    status = "BSP CSG: pick a different node for B.";
                                } else if (!nd.SlabValid || !nb.SlabValid) {
                                    status = "BSP CSG: both nodes need Slab valid.";
                                } else {
                                    PushMapUndoSnapshot(map);
                                    SmfVec3 omin, omax;
                                    if (!Jackhammer::Spatial::SmfAabbIntersect(omin, omax, nd.SlabMin, nd.SlabMax, nb.SlabMin, nb.SlabMax)) {
                                        nd.SlabValid = false;
                                        status = "BSP CSG: intersection empty (slab invalidated on A).";
                                    } else {
                                        nd.SlabMin = omin;
                                        nd.SlabMax = omax;
                                        nd.SlabValid = true;
                                        if (bspIsBoxBrush) {
                                            Jackhammer::Spatial::SyncJhBspBoxBrushFromAabb(b, nd.SlabMin, nd.SlabMax);
                                        }
                                        status = "BSP CSG: intersection applied to selected node's slab.";
                                    }
                                    dirty = true;
                                }
                            }
                            if (ImGui::Button("Subtract A\\\\B (largest box)##jhbsd")) {
                                if (iB == spatialBspSel) {
                                    status = "BSP CSG: pick a different node for B.";
                                } else if (!nd.SlabValid || !nb.SlabValid) {
                                    status = "BSP CSG: both nodes need Slab valid.";
                                } else {
                                    PushMapUndoSnapshot(map);
                                    SmfVec3 dmin, dmax;
                                    if (!Jackhammer::Spatial::SmfAabbBooleanSubtractLargestPiece(dmin, dmax, nd.SlabMin, nd.SlabMax,
                                            nb.SlabMin, nb.SlabMax)) {
                                        nd.SlabValid = false;
                                        status = "BSP CSG: subtract empty (A inside B?).";
                                    } else {
                                        nd.SlabMin = dmin;
                                        nd.SlabMax = dmax;
                                        nd.SlabValid = true;
                                        if (bspIsBoxBrush) {
                                            Jackhammer::Spatial::SyncJhBspBoxBrushFromAabb(b, nd.SlabMin, nd.SlabMax);
                                        }
                                        status = "BSP CSG: subtract applied (largest single AABB in A\\\\B).";
                                    }
                                    dirty = true;
                                }
                            }
                            ImGui::SameLine();
                            if (ImGui::Button("Subtract A\\\\B (all → buffer)##jhbsuball")) {
                                if (iB == spatialBspSel) {
                                    status = "BSP CSG: pick a different node for B.";
                                } else if (!nd.SlabValid || !nb.SlabValid) {
                                    status = "BSP CSG: both nodes need Slab valid.";
                                } else {
                                    Jackhammer::Spatial::SmfAabbBooleanSubtractAllPieces(
                                        s_jhBspCsgPieces, nd.SlabMin, nd.SlabMax, nb.SlabMin, nb.SlabMax);
                                    char csgBuf2[128];
                                    std::snprintf(csgBuf2, sizeof(csgBuf2), "BSP CSG: %zu subtract piece(s) in buffer.",
                                        s_jhBspCsgPieces.size());
                                    status = s_jhBspCsgPieces.empty() ? "BSP CSG: subtract produced no pieces." : csgBuf2;
                                }
                            }
                            if (!s_jhBspCsgPieces.empty()) {
                                ImGui::Text("Piece buffer: %zu (apply one to selected slab)", s_jhBspCsgPieces.size());
                                for (size_t pi = 0; pi < s_jhBspCsgPieces.size(); ++pi) {
                                    ImGui::PushID(static_cast<int>(pi));
                                    if (ImGui::SmallButton("Apply##jhapplycsg")) {
                                        PushMapUndoSnapshot(map);
                                        nd.SlabMin = s_jhBspCsgPieces[pi].first;
                                        nd.SlabMax = s_jhBspCsgPieces[pi].second;
                                        nd.SlabValid = true;
                                        dirty = true;
                                        if (bspIsBoxBrush) {
                                            Jackhammer::Spatial::SyncJhBspBoxBrushFromAabb(b, nd.SlabMin, nd.SlabMax);
                                        }
                                        status = "BSP CSG: applied piece to selected slab.";
                                    }
                                    ImGui::SameLine();
                                    ImGui::Text("piece %u", static_cast<unsigned>(pi));
                                    ImGui::PopID();
                                }
                                if (ImGui::SmallButton("Clear piece buffer##jhcsgr")) {
                                    s_jhBspCsgPieces.clear();
                                    status = "BSP CSG: piece buffer cleared.";
                                }
                            }
                            if (ImGui::Button("Copy B slab to A##jhbscp")) {
                                if (iB == spatialBspSel) {
                                    status = "BSP CSG: pick a different node for B.";
                                } else if (!nb.SlabValid) {
                                    status = "BSP CSG: operand B needs Slab valid.";
                                } else {
                                    PushMapUndoSnapshot(map);
                                    nd.SlabMin = nb.SlabMin;
                                    nd.SlabMax = nb.SlabMax;
                                    nd.SlabValid = true;
                                    dirty = true;
                                    if (bspIsBoxBrush) {
                                        Jackhammer::Spatial::SyncJhBspBoxBrushFromAabb(b, nd.SlabMin, nd.SlabMax);
                                    }
                                    status = "BSP CSG: copied B slab to A.";
                                }
                            }
                            if (ImGui::Button("Mirror A slab across plane B##jhbsm")) {
                                if (iB == spatialBspSel) {
                                    status = "BSP CSG: pick a different node for B (mirror plane).";
                                } else if (!nd.SlabValid) {
                                    status = "BSP CSG: selected node needs Slab valid.";
                                } else {
                                    PushMapUndoSnapshot(map);
                                    Jackhammer::Spatial::MirrorSmfAabbAcrossBspPlane(nd.SlabMin, nd.SlabMax, nd.SlabMin, nd.SlabMax, nb);
                                    nd.SlabValid = true;
                                    dirty = true;
                                    if (bspIsBoxBrush) {
                                        Jackhammer::Spatial::SyncJhBspBoxBrushFromAabb(b, nd.SlabMin, nd.SlabMax);
                                    }
                                    status = "BSP CSG: mirrored selected slab across B's plane.";
                                }
                            }
                            if (ImGui::Button("Strip A \\ B slab (heuristic)##jhbsub")) {
                                if (iB == spatialBspSel) {
                                    status = "BSP CSG: pick a different node for B.";
                                } else if (!nd.SlabValid || !nb.SlabValid) {
                                    status = "BSP CSG: both nodes need Slab valid.";
                                } else {
                                    PushMapUndoSnapshot(map);
                                    SmfVec3 smin, smax;
                                    if (!Jackhammer::Spatial::SmfAabbLargestFaceStripOutsideB(smin, smax, nd.SlabMin, nd.SlabMax, nb.SlabMin,
                                            nb.SlabMax)) {
                                        nd.SlabValid = false;
                                        status =
                                            "BSP CSG: strip subtract empty (A inside B on all six face corridors?).";
                                    } else {
                                        nd.SlabMin = smin;
                                        nd.SlabMax = smax;
                                        nd.SlabValid = true;
                                        if (bspIsBoxBrush) {
                                            Jackhammer::Spatial::SyncJhBspBoxBrushFromAabb(b, nd.SlabMin, nd.SlabMax);
                                        }
                                        status =
                                            "BSP CSG: applied largest face-strip outside B (approximation, not full A\\B).";
                                    }
                                    dirty = true;
                                }
                            }
                            ImGui::TextDisabled(
                                "Strip: largest axis-aligned corridor of A outside B; use for quick cut-outs, not exact CSG.");
                            ImGui::DragFloat("Expand margin (± all axes)", &s_jhBspSlabExpandMargin, 0.01f, 0.f, 1.0e6f);
                            if (ImGui::Button("Expand selected slab##jhbsex")) {
                                if (!nd.SlabValid) {
                                    status = "BSP CSG: selected node needs Slab valid.";
                                } else {
                                    PushMapUndoSnapshot(map);
                                    Jackhammer::Spatial::ExpandSmfAabb(nd.SlabMin, nd.SlabMax, s_jhBspSlabExpandMargin);
                                    dirty = true;
                                    if (bspIsBoxBrush) {
                                        Jackhammer::Spatial::SyncJhBspBoxBrushFromAabb(b, nd.SlabMin, nd.SlabMax);
                                    }
                                    status = "BSP CSG: slab expanded.";
                                }
                            }
                            ImGui::DragFloat("Inset margin (shrink)", &s_jhBspSlabInsetMargin, 0.01f, 0.f, 1.0e6f);
                            if (ImGui::Button("Inset selected slab##jhbsin")) {
                                if (!nd.SlabValid) {
                                    status = "BSP CSG: selected node needs Slab valid.";
                                } else {
                                    PushMapUndoSnapshot(map);
                                    if (!Jackhammer::Spatial::InsetSmfAabb(nd.SlabMin, nd.SlabMax, s_jhBspSlabInsetMargin)) {
                                        nd.SlabValid = false;
                                        status = "BSP CSG: inset too large (slab invalidated).";
                                    } else {
                                        if (bspIsBoxBrush) {
                                            Jackhammer::Spatial::SyncJhBspBoxBrushFromAabb(b, nd.SlabMin, nd.SlabMax);
                                        }
                                        status = "BSP CSG: slab inset.";
                                    }
                                    dirty = true;
                                }
                            }
                            ImGui::Separator();
                            ImGui::TextUnformatted("Slab extrude (per-axis)");
                            static float s_jhSlabExtrudeStep = 0.5f;
                            ImGui::DragFloat("Extrude step##jhsext", &s_jhSlabExtrudeStep, 0.05f, -64.f, 64.f);
                            ImGui::BeginDisabled(!nd.SlabValid);
                            auto extrudeFace = [&](int faceIdx, const char* label) {
                                if (ImGui::SmallButton(label)) {
                                    PushMapUndoSnapshot(map);
                                    SmfVec3 mn = nd.SlabMin;
                                    SmfVec3 mx = nd.SlabMax;
                                    if (Jackhammer::Spatial::PushSlabFace(mn, mx, faceIdx, s_jhSlabExtrudeStep)) {
                                        nd.SlabMin = mn;
                                        nd.SlabMax = mx;
                                        nd.SlabValid = true;
                                        dirty = true;
                                        if (bspIsBoxBrush) {
                                            Jackhammer::Spatial::SyncJhBspBoxBrushFromAabb(b, nd.SlabMin, nd.SlabMax);
                                        }
                                        status = "BSP: extruded slab along one face.";
                                    } else {
                                        status = "BSP: extrude rejected (collapses slab?).";
                                    }
                                }
                            };
                            extrudeFace(0, "+X##jhex0");
                            ImGui::SameLine();
                            extrudeFace(1, "−X##jhex1");
                            ImGui::SameLine();
                            extrudeFace(2, "+Y##jhex2");
                            ImGui::SameLine();
                            extrudeFace(3, "−Y##jhex3");
                            ImGui::SameLine();
                            extrudeFace(4, "+Z##jhex4");
                            ImGui::SameLine();
                            extrudeFace(5, "−Z##jhex5");
                            ImGui::EndDisabled();
                        }
                    }
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Octree")) {
                    if (!map.Octree.has_value()) {
                        if (ImGui::Button("Create octree")) {
                            PushMapUndoSnapshot(map);
                            SmfAuthoringOctree o;
                            SmfAuthoringOctreeNode r{};
                            r.Min = SmfVec3{-4.f, -4.f, -4.f};
                            r.Max = SmfVec3{4.f, 4.f, 4.f};
                            r.Children.fill(-1);
                            r.LeafId = 0xFFFFFFFFu;
                            o.Nodes.push_back(r);
                            o.RootIndex = 0;
                            map.Octree = std::move(o);
                            dirty = true;
                            spatialOctSel = 0;
                        }
                    } else {
                        auto& o = *map.Octree;
                        ImGui::Text("Nodes: %zu  Root: %u", o.Nodes.size(), o.RootIndex);
                        if (ImGui::Button("Subdivide selected (8 children)")) {
                            PushMapUndoSnapshot(map);
                            spatialOctSel = std::clamp(spatialOctSel, 0, static_cast<int>(o.Nodes.size()) - 1);
                            const int parentIdx = spatialOctSel;
                            const SmfAuthoringOctreeNode parentCopy = o.Nodes[static_cast<size_t>(parentIdx)];
                            const int firstChild = static_cast<int>(o.Nodes.size());
                            for (int i = 0; i < 8; ++i) {
                                SmfAuthoringOctreeNode ch{};
                                const float mx = (parentCopy.Min.x + parentCopy.Max.x) * 0.5f;
                                const float my = (parentCopy.Min.y + parentCopy.Max.y) * 0.5f;
                                const float mz = (parentCopy.Min.z + parentCopy.Max.z) * 0.5f;
                                const float x0 = parentCopy.Min.x;
                                const float y0 = parentCopy.Min.y;
                                const float z0 = parentCopy.Min.z;
                                const float x1 = parentCopy.Max.x;
                                const float y1 = parentCopy.Max.y;
                                const float z1 = parentCopy.Max.z;
                                const int ox = (i & 1) ? 1 : 0;
                                const int oy = (i & 2) ? 1 : 0;
                                const int oz = (i & 4) ? 1 : 0;
                                ch.Min.x = ox ? mx : x0;
                                ch.Max.x = ox ? x1 : mx;
                                ch.Min.y = oy ? my : y0;
                                ch.Max.y = oy ? y1 : my;
                                ch.Min.z = oz ? mz : z0;
                                ch.Max.z = oz ? z1 : mz;
                                ch.Children.fill(-1);
                                o.Nodes.push_back(std::move(ch));
                            }
                            for (int i = 0; i < 8; ++i) {
                                o.Nodes[static_cast<size_t>(parentIdx)].Children[static_cast<size_t>(i)] =
                                    firstChild + i;
                            }
                            spatialOctSel = firstChild;
                            dirty = true;
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Clear octree")) {
                            PushMapUndoSnapshot(map);
                            map.Octree.reset();
                            dirty = true;
                        }
                        if (!o.Nodes.empty()) {
                            spatialOctSel = std::clamp(spatialOctSel, 0, static_cast<int>(o.Nodes.size()) - 1);
                            ImGui::SliderInt("Selected node", &spatialOctSel, 0, static_cast<int>(o.Nodes.size()) - 1);
                            SmfAuthoringOctreeNode& nd = o.Nodes[static_cast<size_t>(spatialOctSel)];
                            if (ImGui::DragFloat3("Min", &nd.Min.x, 0.05f)) {
                                dirty = true;
                            }
                            if (ImGui::DragFloat3("Max", &nd.Max.x, 0.05f)) {
                                dirty = true;
                            }
                            ImGui::Separator();
                            static float s_jhOctFitOrigPad = 0.25f;
                            ImGui::DragFloat("Padding (fit all origins)##octpad", &s_jhOctFitOrigPad, 0.01f, 0.f, 1.0e6f);
                            if (ImGui::Button("Fit SELECTED node to all entity origins##octfit")) {
                                SmfVec3 lo, hi;
                                if (!JhComputeEntityOriginAabb(map, lo, hi)) {
                                    status = "Octree: no entity with origin / position.";
                                } else {
                                    PushMapUndoSnapshot(map);
                                    const float p = std::max(s_jhOctFitOrigPad, 0.f);
                                    nd.Min = SmfVec3{lo.x - p, lo.y - p, lo.z - p};
                                    nd.Max = SmfVec3{hi.x + p, hi.y + p, hi.z + p};
                                    dirty = true;
                                    status = "Octree: selected node bounds = origin AABB (+ padding).";
                                }
                            }
                            if (ImGui::Button("Fit ROOT node to all entity origins##octfitroot")) {
                                SmfVec3 lo, hi;
                                if (!JhComputeEntityOriginAabb(map, lo, hi)) {
                                    status = "Octree: no entity with origin / position.";
                                } else {
                                    PushMapUndoSnapshot(map);
                                    const float p = std::max(s_jhOctFitOrigPad, 0.f);
                                    const uint32_t ri =
                                        std::min(o.RootIndex, static_cast<uint32_t>(std::max(o.Nodes.size(), size_t{1}) - 1));
                                    SmfAuthoringOctreeNode& rn = o.Nodes[static_cast<size_t>(ri)];
                                    rn.Min = SmfVec3{lo.x - p, lo.y - p, lo.z - p};
                                    rn.Max = SmfVec3{hi.x + p, hi.y + p, hi.z + p};
                                    dirty = true;
                                    status = "Octree: root node bounds = origin AABB (+ padding).";
                                }
                            }
                            static float s_jhOctSnapGrid = 0.25f;
                            ImGui::DragFloat("Octree snap grid", &s_jhOctSnapGrid, 0.01f, 0.001f, 256.f);
                            if (ImGui::SmallButton("Snap octree min/max to grid##octsg")) {
                                PushMapUndoSnapshot(map);
                                Jackhammer::Spatial::SnapSmfAabbToGrid(nd.Min, nd.Max, s_jhOctSnapGrid);
                                dirty = true;
                                status = "Octree: bounds snapped to grid.";
                            }
                            if (ImGui::Button("Duplicate node (clear children)##octdup")) {
                                PushMapUndoSnapshot(map);
                                SmfAuthoringOctreeNode cpy = nd;
                                cpy.Children.fill(-1);
                                o.Nodes.push_back(std::move(cpy));
                                spatialOctSel = static_cast<int>(o.Nodes.size()) - 1;
                                dirty = true;
                                status = "Octree: duplicated node; child links cleared — wire or subdivide as needed.";
                            }
                            static float s_jhOctBoundsMargin = 0.25f;
                            ImGui::DragFloat("Inflate/deflate margin##octbd", &s_jhOctBoundsMargin, 0.01f, 0.f, 1.0e6f);
                            if (ImGui::SmallButton("Inflate bounds##octinf")) {
                                PushMapUndoSnapshot(map);
                                Jackhammer::Spatial::ExpandSmfAabb(nd.Min, nd.Max, s_jhOctBoundsMargin);
                                dirty = true;
                                status = "Octree: inflated selected AABB.";
                            }
                            ImGui::SameLine();
                            if (ImGui::SmallButton("Deflate bounds##octdef")) {
                                PushMapUndoSnapshot(map);
                                if (!Jackhammer::Spatial::InsetSmfAabb(nd.Min, nd.Max, s_jhOctBoundsMargin)) {
                                    status = "Octree: deflate margin too large (bounds collapsed).";
                                } else {
                                    status = "Octree: deflated selected AABB.";
                                }
                                dirty = true;
                            }
                            if (map.Bsp.has_value() && !map.Bsp->Nodes.empty()) {
                                ImGui::Separator();
                                if (ImGui::Button("Copy octree bounds → BSP selected slab")) {
                                    PushMapUndoSnapshot(map);
                                    spatialBspSel =
                                        std::clamp(spatialBspSel, 0, static_cast<int>(map.Bsp->Nodes.size()) - 1);
                                    SmfAuthoringBspNode& bn = map.Bsp->Nodes[static_cast<size_t>(spatialBspSel)];
                                    bn.SlabMin = nd.Min;
                                    bn.SlabMax = nd.Max;
                                    bn.SlabValid = true;
                                    dirty = true;
                                    status = "Octree → BSP slab: bounds copied.";
                                }
                                ImGui::SameLine();
                                if (ImGui::Button("Copy BSP slab → octree bounds##octbsp")) {
                                    PushMapUndoSnapshot(map);
                                    spatialBspSel =
                                        std::clamp(spatialBspSel, 0, static_cast<int>(map.Bsp->Nodes.size()) - 1);
                                    const SmfAuthoringBspNode& bn = map.Bsp->Nodes[static_cast<size_t>(spatialBspSel)];
                                    if (!bn.SlabValid) {
                                        status = "BSP → Octree: BSP selected node has no valid slab.";
                                    } else {
                                        nd.Min = bn.SlabMin;
                                        nd.Max = bn.SlabMax;
                                        dirty = true;
                                        status = "BSP slab → Octree: bounds copied.";
                                    }
                                }
                                ImGui::TextDisabled("BSP target is \"Selected node\" on the BSP tab.");
                            }
                        }
                    }
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }

        ImGui::Separator();
        if (ImGui::CollapsingHeader("Environment / Skybox##jhsky")) {
            if (!map.Skybox.has_value()) {
                if (ImGui::Button("Add skybox (cubemap paths)##jhskyadd")) {
                    PushMapUndoSnapshot(map);
                    map.Skybox = Solstice::Smf::SmfSkybox{};
                    dirty = true;
                    status = "Skybox: authoring block added (SMAL extras).";
                }
                ImGui::TextDisabled(
                    "Stored in .smf gameplay extras (SMAL v1: zones, lights, skybox, optional world hook paths).");
            } else {
                Solstice::Smf::SmfSkybox& sky = *map.Skybox;
                if (ImGui::Checkbox("Enabled##jhskyen", &sky.Enabled)) {
                    PushMapUndoSnapshot(map);
                    dirty = true;
                }
                if (ImGui::DragFloat("Brightness##jhskybr", &sky.Brightness, 0.01f, 0.f, 16.f)) {
                    PushMapUndoSnapshot(map);
                    dirty = true;
                }
                if (ImGui::DragFloat("Yaw (deg, +Y)##jhskyyaw", &sky.YawDegrees, 0.25f, -360.f, 360.f)) {
                    PushMapUndoSnapshot(map);
                    dirty = true;
                }
                const char* faceIds[6] = {"+X face##sk0", "-X face##sk1", "+Y face##sk2", "-Y face##sk3", "+Z face##sk4",
                    "-Z face##sk5"};
                for (int fi = 0; fi < 6; ++fi) {
                    ImGui::PushID(fi);
                    char pathBuf[768];
                    std::snprintf(pathBuf, sizeof(pathBuf), "%s", sky.FacePaths[static_cast<size_t>(fi)].c_str());
                    if (ImGui::InputText(faceIds[fi], pathBuf, sizeof(pathBuf))) {
                        PushMapUndoSnapshot(map);
                        sky.FacePaths[static_cast<size_t>(fi)] = pathBuf;
                        dirty = true;
                    }
                    ImGui::SameLine();
                    const int fiSnap = fi;
                    if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::Import, "Browse##jhskyb")) {
                        LibUI::FileDialogs::ShowOpenFile(
                            window, "Skybox face image", [&, fiSnap](std::optional<std::string> path) {
                                if (!path || !map.Skybox.has_value()) {
                                    return;
                                }
                                PushMapUndoSnapshot(map);
                                map.Skybox->FacePaths[static_cast<size_t>(fiSnap)] =
                                    ToMapRelativePathIfPossible(*path, currentPath);
                                dirty = true;
                            },
                            kJhImageFileFilters);
                    }
                    ImGui::PopID();
                }
                if (ImGui::Button("Remove skybox block##jhskyrm")) {
                    PushMapUndoSnapshot(map);
                    map.Skybox.reset();
                    dirty = true;
                    status = "Skybox: removed from map (save to persist).";
                }
            }
        }

        ImGui::Separator();
        if (ImGui::CollapsingHeader("World hooks (path links)##jhwhooks")) {
            ImGui::TextDisabled(
                "Solstice Map Format v1 / TP1 — path strings only; no embedded scripts. Consumed by runtime/tools later.");
            auto editHookPath = [&](const char* label, const char* hint, std::string& dest, const char* browseTitle) {
                char buf[768]{};
                std::snprintf(buf, sizeof(buf), "%s", dest.c_str());
                ImGui::PushID(label);
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 36.f);
                if (ImGui::InputText(label, buf, sizeof(buf))) {
                    PushMapUndoSnapshot(map);
                    dest = buf;
                    dirty = true;
                }
                ImGui::SameLine();
                if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::Import, "##jhhkb")) {
                    LibUI::FileDialogs::ShowOpenFile(
                        window, browseTitle, [&](std::optional<std::string> path) {
                            if (!path || path->empty()) {
                                return;
                            }
                            PushMapUndoSnapshot(map);
                            dest = ToMapRelativePathIfPossible(*path, currentPath);
                            dirty = true;
                        },
                        kJhHookPathBrowseFilters);
                }
                ImGui::PopID();
                if (hint) {
                    ImGui::TextDisabled("%s", hint);
                }
            };
            editHookPath("Script##jhwscr", "e.g. Moonwalk entry or module path.", map.WorldAuthoringHooks.ScriptPath, "Script file");
            editHookPath(
                "Cutscene##jhwcs", "e.g. narrative / cutscene JSON.", map.WorldAuthoringHooks.CutscenePath, "Cutscene JSON");
            editHookPath("World UI##jhwui", "e.g. layout or HUD asset path.", map.WorldAuthoringHooks.WorldSpaceUiPath, "UI asset");
        }

        ImGui::Separator();
        if (ImGui::CollapsingHeader("Gameplay (acoustic zones & lights)", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::TreeNodeEx("Acoustic zones (reverb)", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::Button("Add zone")) {
                    PushMapUndoSnapshot(map);
                    SmfAcousticZone z;
                    z.Name = MakeUniqueAcousticZoneName(map, "reverb_zone");
                    map.AcousticZones.push_back(std::move(z));
                    acousticZoneSel = static_cast<int>(map.AcousticZones.size()) - 1;
                    dirty = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("Remove zone") && acousticZoneSel >= 0
                    && acousticZoneSel < static_cast<int>(map.AcousticZones.size())) {
                    PushMapUndoSnapshot(map);
                    map.AcousticZones.erase(map.AcousticZones.begin() + acousticZoneSel);
                    if (map.AcousticZones.empty()) {
                        acousticZoneSel = -1;
                    } else {
                        acousticZoneSel = std::min(acousticZoneSel, static_cast<int>(map.AcousticZones.size()) - 1);
                    }
                    dirty = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("Duplicate zone##acdup") && acousticZoneSel >= 0
                    && acousticZoneSel < static_cast<int>(map.AcousticZones.size())) {
                    PushMapUndoSnapshot(map);
                    SmfAcousticZone copy = map.AcousticZones[static_cast<size_t>(acousticZoneSel)];
                    copy.Name = MakeUniqueAcousticZoneName(map, copy.Name + "_copy");
                    map.AcousticZones.insert(map.AcousticZones.begin() + acousticZoneSel + 1, std::move(copy));
                    acousticZoneSel++;
                    dirty = true;
                    status = "Acoustic: duplicated zone (new name; review music/ambience paths).";
                }
                if (!map.AcousticZones.empty()) {
                    acousticZoneSel = std::clamp(acousticZoneSel, 0, static_cast<int>(map.AcousticZones.size()) - 1);
                    ImGui::SliderInt("Selected zone", &acousticZoneSel, 0, static_cast<int>(map.AcousticZones.size()) - 1);
                    SmfAcousticZone& z = map.AcousticZones[static_cast<size_t>(acousticZoneSel)];
                    char zn[256];
                    std::snprintf(zn, sizeof(zn), "%s", z.Name.c_str());
                    if (ImGui::InputText("Name", zn, sizeof(zn))) {
                        z.Name = zn;
                        dirty = true;
                    }
                    if (ImGui::DragFloat3("Center", &z.Center.x, 0.05f)) {
                        dirty = true;
                    }
                    if (ImGui::DragFloat3("Extents", &z.Extents.x, 0.05f, 0.001f, 1.0e6f)) {
                        dirty = true;
                    }
                    static float s_jhAcousticFitOrigPad = 0.25f;
                    ImGui::DragFloat("Fit padding (entity origins)", &s_jhAcousticFitOrigPad, 0.01f, 0.f, 1.0e6f);
                    if (ImGui::Button("Fit selected zone to ALL entity origins##acfit")) {
                        SmfVec3 lo, hi;
                        if (!JhComputeEntityOriginAabb(map, lo, hi)) {
                            status = "Acoustic: no entity with origin / position.";
                        } else {
                            PushMapUndoSnapshot(map);
                            const float p = std::max(s_jhAcousticFitOrigPad, 0.f);
                            lo.x -= p;
                            lo.y -= p;
                            lo.z -= p;
                            hi.x += p;
                            hi.y += p;
                            hi.z += p;
                            const float cx = (lo.x + hi.x) * 0.5f;
                            const float cy = (lo.y + hi.y) * 0.5f;
                            const float cz = (lo.z + hi.z) * 0.5f;
                            if (z.IsSpherical) {
                                const float dx = hi.x - lo.x;
                                const float dy = hi.y - lo.y;
                                const float dz = hi.z - lo.z;
                                const float r = 0.5f * std::sqrt(dx * dx + dy * dy + dz * dz);
                                const float rr = std::max(r, 1e-4f);
                                z.Center = SmfVec3{cx, cy, cz};
                                z.Extents = SmfVec3{rr, rr, rr};
                            } else {
                                z.Center = SmfVec3{cx, cy, cz};
                                z.Extents = SmfVec3{std::max((hi.x - lo.x) * 0.5f, 1e-4f),
                                    std::max((hi.y - lo.y) * 0.5f, 1e-4f), std::max((hi.z - lo.z) * 0.5f, 1e-4f)};
                            }
                            dirty = true;
                            status = "Acoustic: zone fitted to origin AABB (+ padding).";
                        }
                    }
                    if (ImGui::Button("Center on selected entity##accen")) {
                        if (selectedEntity < 0 || static_cast<size_t>(selectedEntity) >= map.Entities.size()) {
                            status = "Acoustic: select an entity with an origin.";
                        } else if (const SmfVec3* o = TryGetEntityOriginVec3(map.Entities[static_cast<size_t>(selectedEntity)])) {
                            PushMapUndoSnapshot(map);
                            z.Center = *o;
                            dirty = true;
                            status = "Acoustic: zone center = selected entity origin.";
                        } else {
                            status = "Acoustic: selected entity has no origin / position.";
                        }
                    }
                    ImGui::TextDisabled(
                        "Spherical: bounding sphere of the padded AABB (Extents set to radius on all axes).");
                    const char* presets[] = {"None", "Room", "Cave", "Hallway", "Sewer", "Industrial"};
                    int pi = 0;
                    for (int i = 0; i < 6; ++i) {
                        if (z.ReverbPreset == presets[i]) {
                            pi = i;
                            break;
                        }
                    }
                    if (ImGui::BeginCombo("Reverb preset", presets[pi])) {
                        for (int i = 0; i < 6; ++i) {
                            const bool sel = (pi == i);
                            if (ImGui::Selectable(presets[i], sel)) {
                                z.ReverbPreset = presets[i];
                                dirty = true;
                            }
                            if (sel) {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndCombo();
                    }
                    if (ImGui::DragFloat("Wetness", &z.Wetness, 0.01f, 0.f, 1.f)) {
                        dirty = true;
                    }
                    if (ImGui::DragFloat("Obstruction mult.", &z.ObstructionMultiplier, 0.01f, 0.f, 4.f)) {
                        dirty = true;
                    }
                    if (ImGui::DragInt("Priority", &z.Priority)) {
                        dirty = true;
                    }
                    if (ImGui::Checkbox("Enabled", &z.Enabled)) {
                        dirty = true;
                    }
                    if (ImGui::Checkbox("Spherical (extents.x = radius)", &z.IsSpherical)) {
                        dirty = true;
                    }
                    ImGui::Separator();
                    ImGui::TextUnformatted("Zone music / ambience");
                    ImGui::TextDisabled("Relative to the .smf directory; import copies to assets/audio/ and adds a path-table row.");
                    char muzBuf[512]{};
                    char ambBuf[512]{};
                    std::snprintf(muzBuf, sizeof(muzBuf), "%s", z.MusicPath.c_str());
                    std::snprintf(ambBuf, sizeof(ambBuf), "%s", z.AmbiencePath.c_str());
                    if (ImGui::InputText("Music path##acmuz", muzBuf, sizeof(muzBuf))) {
                        z.MusicPath = muzBuf;
                        dirty = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Clear##acmuz")) {
                        PushMapUndoSnapshot(map);
                        z.MusicPath.clear();
                        dirty = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Import…##acmuz")) {
                        const int zIdx = acousticZoneSel;
                        LibUI::FileDialogs::ShowOpenFile(
                            window, "Import zone music (BGM)",
                            [zIdx](std::optional<std::string> path) {
                                if (path && !path->empty()) {
                                    QueueAcousticImportOp(
                                        JhAcousticImportOp{std::move(*path), true, zIdx});
                                }
                            },
                            kAudioImportFilters);
                    }
                    if (ImGui::InputText("Ambience path##acamb", ambBuf, sizeof(ambBuf))) {
                        z.AmbiencePath = ambBuf;
                        dirty = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Clear##acamb")) {
                        PushMapUndoSnapshot(map);
                        z.AmbiencePath.clear();
                        dirty = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Import…##acamb")) {
                        const int zIdx = acousticZoneSel;
                        LibUI::FileDialogs::ShowOpenFile(
                            window, "Import zone ambience (3D loop at zone center)",
                            [zIdx](std::optional<std::string> path) {
                                if (path && !path->empty()) {
                                    QueueAcousticImportOp(
                                        JhAcousticImportOp{std::move(*path), false, zIdx});
                                }
                            },
                            kAudioImportFilters);
                    }
                }
                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx("Authoring lights", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::Button("Add light")) {
                    PushMapUndoSnapshot(map);
                    SmfAuthoringLight L;
                    L.Name = MakeUniqueAuthoringLightName(map, "light");
                    map.AuthoringLights.push_back(std::move(L));
                    authoringLightSel = static_cast<int>(map.AuthoringLights.size()) - 1;
                    dirty = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("Remove light") && authoringLightSel >= 0
                    && authoringLightSel < static_cast<int>(map.AuthoringLights.size())) {
                    PushMapUndoSnapshot(map);
                    map.AuthoringLights.erase(map.AuthoringLights.begin() + authoringLightSel);
                    if (map.AuthoringLights.empty()) {
                        authoringLightSel = -1;
                    } else {
                        authoringLightSel =
                            std::min(authoringLightSel, static_cast<int>(map.AuthoringLights.size()) - 1);
                    }
                    dirty = true;
                }
                if (!map.AuthoringLights.empty()) {
                    authoringLightSel =
                        std::clamp(authoringLightSel, 0, static_cast<int>(map.AuthoringLights.size()) - 1);
                    ImGui::SliderInt("Selected light", &authoringLightSel, 0,
                        static_cast<int>(map.AuthoringLights.size()) - 1);
                    SmfAuthoringLight& L = map.AuthoringLights[static_cast<size_t>(authoringLightSel)];
                    char ln[256];
                    std::snprintf(ln, sizeof(ln), "%s", L.Name.c_str());
                    if (ImGui::InputText("Name", ln, sizeof(ln))) {
                        L.Name = ln;
                        dirty = true;
                    }
                    const char* curType = SmfAuthoringLightTypeLabel(L.Type);
                    if (ImGui::BeginCombo("Type", curType)) {
                        for (int ti = 0; ti < 3; ++ti) {
                            const auto tt = static_cast<SmfAuthoringLightType>(ti);
                            const bool sel = (L.Type == tt);
                            if (ImGui::Selectable(SmfAuthoringLightTypeLabel(tt), sel)) {
                                L.Type = tt;
                                dirty = true;
                            }
                            if (sel) {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndCombo();
                    }
                    if (ImGui::DragFloat3("Position", &L.Position.x, 0.05f)) {
                        dirty = true;
                    }
                    if (ImGui::DragFloat3("Direction", &L.Direction.x, 0.02f, -1.f, 1.f)) {
                        dirty = true;
                    }
                    if (ImGui::DragFloat3("Color", &L.Color.x, 0.02f, 0.f, 4.f)) {
                        dirty = true;
                    }
                    if (ImGui::DragFloat("Intensity", &L.Intensity, 0.02f, 0.f, 1.0e6f)) {
                        dirty = true;
                    }
                    if (ImGui::DragFloat("Hue", &L.Hue, 0.5f)) {
                        dirty = true;
                    }
                    if (ImGui::DragFloat("Attenuation", &L.Attenuation, 0.02f, 0.f, 1.0e6f)) {
                        dirty = true;
                    }
                    if (ImGui::DragFloat("Range (0 = infinite)", &L.Range, 0.5f, 0.f, 1.0e6f)) {
                        dirty = true;
                    }
                    if (ImGui::DragFloat("Spot inner (deg)", &L.SpotInnerDeg, 0.5f, 0.f, 179.f)) {
                        dirty = true;
                    }
                    if (ImGui::DragFloat("Spot outer (deg)", &L.SpotOuterDeg, 0.5f, 0.f, 179.f)) {
                        dirty = true;
                    }
                }
                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx("Fluid volumes (NSSolver)", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::TextDisabled(
                    "Coarse MAC grids only — bounds + resolution. Runtime clamps ~%s interior cells/volume for stability.",
                    "262k");
                if (ImGui::Button("Add fluid volume##jhfl")) {
                    PushMapUndoSnapshot(map);
                    SmfFluidVolume f;
                    f.Name = MakeUniqueFluidVolumeName(map, "fluid_volume");
                    f.BoundsMin = SmfVec3{0.f, 0.f, 0.f};
                    f.BoundsMax = SmfVec3{4.f, 2.f, 4.f};
                    f.ResolutionX = f.ResolutionY = f.ResolutionZ = 24;
                    f.Diffusion = 0.0001f;
                    f.Viscosity = 0.0001f;
                    map.FluidVolumes.push_back(std::move(f));
                    fluidVolSel = static_cast<int>(map.FluidVolumes.size()) - 1;
                    dirty = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("Remove##jhflrm") && fluidVolSel >= 0
                    && fluidVolSel < static_cast<int>(map.FluidVolumes.size())) {
                    PushMapUndoSnapshot(map);
                    map.FluidVolumes.erase(map.FluidVolumes.begin() + fluidVolSel);
                    if (map.FluidVolumes.empty()) {
                        fluidVolSel = -1;
                    } else {
                        fluidVolSel = std::min(fluidVolSel, static_cast<int>(map.FluidVolumes.size()) - 1);
                    }
                    dirty = true;
                }
                if (!map.FluidVolumes.empty()) {
                    fluidVolSel = std::clamp(fluidVolSel, 0, static_cast<int>(map.FluidVolumes.size()) - 1);
                    ImGui::SliderInt("Selected##jhflsel", &fluidVolSel, 0, static_cast<int>(map.FluidVolumes.size()) - 1);
                    SmfFluidVolume& fv = map.FluidVolumes[static_cast<size_t>(fluidVolSel)];
                    char fn[256]{};
                    std::snprintf(fn, sizeof(fn), "%s", fv.Name.c_str());
                    if (ImGui::InputText("Name##jhflnm", fn, sizeof(fn))) {
                        fv.Name = fn;
                        dirty = true;
                    }
                    if (ImGui::Checkbox("Enabled##jhfen", &fv.Enabled)) {
                        dirty = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::Checkbox("MacCormack advection##jhmc", &fv.EnableMacCormack)) {
                        dirty = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::Checkbox("Boussinesq thermal##jhbou", &fv.EnableBoussinesq)) {
                        dirty = true;
                    }
                    if (ImGui::Checkbox("Vol. vis clip (debug)##jhvclip", &fv.VolumeVisualizationClip)) {
                        dirty = true;
                    }
                    if (ImGui::DragFloat3("Bounds min##jhflmn", &fv.BoundsMin.x, 0.05f)) {
                        dirty = true;
                    }
                    if (ImGui::DragFloat3("Bounds max##jhflmx", &fv.BoundsMax.x, 0.05f)) {
                        dirty = true;
                    }
                    if (ImGui::DragInt("Nx##jhflnx", &fv.ResolutionX, 1, Solstice::Smf::kSmfFluidResolutionMin,
                            Solstice::Smf::kSmfFluidResolutionMax)) {
                        dirty = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::DragInt("Ny##jhflny", &fv.ResolutionY, 1, Solstice::Smf::kSmfFluidResolutionMin,
                            Solstice::Smf::kSmfFluidResolutionMax)) {
                        dirty = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::DragInt("Nz##jhflnz", &fv.ResolutionZ, 1, Solstice::Smf::kSmfFluidResolutionMin,
                            Solstice::Smf::kSmfFluidResolutionMax)) {
                        dirty = true;
                    }
                    {
                        const int64_t est =
                            static_cast<int64_t>(fv.ResolutionX) * static_cast<int64_t>(fv.ResolutionY)
                            * static_cast<int64_t>(fv.ResolutionZ);
                        if (est > Solstice::Smf::kSmfFluidInteriorCellBudget) {
                            ImGui::TextColored(ImVec4(1.f, 0.55f, 0.2f, 1.f),
                                "Interior cells %lld — exceeds budget; engine will shrink axes.", static_cast<long long>(est));
                        } else {
                            ImGui::Text("Interior cells (est.): %lld", static_cast<long long>(est));
                        }
                    }
                    if (ImGui::DragFloat("Diffusion##jhfld", &fv.Diffusion, 1e-5f, 0.f, 10.f, "%.6f")) {
                        dirty = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::DragFloat("Viscosity##jhflv", &fv.Viscosity, 1e-5f, 1e-8f, 10.f, "%.6f")) {
                        dirty = true;
                    }
                    if (ImGui::DragFloat("Reference density (kg/m^3)##jhfrd", &fv.ReferenceDensity, 1.f, 1.f, 2000.f)) {
                        dirty = true;
                    }
                    if (ImGui::DragInt("Pressure relax. iters##jhfpr", &fv.PressureRelaxationIterations, 1, 8, 64)) {
                        dirty = true;
                    }
                    if (fv.EnableBoussinesq) {
                        if (ImGui::DragFloat("Buoyancy strength##jhfb", &fv.BuoyancyStrength, 0.02f, 0.f, 500.f)) {
                            dirty = true;
                        }
                        ImGui::SameLine();
                        if (ImGui::DragFloat("Prandtl##jhfp", &fv.Prandtl, 0.01f, 1e-3f, 100.f)) {
                            dirty = true;
                        }
                    }
                    if (ImGui::Button("Fit bounds to ALL entity origins##jhflfit")) {
                        SmfVec3 lo, hi;
                        if (!JhComputeEntityOriginAabb(map, lo, hi)) {
                            status = "Fluid: no entity with origin / position.";
                        } else {
                            PushMapUndoSnapshot(map);
                            fv.BoundsMin = lo;
                            fv.BoundsMax = hi;
                            dirty = true;
                            status = "Fluid: bounds fitted to origin AABB.";
                        }
                    }
                }
                ImGui::TreePop();
            }
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Path table (RELIC)");
        if (ImGui::Button("Add path")) {
            PushMapUndoSnapshot(map);
            map.PathTable.push_back({"", 0});
            dirty = true;
        }
        for (size_t pi = 0; pi < map.PathTable.size(); ++pi) {
            ImGui::PushID(static_cast<int>(pi + 4096));
            char pbuf[512];
            std::snprintf(pbuf, sizeof(pbuf), "%s", map.PathTable[pi].first.c_str());
            ImGui::InputText("##path", pbuf, sizeof(pbuf));
            if (std::string(pbuf) != map.PathTable[pi].first) {
                map.PathTable[pi].first = pbuf;
                dirty = true;
            }
            ImGui::SameLine();
            unsigned long long h = map.PathTable[pi].second;
            ImGui::SetNextItemWidth(180);
            if (ImGui::InputScalar("##hash", ImGuiDataType_U64, &h, nullptr, nullptr, "%016llX")) {
                map.PathTable[pi].second = h;
                dirty = true;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove##pt")) {
                PushMapUndoSnapshot(map);
                map.PathTable.erase(map.PathTable.begin() + static_cast<ptrdiff_t>(pi));
                dirty = true;
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
        }

        ImGui::EndChild();
        ImGui::EndChild();

        ImGui::BeginChild("jh_bottom_bar", ImVec2(0.f, kJhBottomBarH), true, ImGuiWindowFlags_None);
        ImGui::TextUnformatted("Jackhammer — .smf v1");
        if (ImGui::Button("New##jhbot")) {
            requestNew();
        }
        ImGui::SameLine();
        if (ImGui::Button("Template##jhbot")) {
            ApplyNewMapTemplate(map, currentPath, lastHeader, selectedEntity, dirty, status);
        }
        ImGui::SameLine();
        if (ImGui::Button("Open…##jhbot")) {
            LibUI::FileDialogs::ShowOpenFile(
                window, "Open map", [&](std::optional<std::string> path) { requestOpen(std::move(path)); }, kSmfFilters);
        }
        ImGui::SameLine();
        if (ImGui::Button("Save##jhbot")) {
            if (currentPath) {
                doSaveToDisk(*currentPath);
            } else {
                LibUI::FileDialogs::ShowSaveFile(
                    window, "Save map as", [](std::optional<std::string> path) {
                        if (path) {
                            QueueSavePath(std::move(*path));
                        }
                    },
                    kSmfFilters);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Save As…##jhbot")) {
            LibUI::FileDialogs::ShowSaveFile(
                window, "Save map as", [](std::optional<std::string> path) {
                    if (path) {
                        QueueSavePath(std::move(*path));
                    }
                },
                kSmfFilters);
        }
        ImGui::SameLine();
        ImGui::Checkbox("ZSTD##jhbot", &compressSmf);
        ImGui::SameLine();
        ImGui::Checkbox("Watch##jhbot", &watchMapFile);
        ImGui::SameLine();
        if (ImGui::Button("Validate##jhbot")) {
            runValidate();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("F7");
        ImGui::SameLine();
        if (ImGui::Button("Apply DLL##jhbot")) {
            runApplyGameplay();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("F8");
        if (currentPath) {
            ImGui::Text("File: %s", currentPath->c_str());
        } else {
            ImGui::TextUnformatted("File: (unsaved)");
        }
        ImGui::TextUnformatted(status.c_str());
        if (ImGui::CollapsingHeader("Validation##jhbot")) {
            ImGui::TextWrapped("LibSmf: %s", lastValidateCodec.c_str());
            ImGui::TextWrapped("Engine DLL: %s", lastValidateEngine.c_str());
            ImGui::TextWrapped("Engine apply: %s", lastApplyGameplayEngine.c_str());
            {
                std::string clip;
                clip += "LibSmf: ";
                clip += lastValidateCodec;
                clip += "\nEngine DLL: ";
                clip += lastValidateEngine;
                clip += "\nEngine apply: ";
                clip += lastApplyGameplayEngine;
                clip += "\n";
                if (!lastValidateStructure.empty()) {
                    clip += "Map structure:\n";
                    for (const auto& line : lastValidateStructure) {
                        clip += line;
                        clip += '\n';
                    }
                }
                if (LibUI::Tools::CopyTextButton("jh_val_clip_bot", clip.c_str(), "Copy report")) {
                    status = "Validation report copied.";
                }
            }
            if (!lastValidateStructure.empty()) {
                for (const auto& line : lastValidateStructure) {
                    ImGui::BulletText("%s", line.c_str());
                }
            }
        }
        if (ImGui::CollapsingHeader("Map overview##jhbot")) {
            const bool hasWorldHooks = !map.WorldAuthoringHooks.ScriptPath.empty()
                || !map.WorldAuthoringHooks.CutscenePath.empty() || !map.WorldAuthoringHooks.WorldSpaceUiPath.empty();
            ImGui::Text(
                "Entities: %zu | Paths: %zu | BSP: %s | Octree: %s | Lights: %zu | Acoustic: %zu | Fluids: %zu | Skybox: %s | "
                "Hooks: %s",
                map.Entities.size(), map.PathTable.size(), map.Bsp.has_value() ? "yes" : "no",
                map.Octree.has_value() ? "yes" : "no", map.AuthoringLights.size(), map.AcousticZones.size(),
                map.FluidVolumes.size(), map.Skybox.has_value() ? (map.Skybox->Enabled ? "on" : "off") : "—",
                hasWorldHooks ? "paths" : "—");
        }
        if (ImGui::CollapsingHeader("Recovery (autosave)##jhbot")) {
            ImGui::TextDisabled("While the map is dirty, periodic .smf snapshots are written to the recovery store (next to the executable).");
            int jhRi = static_cast<int>(jhRecoveryIntervalSecU32);
            if (ImGui::SliderInt("Interval (sec)", &jhRi, 10, 600)) {
                jhRecoveryIntervalSecU32 = static_cast<uint32_t>(jhRi);
            }
            ImGui::TextDisabled("File → Write recovery snapshot now for a manual on-demand write.");
        }
        LibUI::Tools::DrawRecentPathsCollapsible("Recent##jhbot", ImGuiTreeNodeFlags_None, nullptr, JackhammerOpenRecentPath);
        ImGui::Text("Sections (bytes): str=%u geom=%u bsp=%u ent=%u sec=%u phys=%u script=%u trig=%u path=%u",
            lastHeader.StringTableSize, lastHeader.GeometrySize, lastHeader.BspSize, lastHeader.EntitySize,
            lastHeader.SectorSize, lastHeader.PhysicsSize, lastHeader.ScriptSize, lastHeader.TriggerSize,
            lastHeader.PathTableSize);
        ImGui::EndChild();

        LibUI::Shell::EndMainHostWindow();

        if (compressSmf != compressBeforeFrame || watchMapFile != watchMapBeforeFrame
            || jhRecoveryIntervalSecU32 != jhRecoveryIntervalBeforeU32) {
            jackhammerSettings.SetString(Solstice::SettingsStore::kKeyFormatVersion, "1");
            jackhammerSettings.SetBool(Solstice::SettingsStore::kKeyCompressSmf, compressSmf);
            jackhammerSettings.SetBool(Solstice::SettingsStore::kKeyWatchMapFile, watchMapFile);
            jackhammerSettings.SetInt64("jhRecoveryIntervalSec", static_cast<std::int64_t>(jhRecoveryIntervalSecU32));
            jackhammerSettings.Save();
        }

        LevelEditorPluginsDrawPanel(&showPluginsPanel);
        if (showShortcutsPanel) {
            ImGui::SetNextWindowSize(ImVec2(460, 320), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Keyboard shortcuts##Jackhammer", &showShortcutsPanel)) {
                ImGui::BulletText("F7 — Validate map (LibSmf + structure + optional engine DLL)");
                ImGui::BulletText("F8 — Probe engine SolsticeV1_SmfApplyGameplay (serializes current map)");
                ImGui::BulletText("Ctrl+N — New map (prompts if unsaved)");
                ImGui::BulletText("Ctrl+O — Open…");
                ImGui::BulletText("Ctrl+S — Save / Save As…");
                ImGui::BulletText("File → Reload from disk (discards edits; confirms if dirty)");
                ImGui::BulletText("PgUp / PgDn — Previous / next entity in the list");
                ImGui::BulletText("Ctrl+Z — Undo map edit");
                ImGui::BulletText("Ctrl+Y / Ctrl+Shift+Z — Redo");
                ImGui::Separator();
                ImGui::TextUnformatted("Files and recovery");
                ImGui::BulletText("Drag-and-drop: .smf (open; respects unsaved prompt) | .gltf / .glb (assign to selection or new Mesh entity)");
                ImGui::BulletText("Recovery: periodic / manual .smf snapshots under the executable; restore prompt when snapshots exist (see bottom bar)");
                ImGui::Separator();
                ImGui::TextUnformatted("Engine viewport");
                ImGui::BulletText("F — Focus orbit on selected entity origin");
                ImGui::BulletText("Ctrl+LMB — Place selected entity origin on XZ plane (y preserved)");
                ImGui::BulletText("Arrow keys — Nudge selected entity origin on XZ when the viewport is hovered (uses Place snap, else grid cell)");
                ImGui::BulletText(
                    "Grid — Cell size and half-extent control the ground grid; snap presets 1–32 for Hammer-style spacing");
                ImGui::BulletText(
                    "BSP — Spatial tab: cycle warning; copy plane/slab as text; tree height; jump/copy index; "
                    "parent/child nav; flip plane; swap child links; alloc children; flood front texture; "
                    "slab presets & fit ALL origins; CSG; Octree: fit/snap, bounds ↔ BSP");
                ImGui::BulletText(
                    "Gameplay — Acoustic: fit selected zone to all entity origins (box or spherical bounding sphere).");
                ImGui::BulletText("Engine view — BSP / Octree / Lights toggles; depth sliders; spatial tree overlays; large maps cap mesh preview count");
                ImGui::BulletText("Entity — Diffuse texture path + thumbnail; tints preview cube albedo");
            }
            ImGui::End();
        }
        if (showAboutPanel) {
            LibUI::Tools::AboutWindowContent about{};
            about.windowTitle = "About Jackhammer";
            about.headline = Solstice::Utilities::kAboutHeadline;
            about.body =
                "Solstice Level Editor for .smf v1: entities, properties, RELIC path table, ZSTD, LibSmf codec "
                "validation, optional engine DLL checks, BSP plane/slab authoring with optional textures, and octree authoring.";
            LibUI::Tools::DrawAboutWindow(&showAboutPanel, about, ImVec2(420, 180));
        }

        // SDL file dialogs may complete between PollEvent batches; Pump + drain processes QueueSavePath the same frame.
        SDL_PumpEvents();
        DrainPendingFileOps(map, currentPath, lastHeader, status, dirty, selectedEntity, compressSmf, &jhBannerFile,
            &jhBannerViewport, &mapFileWatch);
        DrainPendingRelicOps(map, currentPath, status, dirty, &jhBannerFile, &jhBannerViewport);
        DrainPendingGltfOps(map, currentPath, selectedEntity, status, dirty, &jhBannerFile, &jhBannerViewport);
        DrainPendingAcousticAudioImport(map, currentPath, status, dirty);
        UpdateWindowTitle(window, currentPath, dirty);

        // Drawable pixel size (matches ImGui framebuffer scale). During maximize/restore SDL may transiently report 0;
        // still call ImGui Render() — skipping it after NewFrame() corrupts ImGui and commonly crashes on the next frame.
        int w = 0;
        int h = 0;
        SDL_GetWindowSizeInPixels(window, &w, &h);
        if (w > 0 && h > 0) {
            jhLastDrawableW = w;
            jhLastDrawableH = h;
        } else {
            w = jhLastDrawableW;
            h = jhLastDrawableH;
        }
        w = std::max(1, w);
        h = std::max(1, h);
        if (!SDL_GL_MakeCurrent(window, glContext)) {
            jhBannerFile = std::string("SDL_GL_MakeCurrent failed before UI render: ") + SDL_GetError();
        } else {
            if (jhBannerFile.rfind("SDL_GL_MakeCurrent failed before UI render", 0) == 0
                || jhBannerFile.rfind("SDL_GL_MakeCurrent failed before NewFrame", 0) == 0) {
                jhBannerFile.clear();
            }
            glViewport(0, 0, w, h);
            glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            LibUI::Core::Render();
            SDL_GL_SwapWindow(window);
        }

    }
    jackhammerEnginePreviewTex.Destroy();
    Solstice::EditorEnginePreview::Shutdown();
    Solstice::EditorAudio::Shutdown();
    LibUI::Core::Shutdown();
    LibUI::Shell::DestroyUtilityGlWindow(gw);
    LibUI::Shell::ShutdownUtilitySdlVideo();
    return 0;
}

} // namespace Jackhammer
