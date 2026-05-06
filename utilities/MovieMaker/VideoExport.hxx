#pragma once

#include <Math/Vector.hxx>

#include <Parallax/ParallaxScene.hxx>

#include <SDL3/SDL.h>

#include <functional>
#include <string>

namespace LibUI::Viewport {
struct OrbitPanZoomState;
}

namespace Solstice::Parallax {
class DevSessionAssetResolver;
}

namespace Smm::Editing {
struct ParticleEditorState;
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
    /// When true, tries to mux first scene AudioSourceElement asset into final video.
    bool muxFirstSceneAudio = true;

    /// Enable disk-backed frame cache. On a hit, the renderer is bypassed for that frame.
    bool enableFrameCache = false;
    /// Optional cache root. When empty and `enableFrameCache` is true, defaults to
    /// `<temp>/Solstice/SmmFrameCache/<projectFingerprintHex>/`.
    std::string frameCachePath;
    /// Hard cap on cached blob bytes; eviction enforces this.
    uint64_t frameCacheBudgetBytes = 4ull * 1024ull * 1024ull * 1024ull; // 4 GiB
    /// Time-to-live in seconds; 0 disables TTL.
    uint64_t frameCacheTtlSeconds = 7ull * 24ull * 60ull * 60ull; // 7 days
    /// Stable fingerprint of the .prlx project bytes so cache hits survive across runs.
    /// When 0, the cache uses the timestamped session id derived in BeginParallaxSceneVideoExport.
    uint64_t projectFingerprint = 0;
};

/// Optional unified-viewport parity: deterministic CPU particle sim + raster during export (`particleEditor` may be nullptr).
struct VideoExportParticleSettings {
    /** When non-null, export uses this orbit/zoom/framing instead of resetting to defaults (viewport WYSIWYG). */
    const LibUI::Viewport::OrbitPanZoomState* viewportOrbitForMatch = nullptr;
    const Smm::Editing::ParticleEditorState* particleEditor = nullptr;
    bool emitterWorldManual = false;
    Solstice::Math::Vec3 emitterWorld{};
};

struct IncrementalVideoExportSession;

/**
 * Starts an incremental export session. Call `StepParallaxSceneVideoExport` each frame until done.
 * On failure, `errOut` is populated and no session is created.
 */
bool BeginParallaxSceneVideoExport(Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::DevSessionAssetResolver& resolver,
    SDL_Window* window, const VideoExportParams& params, IncrementalVideoExportSession*& outSession, std::string& errOut,
    const VideoExportParticleSettings* particleSettings = nullptr);

/**
 * Exports one frame worth of work. Returns false on failure.
 * `outDone` becomes true when the export (including optional audio mux) has finished.
 */
bool StepParallaxSceneVideoExport(IncrementalVideoExportSession& session, Solstice::Parallax::ParallaxScene& scene,
    Solstice::Parallax::DevSessionAssetResolver& resolver, SDL_Window* window, float& outProgress, bool& outDone, std::string& errOut);

/// Cancels an incremental export and frees resources.
void CancelParallaxSceneVideoExport(IncrementalVideoExportSession*& session);

/** Blocking convenience wrapper around Begin+Step+Cancel. Call from the main thread. */
bool ExportParallaxSceneToVideo(Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::DevSessionAssetResolver& resolver,
    SDL_Window* window, const VideoExportParams& params, std::string& errOut,
    const std::function<void(float progress)>& progress = {},
    const VideoExportParticleSettings* particleSettings = nullptr);

} // namespace Solstice::MovieMaker
