#include "VideoExport.hxx"

#include <Parallax/DevSessionAssetResolver.hxx>
#include <Parallax/MGRaster.hxx>
#include <Parallax/ParallaxScene.hxx>
#include <UI/Motion/MotionGraphicsCompositor.hxx>

#include "LibUI/Core/Core.hxx"

#include <imgui.h>

#include <SDL3/SDL_opengl.h>

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <span>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

namespace Solstice::MovieMaker {
namespace {

#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#endif
#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0 0x8CE0
#endif
#ifndef GL_FRAMEBUFFER_COMPLETE
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#endif
#ifndef GL_DRAW_FRAMEBUFFER
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#endif

using PFN_glGenFramebuffers = void (*)(GLsizei n, GLuint* ids);
using PFN_glBindFramebuffer = void (*)(GLenum target, GLuint framebuffer);
using PFN_glFramebufferTexture2D = void (*)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
    GLint level);
using PFN_glDeleteFramebuffers = void (*)(GLsizei n, const GLuint* framebuffers);
using PFN_glCheckFramebufferStatus = GLenum (*)(GLenum target);

static PFN_glGenFramebuffers s_glGenFramebuffers = nullptr;
static PFN_glBindFramebuffer s_glBindFramebuffer = nullptr;
static PFN_glFramebufferTexture2D s_glFramebufferTexture2D = nullptr;
static PFN_glDeleteFramebuffers s_glDeleteFramebuffers = nullptr;
static PFN_glCheckFramebufferStatus s_glCheckFramebufferStatus = nullptr;

static bool LoadFboProcs(std::string& err) {
    if (s_glGenFramebuffers) {
        return true;
    }
    s_glGenFramebuffers = reinterpret_cast<PFN_glGenFramebuffers>(SDL_GL_GetProcAddress("glGenFramebuffers"));
    s_glBindFramebuffer = reinterpret_cast<PFN_glBindFramebuffer>(SDL_GL_GetProcAddress("glBindFramebuffer"));
    s_glFramebufferTexture2D = reinterpret_cast<PFN_glFramebufferTexture2D>(SDL_GL_GetProcAddress("glFramebufferTexture2D"));
    s_glDeleteFramebuffers = reinterpret_cast<PFN_glDeleteFramebuffers>(SDL_GL_GetProcAddress("glDeleteFramebuffers"));
    s_glCheckFramebufferStatus = reinterpret_cast<PFN_glCheckFramebufferStatus>(SDL_GL_GetProcAddress("glCheckFramebufferStatus"));
    if (!s_glGenFramebuffers || !s_glBindFramebuffer || !s_glFramebufferTexture2D || !s_glDeleteFramebuffers
        || !s_glCheckFramebufferStatus) {
        err = "OpenGL framebuffer functions not available (need GL 3.0+ context).";
        return false;
    }
    return true;
}

struct GlFbo {
    GLuint fbo = 0;
    GLuint color = 0;
};

static bool CreateFbo(uint32_t w, uint32_t h, GlFbo& out, std::string& err) {
    glGenTextures(1, &out.color);
    glBindTexture(GL_TEXTURE_2D, out.color);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(w), static_cast<GLsizei>(h), 0, GL_RGBA,
        GL_UNSIGNED_BYTE, nullptr);

    s_glGenFramebuffers(1, &out.fbo);
    s_glBindFramebuffer(GL_FRAMEBUFFER, out.fbo);
    s_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, out.color, 0);
    GLenum st = s_glCheckFramebufferStatus(GL_FRAMEBUFFER);
    s_glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    if (st != GL_FRAMEBUFFER_COMPLETE) {
        err = "Framebuffer incomplete.";
        return false;
    }
    return true;
}

static void DestroyFbo(GlFbo& fb) {
    if (fb.fbo) {
        s_glDeleteFramebuffers(1, &fb.fbo);
        fb.fbo = 0;
    }
    if (fb.color) {
        glDeleteTextures(1, &fb.color);
        fb.color = 0;
    }
}

