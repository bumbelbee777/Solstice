#pragma once

#include <Parallax/ParallaxScene.hxx>

#include <SDL3/SDL.h>

#include <functional>
#include <string>

namespace Solstice::Parallax {
class DevSessionAssetResolver;
}

namespace Solstice::MovieMaker {

enum class VideoContainer {
    Mp4,
    Mov,
};

struct VideoExportParams {
    uint32_t width = 1280;
    uint32_t height = 720;
    uint32_t fps = 30;
    uint64_t startTick = 0;
    /** 0 = use scene timeline end */
    uint64_t endTick = 0;
    VideoContainer container = VideoContainer::Mp4;
    std::string outputPath;
    std::string ffmpegExecutable;
};

/** Renders MG via ImGui/OpenGL (current context) and encodes with ffmpeg CLI. Call from the main thread. */
bool ExportParallaxSceneToVideo(Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::DevSessionAssetResolver& resolver,
    SDL_Window* window, const VideoExportParams& params, std::string& errOut,
    const std::function<void(float progress)>& progress = {});

} // namespace Solstice::MovieMaker
