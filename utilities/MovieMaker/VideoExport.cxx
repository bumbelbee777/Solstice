#include "VideoExport.hxx"

#include <Arzachel/FacialAnimation.hxx>
#include <Parallax/DevSessionAssetResolver.hxx>
#include <Parallax/MGRaster.hxx>
#include <Parallax/ParallaxScene.hxx>
#include <UI/Motion/MotionGraphicsCompositor.hxx>

#include "Cache/FrameCache.hxx"
#include "Editing/SmmParticleEditor.hxx"
#include "EditorEnginePreview/EditorEnginePreview.hxx"
#include "Export/ExportPipeline.hxx"
#include "Export/SharedPreviewMapping.hxx"
#include "LibUI/Core/Core.hxx"
#include "LibUI/Tools/DiagLog.hxx"
#include "LibUI/Viewport/Viewport.hxx"
#include "LibUI/Viewport/ViewportMath.hxx"
#include "Media/SmmAudioMix.hxx"

#include <imgui.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <mutex>
#include <span>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

namespace Solstice::MovieMaker {

namespace EP = ::Solstice::MovieMaker::ExportPipeline;

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

static void ReleaseTextureCache(std::unordered_map<uint64_t, GLuint>& cache);

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
    constexpr size_t kMaxVideoTexCache = 128;
    if (cache.size() >= kMaxVideoTexCache) {
        ReleaseTextureCache(cache);
    }
    cache[hash] = tex;
    return static_cast<ImTextureID>(tex);
}

static std::string ReadSmallTextFile(const std::filesystem::path& p) {
    std::error_code ec;
    if (!std::filesystem::exists(p, ec) || ec) {
        return {};
    }
    std::ifstream in(p, std::ios::binary);
    if (!in) {
        return {};
    }
    std::string s((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (s.size() > 20000) {
        s.resize(20000);
        s += "\n... (truncated)";
    }
    return s;
}

static std::string BuildFfmpegStderrPath() {
    std::error_code ec;
    const std::filesystem::path tdir = std::filesystem::temp_directory_path(ec);
    const std::filesystem::path base = ec ? std::filesystem::current_path() : tdir;
    const auto ms = static_cast<unsigned long long>(SDL_GetTicks());
    return (base / ("smm_ffmpeg_stderr_" + std::to_string(ms) + ".log")).string();
}

static void ReleaseTextureCache(std::unordered_map<uint64_t, GLuint>& cache) {
    for (auto& kv : cache) {
        glDeleteTextures(1, &kv.second);
    }
    cache.clear();
}

static std::string TrimOuterQuotesAndWhitespace(std::string s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r' || s.front() == '\n')) {
        s.erase(s.begin());
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) {
        s.pop_back();
    }
    if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\''))) {
        s = s.substr(1, s.size() - 2);
    }
    return s;
}

static FILE* OpenFfmpegPipe(
    const std::string& ffmpegExe, const VideoExportParams& params, const std::string& stderrPath, std::string& err) {
    const uint32_t w = params.width;
    const uint32_t h = params.height;
    const uint32_t fps = std::max(1u, params.fps);
    std::string exe = TrimOuterQuotesAndWhitespace(ffmpegExe);
    std::string out = TrimOuterQuotesAndWhitespace(params.outputPath);
    if (exe.empty()) {
        err = "ffmpeg executable path is empty.";
        return nullptr;
    }
    if (out.empty()) {
        err = "Video output path is empty.";
        return nullptr;
    }
    try {
        const std::filesystem::path parent = std::filesystem::path(out).parent_path();
        if (!parent.empty()) {
            std::filesystem::create_directories(parent);
        }
    } catch (const std::exception& e) {
        err = std::string("Failed to create video output directory: ") + e.what();
        return nullptr;
    }
    for (char& c : out) {
        if (c == '\\') {
            c = '/';
        }
    }
    const char* mux = (params.container == VideoContainer::Mov) ? "mov" : "mp4";
    std::string args = "-y -hide_banner -loglevel error -f rawvideo -pix_fmt rgba -video_size ";
    args += std::to_string(w) + "x" + std::to_string(h);
    args += " -framerate " + std::to_string(fps);
    // Use built-in MPEG-4 encoder for broader ffmpeg compatibility (libx264 is often missing).
    args += " -i - -vf vflip -c:v mpeg4 -q:v 4 -pix_fmt yuv420p -f ";
    args += mux;
    args += " \"";
    args += out;
    args += "\"";

    std::string cmdCore = "\"" + exe + "\" " + args;
    if (!stderrPath.empty()) {
        cmdCore += " 2> \"";
        cmdCore += stderrPath;
        cmdCore += "\"";
    }
    std::string cmd = cmdCore;
#ifdef _WIN32
    // _popen executes via cmd.exe; wrapping with /s /c keeps nested quotes valid.
    cmd = "cmd /s /c \"" + cmdCore + "\"";
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

static int CloseFfmpegPipe(FILE* f) {
    if (!f) {
        return 0;
    }
#ifdef _WIN32
    return _pclose(f);
#else
    return pclose(f);
#endif
}

static void RestoreGlAfterOffscreen(SDL_Window* window, GLint prevFbo, const GLint vp[4]) {
    s_glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prevFbo));
    glViewport(vp[0], vp[1], vp[2], vp[3]);
    if (window) {
        int ww = 0, wh = 0;
        SDL_GetWindowSize(window, &ww, &wh);
        glViewport(0, 0, ww, wh);
    }
}

/// SIMD/scalar source-over blend (alpha-aware) over the whole frame. This replaces the legacy
/// scalar BlendMgOverScene that allocated/looped pixel-by-pixel; same numeric formula.
static void BlendMgOverScene(uint8_t* dstRgba, const uint8_t* srcRgba, std::size_t pixelCount, float globalAlpha) {
    EP::BlendOverRGBA(dstRgba, srcRgba, pixelCount, globalAlpha);
}

static bool IsMgSpriteWorldMode(const Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::MGIndex mgIndex) {
    if (mgIndex >= scene.GetMGElements().size()) {
        return false;
    }
    const auto& mg = scene.GetMGElements()[mgIndex];
    if (mg.SchemaIndex >= scene.GetSchemas().size() || scene.GetSchemas()[mg.SchemaIndex].TypeName != "MGSpriteElement") {
        return false;
    }
    const auto it = mg.Attributes.find("MGProjectionMode");
    if (it == mg.Attributes.end()) {
        return false;
    }
    if (const auto* mode = std::get_if<int32_t>(&it->second)) {
        return *mode == 1;
    }
    return false;
}

static Solstice::Parallax::MGDisplayList FilterWorldMgEntriesForRaster(
    const Solstice::Parallax::ParallaxScene& scene, const Solstice::Parallax::MGDisplayList& in) {
    Solstice::Parallax::MGDisplayList out = in;
    out.Entries.clear();
    out.Entries.reserve(in.Entries.size());
    const size_t n = (std::min)(in.Entries.size(), scene.GetMGElements().size());
    for (size_t i = 0; i < n; ++i) {
        if (IsMgSpriteWorldMode(scene, static_cast<Solstice::Parallax::MGIndex>(i))) {
            continue;
        }
        out.Entries.push_back(in.Entries[i]);
    }
    return out;
}

static Solstice::Math::Vec3 ReadElementVec3(const Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::ElementIndex ei,
    std::string_view key, const Solstice::Math::Vec3& fallback) {
    const Solstice::Parallax::AttributeValue v = Solstice::Parallax::GetAttribute(scene, ei, key);
    if (const auto* p = std::get_if<Solstice::Math::Vec3>(&v)) {
        return *p;
    }
    return fallback;
}

static float ReadElementFloat(const Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::ElementIndex ei,
    std::string_view key, float fallback) {
    const Solstice::Parallax::AttributeValue v = Solstice::Parallax::GetAttribute(scene, ei, key);
    if (const auto* f = std::get_if<float>(&v)) {
        return *f;
    }
    return fallback;
}

