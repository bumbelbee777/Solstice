#include "Media/SmmImage.hxx"
#include "SmmFileOps.hxx"
#include "SmmView.hxx"

#include <Parallax/MGRaster.hxx>
#include <Parallax/ParallaxScene.hxx>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Smm::Image {

const LibUI::FileDialogs::FileFilter kRasterImportFilters[1] = {
    {"Raster image", "png;jpg;jpeg;bmp;tga;gif;hdr;psd;pic;ppm;pgm;webp"},
};

const LibUI::FileDialogs::FileFilter kRasterExportFilters[1] = {
    {"PNG (raw session bytes)", "png"},
};

namespace {
std::mutex g_RasterMutex;
std::optional<std::string> g_PendingRasterImport;
std::optional<std::string> g_PendingRasterExport;

static std::string_view MgSchemaType(const Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::MGIndex i) {
    const auto& mgs = scene.GetMGElements();
    if (i >= mgs.size()) {
        return {};
    }
    const uint32_t si = mgs[i].SchemaIndex;
    const auto& schemas = scene.GetSchemas();
    if (si >= schemas.size()) {
        return {};
    }
    return schemas[si].TypeName;
}
} // namespace

void QueueRasterImportPath(std::string pathUtf8) {
    std::lock_guard<std::mutex> lock(g_RasterMutex);
    g_PendingRasterImport = std::move(pathUtf8);
}

void QueueRasterExportPath(std::string pathUtf8) {
    std::lock_guard<std::mutex> lock(g_RasterMutex);
    g_PendingRasterExport = std::move(pathUtf8);
}

bool ProbeRasterFileDimensions(const std::string& pathUtf8, int& outW, int& outH) {
    outW = 0;
    outH = 0;
    std::ifstream f(pathUtf8, std::ios::binary);
    if (!f) {
        return false;
    }
    f.seekg(0, std::ios::end);
    const auto sz = f.tellg();
    f.seekg(0);
    if (sz <= 0) {
        return false;
    }
    std::vector<std::byte> buf(static_cast<size_t>(sz));
    f.read(reinterpret_cast<char*>(buf.data()), sz);
    if (!f) {
        return false;
    }
    std::vector<std::byte> rgba;
    return Solstice::Parallax::DecodeImageBytesToRgba(std::span<const std::byte>(buf.data(), buf.size()), rgba, outW, outH);
}

bool TrySetSpriteDisplaySize(Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::MGIndex mgIndex, float width,
    float height, std::string& errOut) {
    errOut.clear();
    auto& mgs = scene.GetMGElements();
    if (mgIndex >= mgs.size()) {
        errOut = "MG element index out of range.";
        return false;
    }
    if (MgSchemaType(scene, mgIndex) != "MGSpriteElement") {
        errOut = "Selected MG row is not an MGSpriteElement.";
        return false;
    }
    int iw = static_cast<int>(std::lround(std::max(width, 1.0f)));
    int ih = static_cast<int>(std::lround(std::max(height, 1.0f)));
    Smm::ClampMgRasterSize(iw, ih);
    mgs[mgIndex].Attributes["Size"] = Solstice::Parallax::AttributeValue{Solstice::Math::Vec2(static_cast<float>(iw),
        static_cast<float>(ih))};
    return true;
}

void DrainPendingRasterOps(Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::DevSessionAssetResolver& resolver,
    int mgElementSelected, bool fitSizeToImage, std::string& statusLine, bool compressPrlx, bool& sceneDirty) {
    std::optional<std::string> imp;
    std::optional<std::string> exp;
    {
        std::lock_guard<std::mutex> lock(g_RasterMutex);
        imp = std::move(g_PendingRasterImport);
        exp = std::move(g_PendingRasterExport);
    }

    if (imp) {
        if (mgElementSelected < 0 || static_cast<size_t>(mgElementSelected) >= scene.GetMGElements().size()) {
            statusLine = "Select an MG element row before importing a raster texture.";
        } else {
            const Solstice::Parallax::MGIndex mg = static_cast<Solstice::Parallax::MGIndex>(mgElementSelected);
            if (MgSchemaType(scene, mg) != "MGSpriteElement") {
                statusLine = "Select an MGSpriteElement row (add MG Sprite) to assign a Texture asset.";
            } else if (const auto h = resolver.ImportFile(std::filesystem::path(*imp))) {
                Smm::PushSceneUndoSnapshot(scene, compressPrlx);
                scene.GetMGElements()[mg].Attributes["Texture"] = Solstice::Parallax::AttributeValue{*h};
                if (fitSizeToImage) {
                    Solstice::Parallax::AssetData ad;
                    int iw = 0;
                    int ih = 0;
                    if (resolver.Resolve(*h, ad) && !ad.Bytes.empty()) {
                        std::vector<std::byte> rgba;
                        if (Solstice::Parallax::DecodeImageBytesToRgba(
                                std::span<const std::byte>(ad.Bytes.data(), ad.Bytes.size()), rgba, iw, ih)) {
                            std::string fitErr;
                            (void)TrySetSpriteDisplaySize(scene, mg, static_cast<float>(iw), static_cast<float>(ih), fitErr);
                        }
                    }
                }
                sceneDirty = true;
                statusLine = "Imported raster to selected MG sprite: " + *imp;
            } else {
                statusLine = "Raster import failed: could not read file: " + *imp;
            }
        }
    }

    if (exp) {
        if (mgElementSelected < 0 || static_cast<size_t>(mgElementSelected) >= scene.GetMGElements().size()) {
            statusLine = "Select an MG element row before exporting a raster texture.";
        } else {
            const Solstice::Parallax::MGIndex mg = static_cast<Solstice::Parallax::MGIndex>(mgElementSelected);
            if (MgSchemaType(scene, mg) != "MGSpriteElement") {
                statusLine = "Select an MGSpriteElement row to export its Texture session bytes.";
            } else {
                const auto itTex = scene.GetMGElements()[mg].Attributes.find("Texture");
                if (itTex == scene.GetMGElements()[mg].Attributes.end()) {
                    statusLine = "Selected MG sprite has no Texture attribute.";
                } else {
                    const Solstice::Parallax::AttributeValue& av = itTex->second;
                    const uint64_t* ph = std::get_if<uint64_t>(&av);
                    if (!ph || *ph == 0) {
                        statusLine = "Selected MG sprite has no Texture asset hash.";
                    } else {
                        Solstice::Parallax::AssetData ad{};
                        if (!resolver.Resolve(*ph, ad) || ad.Bytes.empty()) {
                            statusLine = "Texture asset is not in the current session.";
                        } else {
                            const std::filesystem::path dstPath(*exp);
                            std::error_code ec;
                            if (!dstPath.parent_path().empty()) {
                                std::filesystem::create_directories(dstPath.parent_path(), ec);
                            }
                            std::ofstream out(dstPath, std::ios::binary | std::ios::trunc);
                            if (!out) {
                                statusLine = "Failed to open raster export path: " + *exp;
                            } else {
                                out.write(reinterpret_cast<const char*>(ad.Bytes.data()),
                                    static_cast<std::streamsize>(ad.Bytes.size()));
                                if (!out) {
                                    statusLine = "Failed to write raster export.";
                                } else {
                                    statusLine = "Exported MG sprite Texture session bytes: " + *exp;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

} // namespace Smm::Image
