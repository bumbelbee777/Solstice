#pragma once

#include "LibUI/FileDialogs/FileDialogs.hxx"

#include <Parallax/DevSessionAssetResolver.hxx>
#include <Parallax/ParallaxScene.hxx>

#include <string>

namespace Smm {

extern const LibUI::FileDialogs::FileFilter kGltfFilters[1];

void QueueGltfImportPath(std::string p);
void QueueGltfExportPath(std::string p);

void DrainPendingGltfOps(Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::DevSessionAssetResolver& resolver,
    int elementSelected, std::string& smmLastError, bool compressPrlx, bool& sceneDirty);

} // namespace Smm