static Solstice::Math::Vec4 ReadElementVec4(const Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::ElementIndex ei,
    std::string_view key, const Solstice::Math::Vec4& fallback) {
    const Solstice::Parallax::AttributeValue v = Solstice::Parallax::GetAttribute(scene, ei, key);
    if (const auto* p = std::get_if<Solstice::Math::Vec4>(&v)) {
        return *p;
    }
    return fallback;
}

static std::string ReadElementString(const Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::ElementIndex ei,
    std::string_view key) {
    const Solstice::Parallax::AttributeValue v = Solstice::Parallax::GetAttribute(scene, ei, key);
    if (const auto* p = std::get_if<std::string>(&v)) {
        return *p;
    }
    return {};
}

static bool ReadElementBool(const Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::ElementIndex ei,
    std::string_view key, bool fallback) {
    const Solstice::Parallax::AttributeValue v = Solstice::Parallax::GetAttribute(scene, ei, key);
    if (const auto* p = std::get_if<bool>(&v)) {
        return *p;
    }
    return fallback;
}

static void ReadElementEulerDegrees(
    const Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::ElementIndex ei, float& outPitch, float& outYaw, float& outRoll) {
    outPitch = ReadElementFloat(scene, ei, "PitchDeg", ReadElementFloat(scene, ei, "PitchDegrees", 0.f));
    outYaw = ReadElementFloat(scene, ei, "YawDeg", ReadElementFloat(scene, ei, "YawDegrees", 0.f));
    outRoll = ReadElementFloat(scene, ei, "RollDeg", ReadElementFloat(scene, ei, "RollDegrees", 0.f));
}

static float PreviewMouthOpenHint(const Solstice::Parallax::SceneEvaluationResult& ev, Solstice::Parallax::ElementIndex ei) {
    for (const Solstice::Parallax::ActorFacialPose& fp : ev.ActorFacialPoses) {
        if (fp.Element != ei) {
            continue;
        }
        float m = 0.f;
        const auto jaw = fp.BoneDeltasByName.find("jaw");
        if (jaw != fp.BoneDeltasByName.end()) {
            m = (std::max)(m, std::clamp(-jaw->second.Translation.y * 12.f, 0.f, 1.f));
        }
        const uint32_t jh = Solstice::Arzachel::MorphNameHash("jaw_open");
        const auto mj = fp.MorphWeights.find(jh);
        if (mj != fp.MorphWeights.end()) {
            m = (std::max)(m, std::clamp(mj->second, 0.f, 1.f));
        }
        const uint32_t bh = Solstice::Arzachel::MorphNameHash("blink");
        const auto blink = fp.MorphWeights.find(bh);
        if (blink != fp.MorphWeights.end()) {
            m = (std::max)(m, std::clamp(blink->second * 0.25f, 0.f, 0.15f));
        }
        return m;
    }
    return 0.f;
}

static Solstice::Math::Vec3 RotateForwardByQuaternion(const Solstice::Math::Quaternion& qIn) {
    const Solstice::Math::Quaternion q = qIn.Normalized();
    const float vx = 0.f;
    const float vy = 0.f;
    const float vz = 1.f;
    const float tx = 2.f * (q.y * vz - q.z * vy);
    const float ty = 2.f * (q.z * vx - q.x * vz);
    const float tz = 2.f * (q.x * vy - q.y * vx);
    Solstice::Math::Vec3 out{};
    out.x = vx + q.w * tx + (q.y * tz - q.z * ty);
    out.y = vy + q.w * ty + (q.z * tx - q.x * tz);
    out.z = vz + q.w * tz + (q.x * ty - q.y * tx);
    return out.Normalized();
}

struct ExportCameraPose {
    LibUI::Viewport::OrbitPanZoomState Nav{};
    Solstice::Math::Vec3 Target{0.f, 0.f, 0.f};
    float FovYDeg{55.f};
};

static bool TryBuildSceneCameraPose(const Solstice::Parallax::ParallaxScene& scene,
    const Solstice::Parallax::SceneEvaluationResult& eval, ExportCameraPose& out) {
    for (const auto& et : eval.ElementTransforms) {
        if (Solstice::Parallax::GetElementSchema(scene, et.Element) != "CameraElement") {
            continue;
        }
        const Solstice::Math::Vec3 eye = et.Position;
        Solstice::Math::Vec3 dir = RotateForwardByQuaternion(et.Rotation);
        if (dir.Magnitude() <= 1e-4f) {
            dir = Solstice::Math::Vec3{0.f, 0.f, 1.f};
        }
        const Solstice::Math::Vec3 authoredTarget = ReadElementVec3(scene, et.Element, "Target", eye + dir * 8.f);
        const Solstice::Math::Vec3 eyeToTarget = authoredTarget - eye;
        if (eyeToTarget.Magnitude() > 0.05f) {
            dir = eyeToTarget.Normalized();
        }
        const float dist = std::clamp(eyeToTarget.Magnitude() > 0.05f ? eyeToTarget.Magnitude() : 8.f, 0.5f, 4096.f);
        out.Nav.projection = LibUI::Viewport::OrbitProjectionMode::Perspective;
        out.Nav.distance = dist;
        out.Nav.yaw = std::atan2(dir.x, dir.z);
        out.Nav.pitch = std::asin(std::clamp(dir.y, -1.f, 1.f));
        out.Nav.pan_x = 0.f;
        out.Nav.pan_y = 0.f;
        out.Target = eye + dir * dist;
        out.FovYDeg = std::clamp(ReadElementFloat(scene, et.Element, "FovDegrees", 55.f), 1.f, 179.f);
        return true;
    }
    return false;
}

static void BlendParticlePixelSrcOver(uint8_t* dp, float sr255, float sg255, float sb255, float alpha01) {
    alpha01 = std::clamp(alpha01, 0.f, 1.f);
    if (alpha01 < 1e-5f) {
        return;
    }
    const float inv = 1.f - alpha01;
    const float dr = static_cast<float>(dp[0]);
    const float dg = static_cast<float>(dp[1]);
    const float db = static_cast<float>(dp[2]);
    const float da255 = static_cast<float>(dp[3]);
    dp[0] = static_cast<uint8_t>(std::clamp(alpha01 * sr255 + inv * dr, 0.f, 255.f));
    dp[1] = static_cast<uint8_t>(std::clamp(alpha01 * sg255 + inv * dg, 0.f, 255.f));
    dp[2] = static_cast<uint8_t>(std::clamp(alpha01 * sb255 + inv * db, 0.f, 255.f));
    dp[3] = static_cast<uint8_t>(std::clamp(alpha01 * 255.f + inv * da255, 0.f, 255.f));
}

static Solstice::Math::Vec3 ResolveExportParticleEmitterWorld(const Solstice::Parallax::ParallaxScene& scene, uint64_t tick,
    const Smm::Editing::ParticleEditorState& st, bool manualEmitter, const Solstice::Math::Vec3& manualWorld) {
    if (manualEmitter) {
        return manualWorld;
    }
    const Solstice::Math::Vec3 kFallback{0.f, 1.2f, 0.f};
    if (!st.attachToSceneElement || st.attachElementIndex < 0) {
        return kFallback;
    }
    Solstice::Parallax::SceneEvaluationResult ev{};
    Solstice::Parallax::EvaluateScene(scene, tick, ev);
    const Solstice::Parallax::ElementIndex want = static_cast<Solstice::Parallax::ElementIndex>(st.attachElementIndex);
    for (const auto& et : ev.ElementTransforms) {
        if (et.Element == want) {
            return et.Position;
        }
    }
    return kFallback;
}

