#pragma once

#include "LibUI/AssetBrowser/AssetBrowser.hxx"
#include "LibUI/Core/Core.hxx"
#include "LibUI/FileDialogs/FileDialogs.hxx"
#include "LibUI/Undo/SnapshotStack.hxx"
#include "Workflow/Workflow.hxx"

#include <Parallax/DevSessionAssetResolver.hxx>
#include <Parallax/ParallaxScene.hxx>

#include "UtilityPluginHost/UtilityPluginHost.hxx"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Smm {

struct MovieMakerProjectState {
    std::string exportPath;
    std::string importPath;
    std::string folderPath;
    std::string ffmpegExe;
    std::vector<std::string> recentPrlx;
    std::optional<std::string> loadedFromPath;
    std::string videoExportPath;
    uint32_t videoWidth = 1280;
    uint32_t videoHeight = 720;
    uint32_t videoFps = 30;
    bool videoMp4 = true;
    uint64_t videoStartTick = 0;
    uint64_t videoEndTick = 0;
    bool compressPrlx = false;
    std::string sessionNotes;
    std::vector<std::pair<uint64_t, std::string>> timelineMarkers;
    bool loopRegionEnabled = false;
    uint64_t loopRegionStartTick = 0;
    uint64_t loopRegionEndTick = 0;
    uint32_t playbackPermille = 1000;
};

extern const LibUI::FileDialogs::FileFilter kSmmProjectFilters[2];
extern const LibUI::FileDialogs::FileFilter kRelicFilters[2];

std::filesystem::path MovieMakerDefaultProjectPath();
void SaveMovieMakerProjectToPath(const std::filesystem::path& path, const MovieMakerProjectState& st);
bool LoadMovieMakerProjectFromPath(const std::filesystem::path& path, MovieMakerProjectState& st);
void ApplyMovieMakerProjectState(MovieMakerProjectState& st, char* exportPathBuf, size_t exportPathBufSize,
    char* importPathBuf, size_t importPathBufSize, char* folderPathBuf, size_t folderPathBufSize, char* ffmpegExeBuf,
    size_t ffmpegExeBufSize, char* videoExportPathBuf, size_t videoExportPathBufSize, uint32_t& videoW, uint32_t& videoH,
    uint32_t& videoFps, bool& videoMp4, uint64_t& videoStartTick, uint64_t& videoEndTick, bool& compressPrlx,
    std::vector<std::string>& recentPrlxPaths, std::filesystem::path& activeProjectPath, char* sessionNotesBuf,
    size_t sessionNotesBufSize, std::vector<std::pair<uint64_t, std::string>>* timelineMarkersOut, bool& loopRegionEnabled,
    uint64_t& loopRegionStartTick, uint64_t& loopRegionEndTick, uint32_t& playbackPermilleOut);

extern std::mutex g_ProjectMutex;
extern std::optional<MovieMakerProjectState> g_PendingProjectApply;
void DrainPendingMovieMakerProject(char* exportPathBuf, size_t exportPathBufSize, char* importPathBuf,
    size_t importPathBufSize, char* folderPathBuf, size_t folderPathBufSize, char* ffmpegExeBuf, size_t ffmpegExeBufSize,
    char* videoExportPathBuf, size_t videoExportPathBufSize, uint32_t& videoW, uint32_t& videoH, uint32_t& videoFps,
    bool& videoMp4, uint64_t& videoStartTick, uint64_t& videoEndTick, bool& compressPrlx,
    std::vector<std::string>& recentPrlxPaths, std::filesystem::path& activeProjectPath, char* sessionNotesBuf,
    size_t sessionNotesBufSize, std::vector<std::pair<uint64_t, std::string>>* timelineMarkersOut, bool& loopRegionEnabled,
    uint64_t& loopRegionStartTick, uint64_t& loopRegionEndTick, uint32_t& playbackPermilleOut);

void QueuePath(std::string p);
void QueueRelicImportPath(std::string p);
void QueueRelicExportPath(std::string p);
void DrainImports(Solstice::Parallax::DevSessionAssetResolver& resolver, std::vector<LibUI::AssetBrowser::Entry>& browser,
    Solstice::MovieMaker::Workflow::SessionMilestones* milestones);
void DrainRelicFileOps(Solstice::Parallax::DevSessionAssetResolver& resolver,
    std::vector<LibUI::AssetBrowser::Entry>& browser, std::string& smmLastError,
    Solstice::MovieMaker::Workflow::SessionMilestones* milestones);
void ImportFolderRecursive(const std::filesystem::path& dir, Solstice::Parallax::DevSessionAssetResolver& resolver,
    std::vector<LibUI::AssetBrowser::Entry>& browser, Solstice::MovieMaker::Workflow::SessionMilestones* milestones);

Solstice::Parallax::ChannelIndex FindChannelForAttribute(Solstice::Parallax::ParallaxScene& scene,
    Solstice::Parallax::ElementIndex element, std::string_view attribute, Solstice::Parallax::AttributeType type);
void PushRecentPath(std::vector<std::string>& recent, const std::string& p, size_t maxN = 8);

inline constexpr std::size_t kSceneUndoMax = 24;
extern LibUI::Undo::SnapshotStack<std::vector<std::byte>> g_sceneByteUndo;
void ClearSceneUndo();
void PushSceneUndoSnapshot(const Solstice::Parallax::ParallaxScene& scene, bool compressPrlx);
/** Non-null after `PushSceneUndoSnapshot` failed to retain a byte snapshot (typically OOM). Cleared with `ClearPendingUndoSnapshotMessage`. */
const char* GetPendingUndoSnapshotMessage();
void ClearPendingUndoSnapshotMessage();
bool SceneUndoRedoApply(Solstice::Parallax::ParallaxScene& scene, bool compressPrlx, bool undoNotRedo, int& elementSelected,
    uint64_t& timeTicks, bool loopRegionEnabled, uint64_t loopRegionStartTick, uint64_t loopRegionEndTick);

extern Solstice::UtilityPluginHost::UtilityPluginHost g_MovieMakerPlugins;
extern std::vector<std::pair<std::string, std::string>> g_MovieMakerPluginLoadErrors;
void LoadMovieMakerPlugins();
void MovieMakerPluginsDrawPanel(bool* pOpen);

struct SmmImportPathTarget {
    char* buf;
    size_t cap;
};
void SmmOpenLibUiRecentPath(void* userData, const char* pathUtf8);

const char* ParallaxErrorLabel(Solstice::Parallax::ParallaxError e);

void JumpPlayheadToStartWithLoop(uint64_t& playhead, uint64_t timelineDurationTicks, bool loopEnabled, uint64_t loopStartTick,
    uint64_t loopEndTick);
void JumpPlayheadToEndWithLoop(uint64_t& playhead, uint64_t timelineDurationTicks, bool loopEnabled, uint64_t loopStartTick,
    uint64_t loopEndTick);

#if defined(_WIN32)
void CopyAsciiToSystemClipboard(const std::string& s);
#endif

} // namespace Smm
