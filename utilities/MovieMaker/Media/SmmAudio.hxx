#pragma once

#include "LibUI/FileDialogs/FileDialogs.hxx"

#include <Parallax/DevSessionAssetResolver.hxx>
#include <Parallax/ParallaxScene.hxx>

#include <cstdint>
#include <string>

namespace Smm::Audio {

extern const LibUI::FileDialogs::FileFilter kAudioImportFilters[1];
extern const LibUI::FileDialogs::FileFilter kAudioExportFilters[1];

void QueueAudioImportPath(std::string pathUtf8);
void QueueAudioExportPath(std::string pathUtf8);

/// Import assigns `AudioAsset` on the selected timeline element when its schema is `AudioSourceElement`.
void DrainPendingAudioAssetOps(Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::DevSessionAssetResolver& resolver,
    int elementSelected, std::string& statusLine, bool compressPrlx, bool& sceneDirty);

/// Sets `Volume` and `Pitch` on a selected `AudioSourceElement` (no undo push).
bool TrySetAudioSourceMix(Solstice::Parallax::ParallaxScene& scene, int elementSelected, float volume, float pitch,
    std::string& errOut);

/// Waveform + scrub/playback preview (requires `Solstice::EditorAudio::Init` for the app).
void DrawAudioSourceClipPreview(
    Solstice::Parallax::DevSessionAssetResolver& resolver, std::uint64_t audioContentHash, float deltaSeconds, std::string& statusLine);

} // namespace Smm::Audio