static void RasterizeParticleRibbonsCpu(const Smm::Editing::ParticleEditorState& particles, const LibUI::Viewport::Mat4Col& viewM,
    const LibUI::Viewport::Mat4Col& projM, const ImVec2& panelMin, const ImVec2& panelMax, uint32_t w, uint32_t h,
    uint8_t* dstRgba, std::size_t dstByteCount) {
    if (!particles.ribbonTrails) {
        return;
    }
    const size_t need = static_cast<size_t>(w) * static_cast<size_t>(h) * 4u;
    if (dstByteCount < need || w == 0 || h == 0) {
        return;
    }
    const float lineHalf = std::max(1.f, std::min(static_cast<float>(w), static_cast<float>(h)) * 0.0018f);
    const float thresh2 = lineHalf * lineHalf;
    for (const auto& p : particles.particles) {
        if (p.ribbonCount < 2) {
            continue;
        }
        const float ageNorm = std::clamp(p.age / (std::max)(p.lifetime, 1e-4f), 0.f, 1.f);
        for (uint8_t ri = 0; ri < p.ribbonCount - 1; ++ri) {
            ImVec2 scrA{};
            ImVec2 scrB{};
            const Solstice::Math::Vec3& aW = p.ribbon[ri];
            const Solstice::Math::Vec3& bW = p.ribbon[ri + 1];
            if (!LibUI::Viewport::WorldToScreen(viewM, projM, aW.x, aW.y, aW.z, panelMin, panelMax, scrA)) {
                continue;
            }
            if (!LibUI::Viewport::WorldToScreen(viewM, projM, bW.x, bW.y, bW.z, panelMin, panelMax, scrB)) {
                continue;
            }
            const float tseg = (static_cast<float>(ri) + 0.5f) / static_cast<float>(p.ribbonCount);
            const float tlife = ageNorm * 0.65f + 0.35f * tseg;
            float c4[4]{};
            Smm::Editing::SampleParticleColorOverLife(particles, tlife, c4);
            const float lineA = std::clamp(c4[3] * (200.f / 255.f), 0.f, 1.f);
            if (lineA < 1e-5f) {
                continue;
            }
            const float sr = std::clamp(c4[0] * 255.f, 0.f, 255.f);
            const float sg = std::clamp(c4[1] * 255.f, 0.f, 255.f);
            const float sb = std::clamp(c4[2] * 255.f, 0.f, 255.f);
            float ax = scrA.x;
            float ay = scrA.y;
            float bx = scrB.x;
            float by = scrB.y;
            if (bx < ax) {
                std::swap(ax, bx);
                std::swap(ay, by);
            }
            const int bbPad = static_cast<int>(std::ceil(lineHalf)) + 2;
            const float minX = (std::min)(ax, bx);
            const float maxX = (std::max)(ax, bx);
            const float minY = (std::min)(ay, by);
            const float maxY = (std::max)(ay, by);
            const int xMin = (std::max)(0, static_cast<int>(std::floor(minX)) - bbPad);
            const int xMax = (std::min)(static_cast<int>(w) - 1, static_cast<int>(std::ceil(maxX)) + bbPad);
            const int yMin = (std::max)(0, static_cast<int>(std::floor(minY)) - bbPad);
            const int yMax = (std::min)(static_cast<int>(h) - 1, static_cast<int>(std::ceil(maxY)) + bbPad);
            const float lax = ax;
            const float lay = ay;
            const float lbx = bx;
            const float lby = by;
            const float rdx = lbx - lax;
            const float rdy = lby - lay;
            const float abLen2 = rdx * rdx + rdy * rdy;
            for (int y = yMin; y <= yMax; ++y) {
                const float py = static_cast<float>(y) + 0.5f;
                for (int x = xMin; x <= xMax; ++x) {
                    const float px = static_cast<float>(x) + 0.5f;
                    float dist2;
                    if (abLen2 < 1e-10f) {
                        const float ex = px - lax;
                        const float ey = py - lay;
                        dist2 = ex * ex + ey * ey;
                    } else {
                        const float apx = px - lax;
                        const float apy = py - lay;
                        float tproj = (apx * rdx + apy * rdy) / abLen2;
                        tproj = std::clamp(tproj, 0.f, 1.f);
                        const float qx = lax + tproj * rdx;
                        const float qy = lay + tproj * rdy;
                        const float vx = px - qx;
                        const float vy = py - qy;
                        dist2 = vx * vx + vy * vy;
                    }
                    if (dist2 > thresh2) {
                        continue;
                    }
                    uint8_t* pxo = dstRgba + (static_cast<size_t>(y) * w + static_cast<size_t>(x)) * 4u;
                    BlendParticlePixelSrcOver(pxo, sr, sg, sb, lineA);
                }
            }
        }
    }
}

static void RasterizeParticleDisksCpu(const Smm::Editing::ParticleEditorState& particles, const LibUI::Viewport::Mat4Col& viewM,
    const LibUI::Viewport::Mat4Col& projM, const ImVec2& panelMin, const ImVec2& panelMax, uint32_t w, uint32_t h,
    uint8_t* dstRgba, std::size_t dstByteCount) {
    const size_t need = static_cast<size_t>(w) * static_cast<size_t>(h) * 4u;
    if (dstByteCount < need || w == 0 || h == 0) {
        return;
    }
    for (const auto& p : particles.particles) {
        ImVec2 sp{};
        if (!LibUI::Viewport::WorldToScreen(viewM, projM, p.position.x, p.position.y, p.position.z, panelMin, panelMax, sp)) {
            continue;
        }
        const float rad = std::max(2.f, p.size * 120.f);
        const float tLife = std::clamp(p.age / (std::max)(p.lifetime, 1e-4f), 0.f, 1.f);
        float c4[4]{};
        Smm::Editing::SampleParticleColorOverLife(particles, tLife, c4);
        const float sa = std::clamp(c4[3], 0.f, 1.f);
        if (sa < 1e-5f) {
            continue;
        }
        const float sr = std::clamp(c4[0] * 255.f, 0.f, 255.f);
        const float sg = std::clamp(c4[1] * 255.f, 0.f, 255.f);
        const float sb = std::clamp(c4[2] * 255.f, 0.f, 255.f);
        const float rOut = rad + 1.f;
        const float rIn2 = rad * rad;
        const float rOut2 = rOut * rOut;
        constexpr float kOutlineA = 160.f / 255.f;
        const int x0 = (std::max)(0, static_cast<int>(std::floor(static_cast<float>(sp.x - rOut))) - 1);
        const int x1 = (std::min)(static_cast<int>(w) - 1, static_cast<int>(std::ceil(static_cast<float>(sp.x + rOut))) + 1);
        const int y0 = (std::max)(0, static_cast<int>(std::floor(static_cast<float>(sp.y - rOut))) - 1);
        const int y1 = (std::min)(static_cast<int>(h) - 1, static_cast<int>(std::ceil(static_cast<float>(sp.y + rOut))) + 1);
        for (int y = y0; y <= y1; ++y) {
            const float py = static_cast<float>(y) + 0.5f;
            for (int x = x0; x <= x1; ++x) {
                const float px = static_cast<float>(x) + 0.5f;
                const float dx = px - static_cast<float>(sp.x);
                const float dy = py - static_cast<float>(sp.y);
                const float d2 = dx * dx + dy * dy;
                uint8_t* pxo = dstRgba + (static_cast<size_t>(y) * w + static_cast<size_t>(x)) * 4u;
                if (d2 <= rIn2) {
                    BlendParticlePixelSrcOver(pxo, sr, sg, sb, sa);
                } else if (d2 <= rOut2) {
                    BlendParticlePixelSrcOver(pxo, 20.f, 20.f, 30.f, kOutlineA * sa);
                }
            }
        }
    }
}

static void RasterizeParticleCpuOverlayIntoRgba(Smm::Editing::ParticleEditorState& particlesSim, const Solstice::Parallax::ParallaxScene& scene,
    uint64_t tick, float dtSeconds, uint32_t w, uint32_t h, const LibUI::Viewport::OrbitPanZoomState& nav, bool emitterManual,
    const Solstice::Math::Vec3& emitterManualWorld, uint8_t* dstRgba, std::size_t dstByteCount) {
    if (!particlesSim.enabled || !(dtSeconds > 0.f)) {
        return;
    }
    const Solstice::Math::Vec3 ew = ResolveExportParticleEmitterWorld(scene, tick, particlesSim, emitterManual, emitterManualWorld);
    Smm::Editing::TickParticlePreview(particlesSim, ew, dtSeconds);
    const float aspect = static_cast<float>(w) / static_cast<float>((std::max)(1u, h));
    LibUI::Viewport::Mat4Col viewM{};
    LibUI::Viewport::Mat4Col projM{};
    LibUI::Viewport::ComputeOrbitViewProjectionColMajor(nav, 0.f, 0.f, 0.f, 55.f, aspect, 0.12f, 2048.f, viewM, projM);
    const ImVec2 pmin(0.f, 0.f);
    const ImVec2 pmax(static_cast<float>(w), static_cast<float>(h));
    RasterizeParticleRibbonsCpu(particlesSim, viewM, projM, pmin, pmax, w, h, dstRgba, dstByteCount);
    RasterizeParticleDisksCpu(particlesSim, viewM, projM, pmin, pmax, w, h, dstRgba, dstByteCount);
}

} // namespace