static ImTextureID TextureForHash(uint64_t hash, Solstice::Parallax::DevSessionAssetResolver& resolver,
    std::unordered_map<uint64_t, GLuint>& cache) {
    auto it = cache.find(hash);
    if (it != cache.end()) {
        return static_cast<ImTextureID>(it->second);
    }
    Solstice::Parallax::AssetData ad;
    if (!resolver.Resolve(hash, ad) || ad.Bytes.empty()) {
        return static_cast<ImTextureID>(0);
    }
    std::vector<std::byte> decoded;
    int w = 0;
    int h = 0;
    if (!Solstice::Parallax::DecodeImageBytesToRgba(std::span<const std::byte>(ad.Bytes.data(), ad.Bytes.size()), decoded, w, h)
        || w <= 0 || h <= 0 || decoded.empty()) {
        return static_cast<ImTextureID>(0);
    }
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, decoded.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    cache[hash] = tex;
    return static_cast<ImTextureID>(tex);
}

static void ReleaseTextureCache(std::unordered_map<uint64_t, GLuint>& cache) {
    for (auto& kv : cache) {
        glDeleteTextures(1, &kv.second);
    }
    cache.clear();
}

static FILE* OpenFfmpegPipe(const std::string& ffmpegExe, const VideoExportParams& params, std::string& err) {
    const uint32_t w = params.width;
    const uint32_t h = params.height;
    const uint32_t fps = std::max(1u, params.fps);
    std::string out = params.outputPath;
    for (char& c : out) {
        if (c == '\\') {
            c = '/';
        }
    }
    const char* mux = (params.container == VideoContainer::Mov) ? "mov" : "mp4";
    std::string args = "-y -hide_banner -loglevel error -f rawvideo -pixel_format rgba -video_size ";
    args += std::to_string(w) + "x" + std::to_string(h);
    args += " -framerate " + std::to_string(fps);
    args += " -i - -vf vflip -c:v libx264 -pix_fmt yuv420p -f ";
    args += mux;
    args += " \"";
    args += out;
    args += "\"";

    std::string cmd = "\"" + ffmpegExe + "\" " + args;
#ifdef _WIN32
    FILE* pipe = _popen(cmd.c_str(), "wb");
#else
    FILE* pipe = popen(cmd.c_str(), "w");
#endif
    if (!pipe) {
        err = "Failed to start ffmpeg process.\nCommand:\n" + cmd;
        return nullptr;
    }
#ifdef _WIN32
    _setmode(_fileno(pipe), _O_BINARY);
#endif
    return pipe;
}

} // namespace

