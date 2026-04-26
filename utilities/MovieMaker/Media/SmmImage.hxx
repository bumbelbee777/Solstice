#pragma once

#include "LibUI/FileDialogs/FileDialogs.hxx"

#include <Parallax/DevSessionAssetResolver.hxx>
#include <Parallax/ParallaxScene.hxx>

#include <string>

namespace Smm::Image {

extern const LibUI::FileDialogs::FileFilter kRasterImportFilters[1];
extern const LibUI::FileDialogs::FileFilter kRasterExportFilters[1];

void QueueRasterImportPath(std::string pathUtf8);
void QueueRasterExportPath(std::string pathUtf8);

/// Applies pending import/export. Import assigns `Texture` on the selected MG row when its schema is `MGSpriteElement`.
/// When `fitSizeToImage`, decodes the source file to read pixel size and sets `Size` (clamped for MG raster budgets).
void DrainPendingRasterOps(Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::DevSessionAssetResolver& resolver,
    int mgElementSelected, bool fitSizeToImage, std::string& statusLine, bool compressPrlx, bool& sceneDirty);

/// Read width/height from a raster file (stb_image via LibUI). Returns false if decode fails.
bool ProbeRasterFileDimensions(const std::string& pathUtf8, int& outW, int& outH);

/// Sets `Size` on an `MGSpriteElement` MG row (no undo push). Validates schema and clamps dimensions.
bool TrySetSpriteDisplaySize(Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::MGIndex mgIndex, float width,
    float height, std::string& errOut);

} // namespace Smm::Image