/// Async ffmpeg writer thread state. Producer (export step) appends frames to a bounded queue;
/// the writer thread drains them with `fwrite`. Decouples render cadence from encoder backpressure
/// so a slow encoder no longer stalls the renderer for the whole frame budget.
struct EncoderWriter {
    /// Allocated byte slab for a single frame (RGBA8 packed top-down). `pixelCount * 4` bytes.
    struct FrameBuffer {
        std::vector<std::uint8_t> Bytes;
        std::uint32_t FrameIndex{0};
    };

    static constexpr std::size_t kQueueCapacity = 8;
    EP::LockFreeSpscQueue<std::shared_ptr<FrameBuffer>, kQueueCapacity> Queue;
    std::atomic<bool> Running{false};
    std::atomic<bool> StopFlag{false};
    std::atomic<bool> Failed{false};
    std::atomic<std::uint64_t> BytesWritten{0};
    std::atomic<std::uint64_t> MaxQueueDepth{0};
    std::string LastError;
    std::mutex ErrorMutex;
    std::thread Thread;
    FILE* Pipe{nullptr};
    std::string FfmpegCmdSummary;
    std::string FfmpegStderrPath;

    EncoderWriter() = default;
    EncoderWriter(const EncoderWriter&) = delete;
    EncoderWriter& operator=(const EncoderWriter&) = delete;
    ~EncoderWriter() {
        // Defensive: ensure the writer thread is always joined and the pipe closed even if
        // Stop() was not called explicitly (e.g. exception path through CancelParallaxSceneVideoExport).
        if (Thread.joinable()) {
            StopFlag.store(true, std::memory_order_release);
            Thread.join();
        }
        if (Pipe) {
            CloseFfmpegPipe(Pipe);
            Pipe = nullptr;
        }
    }

    void Start(FILE* pipe, std::string cmdSummary, std::string stderrPath) {
        Pipe = pipe;
        FfmpegCmdSummary = std::move(cmdSummary);
        FfmpegStderrPath = std::move(stderrPath);
        Running.store(true, std::memory_order_release);
        StopFlag.store(false, std::memory_order_release);
        Failed.store(false, std::memory_order_release);
        Thread = std::thread([this]() { Run(); });
    }

    void Run() {
        while (!StopFlag.load(std::memory_order_acquire) || !Queue.Empty()) {
            std::shared_ptr<FrameBuffer> fb;
            if (!Queue.TryPop(fb)) {
                std::this_thread::yield();
                continue;
            }
            if (!fb || !Pipe) {
                continue;
            }
            const size_t n = fb->Bytes.size();
            if (n == 0) {
                continue;
            }
            const size_t wrote = std::fwrite(fb->Bytes.data(), 1, n, Pipe);
            if (wrote != n) {
                std::lock_guard<std::mutex> lg(ErrorMutex);
                LastError = "Encoder writer failed to write " + std::to_string(n) + " bytes (wrote "
                    + std::to_string(wrote) + ").";
                Failed.store(true, std::memory_order_release);
                StopFlag.store(true, std::memory_order_release);
                return;
            }
            BytesWritten.fetch_add(static_cast<std::uint64_t>(n), std::memory_order_relaxed);
        }
        Running.store(false, std::memory_order_release);
    }

    /// Pushes a frame into the queue; blocks (yielding) until queue space or stop requested.
    bool Submit(std::shared_ptr<FrameBuffer> fb,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(60000)) {
        if (!fb) {
            return false;
        }
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (!Failed.load(std::memory_order_acquire)) {
            if (Queue.TryPush(fb)) {
                const std::uint64_t depth = static_cast<std::uint64_t>(Queue.SizeApprox());
                std::uint64_t prev = MaxQueueDepth.load(std::memory_order_relaxed);
                while (depth > prev && !MaxQueueDepth.compare_exchange_weak(prev, depth, std::memory_order_relaxed)) {
                    // retry on contention.
                }
                return true;
            }
            if (std::chrono::steady_clock::now() > deadline) {
                return false;
            }
            std::this_thread::yield();
        }
        return false;
    }

    int Stop() {
        StopFlag.store(true, std::memory_order_release);
        if (Thread.joinable()) {
            Thread.join();
        }
        Running.store(false, std::memory_order_release);
        const int ec = CloseFfmpegPipe(Pipe);
        Pipe = nullptr;
        return ec;
    }

    bool HasError(std::string& outErr) {
        if (!Failed.load(std::memory_order_acquire)) {
            return false;
        }
        std::lock_guard<std::mutex> lg(ErrorMutex);
        outErr = LastError;
        return true;
    }
};

struct IncrementalVideoExportSession {
    VideoExportParams Params{};
    std::string FfmpegCmdSummary;
    std::string FfmpegStderrPath;
    std::unique_ptr<EncoderWriter> Encoder;
    uint32_t Tps{1};
    uint32_t Fps{30};
    uint32_t TotalFrames{0};
    uint32_t FrameIndex{0};
    uint64_t StartTick{0};
    uint64_t EndTick{0};
    bool Finished{false};
    bool Failed{false};
    bool AudioMuxTried{false};

    /// Persistent per-export buffers (allocated once in BeginParallaxSceneVideoExport so frame
    /// production never allocates a full-frame buffer in a 4K/60+ inner loop).
    std::vector<std::byte> CaptureRgba;
    std::vector<std::byte> CompositeRgba;
    std::vector<std::byte> MgRgba;

    /// Frame arena for transient per-frame scratch (entity/light vectors, working buffers).
    EP::FrameArena ScratchArena;

    /// Stage stats, cumulatively updated.
    EP::StageStats Stats;

    /// Capture reliability: warmup + fail streak (mirrors unified viewport behavior).
    int WarmupFramesRemaining{2};
    int CaptureFailStreak{0};
    bool CaptureDisabled{false};
    int MaxCaptureFailStreakBeforeDisable{6};

    LibUI::Viewport::OrbitPanZoomState Nav{};
    bool ParticleExportActive{false};
    Smm::Editing::ParticleEditorState ParticleExportSim{};
    bool ParticleEmitterManual{false};
    Solstice::Math::Vec3 ParticleEmitterManualWorld{};

    /// Coverage diagnostics emitted once per export to DiagLog.
    bool CoverageReported{false};

    /// Disk-backed frame cache (LZX-compressed, mmap-backed). When `nullptr`, caching is off
    /// for this export. Key = `BuildFrameKey(projectFingerprint, tick, w, h, fps, postFp)`.
    std::unique_ptr<Solstice::MovieMaker::Cache::FrameCache> Cache;
    std::uint64_t ProjectFingerprint{0};
    /// Cached frame from the previous step; provides the base for delta encoding.
    std::uint64_t PrevCachedKey{0};
    std::vector<std::uint8_t> PrevCachedRgba;
    bool PrevCachedKeyValid{false};
};