bool ExportParallaxSceneToVideo(Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::DevSessionAssetResolver& resolver,
    SDL_Window* window, const VideoExportParams& params, std::string& errOut, const std::function<void(float)>& progress) {
    errOut.clear();
    if (params.ffmpegExecutable.empty()) {
        errOut = "ffmpeg executable path is empty.";
        return false;
    }
    if (params.width < 16 || params.height < 16 || params.width > 8192 || params.height > 8192) {
        errOut = "Export resolution out of range.";
        return false;
    }
    if (!LoadFboProcs(errOut)) {
        return false;
    }

    ImGuiContext* imguiCtx = LibUI::Core::GetContext();
    if (!imguiCtx) {
        errOut = "LibUI ImGui context not available.";
        return false;
    }

    const uint32_t tps = std::max(1u, scene.GetTicksPerSecond());
    uint64_t startTick = params.startTick;
    uint64_t endTick = params.endTick;
    if (endTick == 0) {
        endTick = scene.GetTimelineDurationTicks();
    }
    if (endTick <= startTick) {
        errOut = "Invalid tick range (end <= start).";
        return false;
    }

    const uint32_t fps = std::max(1u, params.fps);
    const double durationSec = static_cast<double>(endTick - startTick) / static_cast<double>(tps);
    uint32_t totalFrames = static_cast<uint32_t>(std::ceil(durationSec * static_cast<double>(fps)));
    if (totalFrames == 0) {
        totalFrames = 1;
    }

    std::string ffmpegCmdSummary;
    {
        const uint32_t w = params.width;
        const uint32_t h = params.height;
        const uint32_t fps = std::max(1u, params.fps);
        std::string out = params.outputPath;
        for (char& c : out) {
            if (c == '\\') {
                c = '/';
            }
        }
        const char* mux = (params.container == VideoContainer::Mov) ? "mov" : "mp4";
        ffmpegCmdSummary = "\"" + params.ffmpegExecutable + "\" -y ... rawvideo rgba " + std::to_string(w) + "x" + std::to_string(h)
            + " @ " + std::to_string(fps) + "fps -> " + mux + " \"" + out + "\"";
    }

    FILE* ff = OpenFfmpegPipe(params.ffmpegExecutable, params, errOut);
    if (!ff) {
        return false;
    }

    std::vector<uint8_t> rgba(static_cast<size_t>(params.width) * static_cast<size_t>(params.height) * 4);

    GlFbo fbo{};
    if (!CreateFbo(params.width, params.height, fbo, errOut)) {
#ifdef _WIN32
        _pclose(ff);
#else
        pclose(ff);
#endif
        return false;
    }

    GLint prevFbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);

    Solstice::UI::MotionGraphics::Compositor compositor;
    compositor.SetViewportSize(static_cast<float>(params.width), static_cast<float>(params.height));
    std::unordered_map<uint64_t, GLuint> texCache;

    ImGui::SetCurrentContext(imguiCtx);

    for (uint32_t fi = 0; fi < totalFrames; ++fi) {
        const double t = static_cast<double>(fi) / static_cast<double>(fps);
        uint64_t tick = startTick + static_cast<uint64_t>(std::llround(t * static_cast<double>(tps)));
        if (tick >= endTick) {
            tick = endTick - 1;
        }

        compositor.ClearTextureResolver();
        compositor.SetTextureResolver([&](uint64_t h) -> ImTextureID { return TextureForHash(h, resolver, texCache); });

        Solstice::Parallax::MGDisplayList mg = Solstice::Parallax::EvaluateMG(scene, tick);
        compositor.Submit(mg);

        s_glBindFramebuffer(GL_FRAMEBUFFER, fbo.fbo);
        glViewport(0, 0, static_cast<GLsizei>(params.width), static_cast<GLsizei>(params.height));
        glDisable(GL_DEPTH_TEST);
        glClearColor(0.f, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);

        LibUI::Core::NewFrameOffscreen(static_cast<float>(params.width), static_cast<float>(params.height),
            1.0f / static_cast<float>(fps));

        ImGui::Begin("##SMMExport", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNav);
        ImGui::SetWindowPos(ImVec2(0.f, 0.f));
        ImGui::SetWindowSize(ImVec2(static_cast<float>(params.width), static_cast<float>(params.height)));
        compositor.Render(ImGui::GetWindowDrawList());
        ImGui::End();

        LibUI::Core::RenderOffscreen();

        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, static_cast<GLsizei>(params.width), static_cast<GLsizei>(params.height), GL_RGBA, GL_UNSIGNED_BYTE,
            rgba.data());

        const size_t n = rgba.size();
        if (fwrite(rgba.data(), 1, n, ff) != n) {
            errOut = "Failed writing frame data to ffmpeg stdin.\n" + ffmpegCmdSummary;
            ReleaseTextureCache(texCache);
            DestroyFbo(fbo);
            s_glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prevFbo));
            glViewport(vp[0], vp[1], vp[2], vp[3]);
#ifdef _WIN32
            _pclose(ff);
#else
            pclose(ff);
#endif
            return false;
        }

        if (progress) {
            progress(static_cast<float>(fi + 1) / static_cast<float>(totalFrames));
        }
    }

#ifdef _WIN32
    const int ec = _pclose(ff);
#else
    const int ec = pclose(ff);
#endif
    if (ec != 0) {
        errOut = "ffmpeg exit code " + std::to_string(ec) + " (non-zero; verify the output file). Stderr was not captured from the pipe.\n"
            + ffmpegCmdSummary;
    }

    ReleaseTextureCache(texCache);
    DestroyFbo(fbo);
    s_glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prevFbo));
    glViewport(vp[0], vp[1], vp[2], vp[3]);

    if (window) {
        int ww = 0, wh = 0;
        SDL_GetWindowSize(window, &ww, &wh);
        glViewport(0, 0, ww, wh);
    }

    return true;
}

} // namespace Solstice::MovieMaker