namespace {

static bool TryMuxFirstSceneAudioFromAsset(const IncrementalVideoExportSession& session,
    const Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::DevSessionAssetResolver& resolver,
    std::string& warnOut) {
    warnOut.clear();
    if (!session.Params.muxFirstSceneAudio) {
        return true;
    }
    uint64_t firstAudioHash = 0;
    for (Solstice::Parallax::ElementIndex ei = 0; ei < scene.GetElements().size(); ++ei) {
        if (Solstice::Parallax::GetElementSchema(scene, ei) != "AudioSourceElement") {
            continue;
        }
        const Solstice::Parallax::AttributeValue av = Solstice::Parallax::GetAttribute(scene, ei, "AudioAsset");
        if (const auto* h = std::get_if<uint64_t>(&av); h && *h != 0) {
            firstAudioHash = *h;
            break;
        }
    }
    if (firstAudioHash == 0) {
        return true;
    }
    Solstice::Parallax::AssetData ad;
    if (!resolver.Resolve(firstAudioHash, ad) || ad.Bytes.empty()) {
        warnOut = "Audio mux skipped: AudioAsset bytes unavailable in resolver.";
        return true;
    }
    std::string ext = ".bin";
    if (ad.Bytes.size() >= 12 && std::memcmp(ad.Bytes.data(), "RIFF", 4) == 0
        && std::memcmp(reinterpret_cast<const char*>(ad.Bytes.data()) + 8, "WAVE", 4) == 0) {
        ext = ".wav";
    } else if (ad.Bytes.size() >= 4 && std::memcmp(ad.Bytes.data(), "fLaC", 4) == 0) {
        ext = ".flac";
    } else if (ad.Bytes.size() >= 4 && std::memcmp(ad.Bytes.data(), "OggS", 4) == 0) {
        ext = ".ogg";
    } else if (ad.Bytes.size() >= 3 && std::memcmp(ad.Bytes.data(), "ID3", 3) == 0) {
        ext = ".mp3";
    }
    std::error_code ec;
    const std::filesystem::path tempDir = std::filesystem::temp_directory_path(ec);
    const std::filesystem::path base = ec ? std::filesystem::current_path() : tempDir;
    const auto nonce = static_cast<unsigned long long>(SDL_GetTicks());
    const std::filesystem::path audioPath = base / ("smm_mux_audio_" + std::to_string(nonce) + ext);
    const std::filesystem::path muxOutPath = base / ("smm_mux_video_" + std::to_string(nonce) + ".mp4");
    {
        std::ofstream out(audioPath, std::ios::binary | std::ios::trunc);
        if (!out) {
            warnOut = "Audio mux skipped: could not write temp audio file.";
            return true;
        }
        out.write(reinterpret_cast<const char*>(ad.Bytes.data()), static_cast<std::streamsize>(ad.Bytes.size()));
    }
    const std::string exe = TrimOuterQuotesAndWhitespace(session.Params.ffmpegExecutable);
    std::string cmdCore = "\"" + exe + "\" -y -hide_banner -loglevel error -i \"" + session.Params.outputPath + "\" -i \""
        + audioPath.string() + "\" -c:v copy -c:a aac -shortest \"" + muxOutPath.string() + "\"";
    std::string stderrPath = BuildFfmpegStderrPath();
    cmdCore += " 2> \"" + stderrPath + "\"";
#ifdef _WIN32
    const std::string cmd = "cmd /s /c \"" + cmdCore + "\"";
    FILE* p = _popen(cmd.c_str(), "rb");
#else
    const std::string cmd = cmdCore;
    FILE* p = popen(cmd.c_str(), "r");
#endif
    if (!p) {
        warnOut = "Audio mux skipped: ffmpeg mux process failed to start.";
        return true;
    }
    char rb[512]{};
    while (fgets(rb, sizeof(rb), p)) {
    }
#ifdef _WIN32
    int ecMux = _pclose(p);
#else
    int ecMux = pclose(p);
#endif
    if (ecMux != 0 || !std::filesystem::exists(muxOutPath)) {
        warnOut = "Audio mux skipped: ffmpeg mux pass failed.";
        const std::string ffErr = ReadSmallTextFile(stderrPath);
        if (!ffErr.empty()) {
            warnOut += "\n" + ffErr;
        }
        return true;
    }
    std::filesystem::remove(session.Params.outputPath, ec);
    std::filesystem::rename(muxOutPath, session.Params.outputPath, ec);
    if (ec) {
        warnOut = "Audio mux warning: mux file created but replace failed (" + ec.message() + ").";
    }
    std::filesystem::remove(audioPath, ec);
    return true;
}

/// Multi-source evaluated mixdown: produces a temporary WAV with timeline-evaluated `Volume` and
/// `Pitch` automation across every `AudioSourceElement` in the scene, then muxes it into the video.
/// On any failure, falls back to the legacy first-asset mux to preserve backward-compatible
/// behavior. This is the audio-parity upgrade described in Phase 5 of the plan.
static bool TryMuxEvaluatedSceneAudio(const IncrementalVideoExportSession& session,
    const Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::DevSessionAssetResolver& resolver,
    std::string& warnOut) {
    warnOut.clear();
    if (!session.Params.muxFirstSceneAudio) {
        return true;
    }
    // First, try evaluated multi-source mixdown.
    std::error_code ec;
    const std::filesystem::path tempDir = std::filesystem::temp_directory_path(ec);
    const std::filesystem::path base = ec ? std::filesystem::current_path() : tempDir;
    const auto nonce = static_cast<unsigned long long>(SDL_GetTicks());
    const std::filesystem::path tmpWav = base / ("smm_mix_eval_" + std::to_string(nonce) + ".wav");
    const std::filesystem::path muxOutPath = base / ("smm_mux_eval_" + std::to_string(nonce) + ".mp4");

    Smm::Audio::OfflineMixdownParams mp{};
    mp.StartTick = session.StartTick;
    mp.EndTick = session.EndTick;
    mp.SampleRateHz = 48000;
    mp.AutomationFps = static_cast<int>(std::clamp<uint32_t>(session.Fps, 1u, 240u));
    mp.OutputWavPath = tmpWav;
    Smm::Audio::OfflineMixdownStats st{};
    std::string mixErr;
    const bool mixOk = Smm::Audio::RenderEvaluatedMixdownToWav(scene, resolver, mp, st, mixErr);
    if (!mixOk || !st.WroteFile) {
        // Fall back to legacy first-asset mux when the evaluated path can't run (no sources / decoder
        // unavailable / no editor audio). Behavior parity with previous releases.
        return TryMuxFirstSceneAudioFromAsset(session, scene, resolver, warnOut);
    }

    const std::string exe = TrimOuterQuotesAndWhitespace(session.Params.ffmpegExecutable);
    std::string cmdCore = "\"" + exe + "\" -y -hide_banner -loglevel error -i \"" + session.Params.outputPath + "\" -i \""
        + tmpWav.string() + "\" -c:v copy -c:a aac -shortest \"" + muxOutPath.string() + "\"";
    std::string stderrPath = BuildFfmpegStderrPath();
    cmdCore += " 2> \"" + stderrPath + "\"";
#ifdef _WIN32
    const std::string cmd = "cmd /s /c \"" + cmdCore + "\"";
    FILE* p = _popen(cmd.c_str(), "rb");
#else
    const std::string cmd = cmdCore;
    FILE* p = popen(cmd.c_str(), "r");
#endif
    if (!p) {
        std::filesystem::remove(tmpWav, ec);
        warnOut = "Audio mux skipped: ffmpeg mux process failed to start.";
        return true;
    }
    char rb[512]{};
    while (fgets(rb, sizeof(rb), p)) {
    }
#ifdef _WIN32
    int ecMux = _pclose(p);
#else
    int ecMux = pclose(p);
#endif
    if (ecMux != 0 || !std::filesystem::exists(muxOutPath)) {
        std::filesystem::remove(tmpWav, ec);
        warnOut = "Audio mux skipped: ffmpeg mux pass failed.";
        const std::string ffErr = ReadSmallTextFile(stderrPath);
        if (!ffErr.empty()) {
            warnOut += "\n" + ffErr;
        }
        return true;
    }
    std::filesystem::remove(session.Params.outputPath, ec);
    std::filesystem::rename(muxOutPath, session.Params.outputPath, ec);
    if (ec) {
        warnOut = "Audio mux warning: mux file created but replace failed (" + ec.message() + ").";
    }
    std::filesystem::remove(tmpWav, ec);
    return true;
}

} // namespace

bool BeginParallaxSceneVideoExport(Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::DevSessionAssetResolver& resolver,
    SDL_Window* window, const VideoExportParams& params, IncrementalVideoExportSession*& outSession, std::string& errOut,
    const VideoExportParticleSettings* particleSettings) {
    (void)window;
    (void)resolver;
    errOut.clear();
    outSession = nullptr;
    auto* s = new IncrementalVideoExportSession();
    s->Params = params;
    if (params.ffmpegExecutable.empty()) {
        errOut = "ffmpeg executable path is empty.";
        delete s;
        return false;
    }
    if (params.width < 16 || params.height < 16 || params.width > 8192 || params.height > 8192) {
        errOut = "Export resolution out of range (allowed 16..8192 per side).";
        delete s;
        return false;
    }
    s->Tps = std::max(1u, scene.GetTicksPerSecond());
    s->Fps = std::max(1u, params.fps);
    s->StartTick = params.startTick;
    s->EndTick = params.endTick == 0 ? scene.GetTimelineDurationTicks() : params.endTick;
    if (s->EndTick <= s->StartTick) {
        errOut = "Invalid tick range (end <= start).";
        delete s;
        return false;
    }
    const double durationSec = static_cast<double>(s->EndTick - s->StartTick) / static_cast<double>(s->Tps);
    s->TotalFrames = static_cast<uint32_t>(std::ceil(durationSec * static_cast<double>(s->Fps)));
    if (s->TotalFrames == 0) {
        s->TotalFrames = 1;
    }
    std::string out = params.outputPath;
    for (char& c : out) {
        if (c == '\\') {
            c = '/';
        }
    }
    const char* mux = (params.container == VideoContainer::Mov) ? "mov" : "mp4";
    s->FfmpegCmdSummary = "\"" + params.ffmpegExecutable + "\" -y ... rawvideo rgba " + std::to_string(params.width) + "x"
        + std::to_string(params.height) + " @ " + std::to_string(s->Fps) + "fps -> " + mux + " \"" + out + "\"";

    s->FfmpegStderrPath = BuildFfmpegStderrPath();
    {
        std::error_code ec;
        std::filesystem::remove(s->FfmpegStderrPath, ec);
    }
    FILE* pipe = OpenFfmpegPipe(params.ffmpegExecutable, params, s->FfmpegStderrPath, errOut);
    if (!pipe && params.ffmpegExecutable == "ffmpeg") {
        const char* b = SDL_GetBasePath();
        if (b) {
            std::filesystem::path bundled(b);
#if defined(_WIN32)
            bundled /= "ffmpeg.exe";
#else
            bundled /= "ffmpeg";
#endif
            if (std::filesystem::exists(bundled)) {
                pipe = OpenFfmpegPipe(bundled.string(), params, s->FfmpegStderrPath, errOut);
                if (pipe) {
                    s->Params.ffmpegExecutable = bundled.string();
                }
            }
        }
    }
    if (!pipe) {
        delete s;
        return false;
    }
    s->Encoder = std::make_unique<EncoderWriter>();
    s->Encoder->Start(pipe, s->FfmpegCmdSummary, s->FfmpegStderrPath);

    const std::size_t pixelCount = static_cast<std::size_t>(params.width) * static_cast<std::size_t>(params.height);
    const std::size_t byteCount = pixelCount * 4u;
    s->CaptureRgba.assign(byteCount, std::byte{0});
    s->CompositeRgba.assign(byteCount, std::byte{0});
    s->MgRgba.assign(byteCount, std::byte{0});
    // Frame arena: 64 MiB ceiling is enough for transient entity/light/MG vectors at any current
    // resolution, and is bounded so very long exports cannot grow unbounded.
    s->ScratchArena.Reserve(64u * 1024u * 1024u);

    if (particleSettings && particleSettings->viewportOrbitForMatch) {
        s->Nav = *particleSettings->viewportOrbitForMatch;
    } else {
        LibUI::Viewport::ResetOrbitPanZoom(s->Nav);
    }
    s->ParticleExportActive = false;
    if (particleSettings && particleSettings->particleEditor && particleSettings->particleEditor->enabled) {
        s->ParticleExportActive = true;
        s->ParticleExportSim = *particleSettings->particleEditor;
        s->ParticleExportSim.particles.clear();
        s->ParticleExportSim.accum = 0.f;
        s->ParticleEmitterManual = particleSettings->emitterWorldManual;
        s->ParticleEmitterManualWorld = particleSettings->emitterWorld;
    }
    if (params.enableFrameCache) {
        std::filesystem::path cacheRoot;
        if (!params.frameCachePath.empty()) {
            cacheRoot = params.frameCachePath;
        } else {
            std::error_code ec;
            const auto base = std::filesystem::temp_directory_path(ec);
            if (!ec) {
                char fpHex[32]{};
                const std::uint64_t fp = params.projectFingerprint != 0 ? params.projectFingerprint
                    : Cache::Fnv1a64(std::span<const std::byte>(
                          reinterpret_cast<const std::byte*>(params.outputPath.data()),
                          params.outputPath.size()));
                std::snprintf(fpHex, sizeof(fpHex), "%016llx", static_cast<unsigned long long>(fp));
                cacheRoot = base / "Solstice" / "SmmFrameCache" / fpHex;
            }
        }
        if (!cacheRoot.empty()) {
            Cache::FrameCacheOptions opts{};
            opts.BudgetBytes = params.frameCacheBudgetBytes;
            opts.TtlSeconds = params.frameCacheTtlSeconds;
            auto cache = std::make_unique<Cache::FrameCache>();
            if (cache->Open(cacheRoot, opts)) {
                s->Cache = std::move(cache);
                s->ProjectFingerprint = params.projectFingerprint;
                LibUI::Tools::DiagLogLine(std::string("[VideoExport] cache=open root=")
                    + cacheRoot.string()
                    + " budget=" + std::to_string(opts.BudgetBytes / (1024ull * 1024ull)) + "MiB"
                    + " ttl_s=" + std::to_string(opts.TtlSeconds)
                    + " entries=" + std::to_string(s->Cache->EntryCount()));
            } else {
                LibUI::Tools::DiagLogLine(std::string("[VideoExport] cache=open-failed root=") + cacheRoot.string());
            }
        }
    }

    LibUI::Tools::DiagLogLine(std::string("[VideoExport] start fmt=")
        + (params.container == VideoContainer::Mov ? "mov" : "mp4")
        + " " + std::to_string(params.width) + "x" + std::to_string(params.height)
        + "@" + std::to_string(s->Fps) + " frames=" + std::to_string(s->TotalFrames)
        + " simd=" + EP::SimdFeatureString()
        + " cache=" + (s->Cache ? "on" : "off"));

    outSession = s;
    return true;
}

bool StepParallaxSceneVideoExport(IncrementalVideoExportSession& s, Solstice::Parallax::ParallaxScene& scene,
    Solstice::Parallax::DevSessionAssetResolver& resolver, SDL_Window* window, float& outProgress, bool& outDone, std::string& errOut) {
    (void)window;
    errOut.clear();
    outProgress = 0.f;
    outDone = false;
    if (s.Finished || s.Failed) {
        outDone = true;
        outProgress = 1.f;
        return !s.Failed;
    }

    // Encoder failure is a hard abort.
    if (s.Encoder) {
        std::string encErr;
        if (s.Encoder->HasError(encErr)) {
            errOut = "ffmpeg writer thread reported failure: " + encErr;
            const std::string ffErr = ReadSmallTextFile(s.FfmpegStderrPath);
            if (!ffErr.empty()) {
                errOut += "\n\nffmpeg stderr:\n" + ffErr;
            }
            s.Failed = true;
            return false;
        }
    }

    const std::uint64_t tFrameStart = EP::MonoNowUs();

    s.ScratchArena.Reset();

    const uint32_t fi = s.FrameIndex;
    const double t = static_cast<double>(fi) / static_cast<double>(s.Fps);
    uint64_t tick = s.StartTick + static_cast<uint64_t>(std::llround(t * static_cast<double>(s.Tps)));
    if (tick >= s.EndTick) {
        tick = s.EndTick - 1;
    }

    // Frame cache lookup. The key blends `(projectFingerprint, tick, w, h, fps)`; rendering is
    // a pure function of the scene state at `tick` plus the export resolution/fps so an exact
    // match means the prior render is reusable bit-for-bit. On hit we skip evaluation, capture,
    // raster, composite, and post entirely and feed the cached bytes straight to the encoder.
    bool fromCache = false;
    const std::uint64_t cacheKey = s.Cache
        ? Cache::BuildFrameKey(s.ProjectFingerprint != 0 ? s.ProjectFingerprint
                                                          : static_cast<std::uint64_t>(s.TotalFrames),
              tick, s.Params.width, s.Params.height, s.Fps, /*postFp=*/0)
        : 0ull;
    if (s.Cache) {
        std::vector<std::uint8_t> cachedBytes;
        if (s.Cache->Lookup(cacheKey, cachedBytes)
            && cachedBytes.size() == s.CompositeRgba.size()) {
            std::memcpy(s.CompositeRgba.data(), cachedBytes.data(), s.CompositeRgba.size());
            s.PrevCachedRgba = std::move(cachedBytes);
            s.PrevCachedKey = cacheKey;
            s.PrevCachedKeyValid = true;
            fromCache = true;
        }
    }

    Solstice::Parallax::SceneEvaluationResult eval{};
    if (!fromCache) {
        const std::uint64_t tEvalStart = EP::MonoNowUs();
        Solstice::Parallax::EvaluateScene(scene, tick, eval);
        s.Stats.EvaluateUs += EP::MonoNowUs() - tEvalStart;
    }

    Solstice::Parallax::MGDisplayList rasterMgList;
    Solstice::Parallax::MGPostProcessSettings mgPostFinal{};
    Solstice::MovieMaker::Export::PreviewMappingResult mapping;
    if (!fromCache) {
        rasterMgList = FilterWorldMgEntriesForRaster(scene, eval.MotionGraphics);
        // `RasterizeMGDisplayList` applies root CA/grade internally. Applying the same `Post` again to the MG+3D composite
        // doubles the grade (and crushes schematic pixels to black with low GradeExposure).
        mgPostFinal = rasterMgList.Post;
        rasterMgList.Post.ChromaticAberrationPx = 0.f;
        rasterMgList.Post.GradeExposure = 1.f;
        rasterMgList.Post.GradeSaturation = 1.f;
        rasterMgList.Post.GradeContrast = 1.f;
        rasterMgList.Post.GradeLift = 0.f;

        Solstice::MovieMaker::Export::BuildPreviewEntitiesAndLights(scene, eval, mapping);
        if (!s.CoverageReported) {
            s.CoverageReported = true;
            if (mapping.Entities.size() > 256) {
                LibUI::Tools::DiagLogLine("[VideoExport] coverage warning: " + std::to_string(mapping.Entities.size())
                    + " preview entities exceed schematic preview cap (256); excess entities will be dropped at capture.");
            }
            LibUI::Tools::DiagLogLine("[VideoExport] coverage: entities=" + std::to_string(mapping.Entities.size())
                + " lights=" + std::to_string(mapping.Lights.size())
                + " mgEntries=" + std::to_string(rasterMgList.Entries.size())
                + " fluids=" + std::to_string(eval.FluidVolumes.size())
                + " softbodies=" + std::to_string(eval.SoftBodies.size())
                + " vehicles=" + std::to_string(eval.Vehicles.size()));
        }
    }
    std::vector<Solstice::EditorEnginePreview::PreviewEntity>& entities = mapping.Entities;
    std::vector<Solstice::Physics::LightSource>& lights = mapping.Lights;

    if (!fromCache) {
        int capW = 0;
        int capH = 0;
        const float aspect = static_cast<float>(s.Params.width) / static_cast<float>(std::max(1u, s.Params.height));
        LibUI::Viewport::OrbitPanZoomState viewNav = s.Nav;
        Solstice::Math::Vec3 target{0.f, 0.f, 0.f};
        float fovYDeg = 55.f;
        const Solstice::MovieMaker::Export::PreviewCameraPose camPose
            = Solstice::MovieMaker::Export::ResolvePreviewCameraPose(scene, eval);
        if (camPose.Found) {
            viewNav.projection = LibUI::Viewport::OrbitProjectionMode::Perspective;
            viewNav.yaw = camPose.NavYaw;
            viewNav.pitch = camPose.NavPitch;
            viewNav.distance = camPose.NavDistance;
            viewNav.pan_x = camPose.NavPanX;
            viewNav.pan_y = camPose.NavPanY;
            target = camPose.Target;
            fovYDeg = camPose.FovYDeg;
        }

        const std::uint64_t tCapStart = EP::MonoNowUs();
        bool captureOk = !s.CaptureDisabled
            && Solstice::EditorEnginePreview::CaptureOrbitRgb(viewNav, target.x, target.y, target.z, fovYDeg, aspect,
                static_cast<int>(s.Params.width), static_cast<int>(s.Params.height), entities.data(), entities.size(), lights.data(),
                lights.size(), s.CaptureRgba, capW, capH);
        if (captureOk
            && (static_cast<uint32_t>(capW) != s.Params.width || static_cast<uint32_t>(capH) != s.Params.height
                || s.CaptureRgba.size() != s.CompositeRgba.size())) {
            captureOk = false;
        }
        s.Stats.CaptureUs += EP::MonoNowUs() - tCapStart;

        if (!captureOk) {
            s.CaptureFailStreak++;
            if (s.WarmupFramesRemaining > 0) {
                // Warmup grace: tolerate a few early failures by filling the capture with neutral gray
                // and continuing the export, mirroring `PreviewPanels` warmup. This stops a cold-start
                // failure from killing very long exports outright.
                std::memset(s.CaptureRgba.data(), 32, s.CaptureRgba.size());
                for (std::size_t i = 3; i < s.CaptureRgba.size(); i += 4) {
                    s.CaptureRgba[i] = std::byte{255};
                }
                captureOk = true;
                s.WarmupFramesRemaining--;
                LibUI::Tools::DiagLogLine("[VideoExport] capture warmup grace frame " + std::to_string(fi));
            } else if (s.CaptureFailStreak >= s.MaxCaptureFailStreakBeforeDisable) {
                // Disable GPU capture for the rest of the export and fall back to MG-only frames so we
                // can finish the file rather than aborting hours of work.
                s.CaptureDisabled = true;
                std::memset(s.CaptureRgba.data(), 0, s.CaptureRgba.size());
                for (std::size_t i = 3; i < s.CaptureRgba.size(); i += 4) {
                    s.CaptureRgba[i] = std::byte{255};
                }
                captureOk = true;
                LibUI::Tools::DiagLogLine("[VideoExport] capture disabled after fail streak; continuing MG-only export.");
            } else if (s.CaptureDisabled) {
                std::memset(s.CaptureRgba.data(), 0, s.CaptureRgba.size());
                for (std::size_t i = 3; i < s.CaptureRgba.size(); i += 4) {
                    s.CaptureRgba[i] = std::byte{255};
                }
                captureOk = true;
            }
        } else {
            s.CaptureFailStreak = 0;
        }

        if (!captureOk) {
            errOut = "3D capture failed during video export.";
            s.Failed = true;
            return false;
        }

        // Composite is built in `CompositeRgba` (persistent buffer; no per-frame allocation).
        std::memcpy(s.CompositeRgba.data(), s.CaptureRgba.data(), s.CompositeRgba.size());

        const std::uint64_t tMgStart = EP::MonoNowUs();
        Solstice::Parallax::RasterizeMGDisplayList(rasterMgList, &resolver, s.Params.width, s.Params.height,
            std::span<std::byte>(s.MgRgba.data(), s.MgRgba.size()));
        s.Stats.MgRasterUs += EP::MonoNowUs() - tMgStart;

        const std::uint64_t tCompStart = EP::MonoNowUs();
        // SIMD-accelerated source-over blend over the whole composite buffer in place.
        const std::size_t pixels = static_cast<std::size_t>(s.Params.width) * static_cast<std::size_t>(s.Params.height);
        BlendMgOverScene(reinterpret_cast<uint8_t*>(s.CompositeRgba.data()),
            reinterpret_cast<const uint8_t*>(s.MgRgba.data()), pixels, 1.0f);
        if (s.ParticleExportActive) {
            const float dt = 1.f / static_cast<float>(std::max(1u, s.Fps));
            RasterizeParticleCpuOverlayIntoRgba(s.ParticleExportSim, scene, tick, dt, s.Params.width, s.Params.height, s.Nav,
                s.ParticleEmitterManual, s.ParticleEmitterManualWorld,
                reinterpret_cast<uint8_t*>(s.CompositeRgba.data()), s.CompositeRgba.size());
        }
        s.Stats.CompositeUs += EP::MonoNowUs() - tCompStart;

        const std::uint64_t tPostStart = EP::MonoNowUs();
        Solstice::Parallax::ApplyMGPostProcessRgba(
            mgPostFinal, s.Params.width, s.Params.height, std::span<std::byte>(s.CompositeRgba.data(), s.CompositeRgba.size()));
        s.Stats.PostUs += EP::MonoNowUs() - tPostStart;

        // Insert into the cache after composite/post. Skip insertion when capture is degraded
        // (warmup or disabled) so corrupted/placeholder frames never poison subsequent exports.
        if (s.Cache && !s.CaptureDisabled && s.WarmupFramesRemaining == 0) {
            const std::uint64_t baseKey = s.PrevCachedKeyValid ? s.PrevCachedKey : 0ull;
            (void)s.Cache->Insert(cacheKey, baseKey, s.Params.width, s.Params.height,
                std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(s.CompositeRgba.data()),
                    s.CompositeRgba.size()));
            s.PrevCachedKey = cacheKey;
            s.PrevCachedKeyValid = true;
            // Hold a copy so the next frame's delta can be computed locally if useful.
            s.PrevCachedRgba.assign(reinterpret_cast<const std::uint8_t*>(s.CompositeRgba.data()),
                reinterpret_cast<const std::uint8_t*>(s.CompositeRgba.data()) + s.CompositeRgba.size());
        } else {
            s.PrevCachedKeyValid = false;
        }
    }

    // Hand off to the async encoder writer thread. We pre-build the FrameBuffer here using a
    // shared pointer so the queue can take ownership without copying buffer bytes out of the
    // session if/when the writer drains it after we move on to the next frame.
    auto fb = std::make_shared<EncoderWriter::FrameBuffer>();
    fb->FrameIndex = fi;
    fb->Bytes.resize(s.CompositeRgba.size());
    std::memcpy(fb->Bytes.data(), s.CompositeRgba.data(), s.CompositeRgba.size());

    const std::uint64_t tWriteStart = EP::MonoNowUs();
    if (!s.Encoder->Submit(std::move(fb))) {
        errOut = "Failed to submit frame to ffmpeg writer (encoder backpressure or failure).\n" + s.FfmpegCmdSummary;
        std::string encErr;
        if (s.Encoder->HasError(encErr)) {
            errOut += "\n" + encErr;
        }
        const std::string ffErr = ReadSmallTextFile(s.FfmpegStderrPath);
        if (!ffErr.empty()) {
            errOut += "\n\nffmpeg stderr:\n" + ffErr;
        }
        s.Failed = true;
        return false;
    }
    s.Stats.EncodeWriteUs += EP::MonoNowUs() - tWriteStart;
    s.Stats.EncodeBytes = s.Encoder->BytesWritten.load(std::memory_order_acquire);
    s.Stats.MaxQueueDepth = s.Encoder->MaxQueueDepth.load(std::memory_order_acquire);

    s.FrameIndex++;
    s.Stats.Frames = s.FrameIndex;
    s.Stats.FrameTotalUs += EP::MonoNowUs() - tFrameStart;

    outProgress = static_cast<float>(s.FrameIndex) / static_cast<float>(std::max(1u, s.TotalFrames));
    if (s.FrameIndex < s.TotalFrames) {
        outDone = false;
        return true;
    }

    // Final frame done: stop encoder, then run audio mux.
    const std::uint64_t tWaitStart = EP::MonoNowUs();
    const int ec = s.Encoder->Stop();
    s.Stats.EncoderWaitUs += EP::MonoNowUs() - tWaitStart;
    s.Encoder.reset();

    // Emit final stage stats to diag log as a one-line summary.
    {
        const std::uint64_t total = std::max<std::uint64_t>(1, s.Stats.FrameTotalUs);
        std::string line = "[VideoExport] done frames=" + std::to_string(s.Stats.Frames)
            + " total_ms=" + std::to_string(total / 1000ull)
            + " eval_ms=" + std::to_string(s.Stats.EvaluateUs / 1000ull)
            + " cap_ms=" + std::to_string(s.Stats.CaptureUs / 1000ull)
            + " mg_ms=" + std::to_string(s.Stats.MgRasterUs / 1000ull)
            + " comp_ms=" + std::to_string(s.Stats.CompositeUs / 1000ull)
            + " post_ms=" + std::to_string(s.Stats.PostUs / 1000ull)
            + " write_ms=" + std::to_string(s.Stats.EncodeWriteUs / 1000ull)
            + " wait_ms=" + std::to_string(s.Stats.EncoderWaitUs / 1000ull)
            + " enc_bytes=" + std::to_string(s.Stats.EncodeBytes)
            + " max_q=" + std::to_string(s.Stats.MaxQueueDepth);
        if (s.Cache) {
            const auto cs = s.Cache->Stats();
            line += " cache_lookups=" + std::to_string(cs.Lookups)
                + " cache_hits=" + std::to_string(cs.Hits)
                + "(" + std::to_string(cs.HitsDelta) + " delta)"
                + " cache_inserts=" + std::to_string(cs.Inserts)
                + "(" + std::to_string(cs.InsertsDelta) + " delta)"
                + " cache_evicted=" + std::to_string(cs.Evicted)
                + " cache_bytes_stored=" + std::to_string(cs.BytesStored)
                + " cache_bytes_saved_delta=" + std::to_string(cs.BytesSavedByDelta);
        }
        LibUI::Tools::DiagLogLine(line);
        if (s.Cache) {
            s.Cache->Flush();
        }
    }

    if (ec != 0) {
        errOut = "ffmpeg exit code " + std::to_string(ec) + " (non-zero; verify the output file).\n" + s.FfmpegCmdSummary;
        const std::string ffErr = ReadSmallTextFile(s.FfmpegStderrPath);
        if (!ffErr.empty()) {
            errOut += "\n\nffmpeg stderr:\n" + ffErr;
        }
        s.Failed = true;
        return false;
    }
    s.AudioMuxTried = true;
    std::string muxWarn;
    (void)TryMuxEvaluatedSceneAudio(s, scene, resolver, muxWarn);
    if (!muxWarn.empty()) {
        errOut = muxWarn;
    }
    s.Finished = true;
    outDone = true;
    outProgress = 1.f;
    return true;
}

void CancelParallaxSceneVideoExport(IncrementalVideoExportSession*& session) {
    if (!session) {
        return;
    }
    if (session->Encoder) {
        (void)session->Encoder->Stop();
        session->Encoder.reset();
    }
    delete session;
    session = nullptr;
}

bool ExportParallaxSceneToVideo(Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::DevSessionAssetResolver& resolver,
    SDL_Window* window, const VideoExportParams& params, std::string& errOut, const std::function<void(float)>& progress,
    const VideoExportParticleSettings* particleSettings) {
    IncrementalVideoExportSession* s = nullptr;
    if (!BeginParallaxSceneVideoExport(scene, resolver, window, params, s, errOut, particleSettings)) {
        return false;
    }
    bool done = false;
    float pr = 0.f;
    while (!done) {
        std::string stepErr;
        if (!StepParallaxSceneVideoExport(*s, scene, resolver, window, pr, done, stepErr)) {
            CancelParallaxSceneVideoExport(s);
            errOut = stepErr;
            return false;
        }
        if (progress) {
            progress(pr);
        }
        SDL_PumpEvents();
    }
    if (!s) {
        return false;
    }
    CancelParallaxSceneVideoExport(s);
    if (!errOut.empty()) {
        // Non-fatal warning (e.g. audio mux skipped).
        return true;
    }
    return true;
}

} // namespace Solstice::MovieMaker
