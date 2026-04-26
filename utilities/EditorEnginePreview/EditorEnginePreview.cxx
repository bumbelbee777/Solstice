#include "EditorEnginePreview.hxx"

#include <Arzachel/MeshFactory.hxx>
#include <Math/Quaternion.hxx>
#include <Core/System/Async.hxx>
#include <Core/Profiling/ScopeTimer.hxx>
#include <Material/Material.hxx>
#include <Material/SmatBinary.hxx>
#include <Render/Assets/TextureRegistry.hxx>
#include <Render/Scene/Camera.hxx>
#include <Render/Scene/Scene.hxx>
#include <Render/SoftwareRenderer.hxx>
#include <UI/Media/ImageLoader.hxx>

#include "LibUI/Tools/DiagLog.hxx"
#include "LibUI/Viewport/Viewport.hxx"

#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <exception>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace Solstice::EditorEnginePreview {
namespace {

using namespace Solstice::Render;
namespace MF = Solstice::Arzachel::MeshFactory;

SDL_Window* g_Window = nullptr;
std::unique_ptr<SoftwareRenderer> g_Renderer;
Scene g_Scene;
MeshLibrary g_MeshLib;
Solstice::Core::MaterialLibrary g_MatLib;
uint32_t g_PlaneMesh = 0;
uint32_t g_CubeMesh = 0;
uint32_t g_MatPlane = 0;
static constexpr size_t kMaxEntityMats = 256;
uint32_t g_EntityMatIds[kMaxEntityMats]{};

bool g_Inited = false;

/// Reserved `TextureRegistry` indices for editor preview (per-entity albedo / normal / roughness).
static constexpr uint32_t kPreviewTexBase = 56000u;

static uint16_t PreviewTextureSlot(unsigned entityIndex, int kind) {
    return static_cast<uint16_t>(kPreviewTexBase + entityIndex * 4u + static_cast<unsigned>(kind));
}

static void ClearEditorPreviewTextureSlots(SoftwareRenderer& renderer, size_t maxEntities) {
    Solstice::Render::TextureRegistry& reg = renderer.GetTextureRegistry();
    for (size_t i = 0; i < maxEntities; ++i) {
        for (int k = 0; k < 3; ++k) {
            const uint16_t slot = PreviewTextureSlot(static_cast<unsigned>(i), k);
            if (reg.Has(slot)) {
                bgfx::TextureHandle h = reg.Get(slot);
                if (bgfx::isValid(h)) {
                    bgfx::destroy(h);
                }
                reg.Remove(slot);
            }
        }
    }
}

static bool ReadWholeFileBinary(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) {
        return false;
    }
    const auto end = f.tellg();
    if (end <= 0) {
        return false;
    }
    out.resize(static_cast<size_t>(end));
    f.seekg(0, std::ios::beg);
    if (!f.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size()))) {
        out.clear();
        return false;
    }
    return true;
}

static bgfx::TextureHandle LoadUncachedRaster(const std::string& path) {
    std::vector<uint8_t> bytes;
    if (!ReadWholeFileBinary(path, bytes) || bytes.empty()) {
        return BGFX_INVALID_HANDLE;
    }
    return Solstice::UI::ImageLoader::GetInstance().LoadImageFromMemory(bytes.data(), bytes.size());
}

static void SanitizeMaterialTextureRefs(Solstice::Core::Material& m, const Solstice::Render::TextureRegistry& reg) {
    if (m.AlbedoTexIndex != 0xFFFF && !reg.Has(m.AlbedoTexIndex)) {
        m.AlbedoTexIndex = 0xFFFF;
    }
    if (m.AlbedoTexIndex2 != 0xFFFF && !reg.Has(m.AlbedoTexIndex2)) {
        m.AlbedoTexIndex2 = 0xFFFF;
    }
    if (m.AlbedoTexIndex3 != 0xFFFF && !reg.Has(m.AlbedoTexIndex3)) {
        m.AlbedoTexIndex3 = 0xFFFF;
    }
    if (m.RoughnessTexIndex != 0xFFFF && !reg.Has(m.RoughnessTexIndex)) {
        m.RoughnessTexIndex = 0xFFFF;
    }
    if (m.NormalMapIndex != 0xFFFF && !reg.Has(m.NormalMapIndex)) {
        m.NormalMapIndex = 0xFFFF;
        m.Flags &= ~static_cast<uint16_t>(Solstice::Core::MaterialFlag_HasNormalMap);
    }
}

static void BindPreviewTextureIfPath(SoftwareRenderer& renderer, Solstice::Core::Material& m, const char* pathUtf8,
    unsigned entityIndex, int kind, void (*applyIndex)(Solstice::Core::Material&, uint16_t)) {
    if (!pathUtf8 || pathUtf8[0] == '\0') {
        return;
    }
    const std::string path(pathUtf8);
    bgfx::TextureHandle h = LoadUncachedRaster(path);
    if (!bgfx::isValid(h)) {
        return;
    }
    Solstice::Render::TextureRegistry& reg = renderer.GetTextureRegistry();
    const uint16_t slot = PreviewTextureSlot(entityIndex, kind);
    if (!reg.Register(slot, h)) {
        bgfx::destroy(h);
        return;
    }
    applyIndex(m, slot);
}

static void ApplyPreviewTextureMaps(SoftwareRenderer& renderer, Solstice::Core::Material& m, const PreviewEntity& e,
    unsigned entityIndex) {
    BindPreviewTextureIfPath(
        renderer, m, e.PreviewAlbedoTexturePath, entityIndex, 0, [](Solstice::Core::Material& mat, uint16_t slot) {
            mat.AlbedoTexIndex = slot;
        });
    BindPreviewTextureIfPath(
        renderer, m, e.PreviewNormalTexturePath, entityIndex, 1, [](Solstice::Core::Material& mat, uint16_t slot) {
            mat.NormalMapIndex = slot;
            mat.Flags |= Solstice::Core::MaterialFlag_HasNormalMap;
        });
    BindPreviewTextureIfPath(
        renderer, m, e.PreviewRoughnessTexturePath, entityIndex, 2, [](Solstice::Core::Material& mat, uint16_t slot) {
            mat.RoughnessTexIndex = slot;
        });
}

static void ClearSceneObjects() {
    while (g_Scene.GetObjectCount() > 0) {
        g_Scene.RemoveObject(static_cast<SceneObjectID>(g_Scene.GetObjectCount() - 1));
    }
}

static uint32_t MaterialForAlbedo(const Math::Vec3& albedo) {
    uint32_t id = g_MatLib.AddMaterial(Solstice::Core::Materials::CreateDefault());
    if (auto* m = g_MatLib.GetMaterial(id)) {
        m->SetAlbedoColor(albedo, 0.35f);
    }
    return id;
}

static void BuildCameraFromOrbit(const LibUI::Viewport::OrbitPanZoomState& st, float targetX, float targetY,
    float targetZ, float aspect, float fovYDeg, Camera& out) {
    using LibUI::Viewport::OrbitProjectionMode;
    out.WorldUp = Math::Vec3(0.f, 1.f, 0.f);
    if (st.projection == OrbitProjectionMode::Perspective) {
        const float yaw = st.yaw;
        const float pitch = st.pitch;
        const float dist = std::max(st.distance, 1e-4f);
        const float fx = std::cos(pitch) * std::sin(yaw);
        const float fy = std::sin(pitch);
        const float fz = std::cos(pitch) * std::cos(yaw);
        const float tx = targetX + st.pan_x;
        const float ty = targetY + st.pan_y;
        const float tz = targetZ;
        Math::Vec3 eye(targetX - fx * dist, targetY - fy * dist, targetZ - fz * dist);
        Math::Vec3 target(tx, ty, tz);
        Math::Vec3 forward = (target - eye).Normalized();
        out.Position = eye;
        out.Front = forward;
        out.Right = out.WorldUp.Cross(out.Front).Normalized();
        out.Up = out.Front.Cross(out.Right).Normalized();
        out.UseOrthographic = false;
        out.Zoom = fovYDeg;
    } else {
        const float cx = targetX + st.pan_x;
        const float cy = targetY + st.pan_y;
        const float cz = targetZ;
        const float he = std::max(st.distance, 1e-4f);
        float eyeX = cx;
        float eyeY = cy;
        float eyeZ = cz;
        Math::Vec3 worldUp(0.f, 1.f, 0.f);
        if (st.projection == OrbitProjectionMode::OrthoTop) {
            eyeY = cy + 1000.f;
            worldUp = Math::Vec3(0.f, 0.f, 1.f);
        } else if (st.projection == OrbitProjectionMode::OrthoFront) {
            eyeZ = cz + 1000.f;
        } else if (st.projection == OrbitProjectionMode::OrthoSide) {
            eyeX = cx + 1000.f;
        }
        Math::Vec3 eye(eyeX, eyeY, eyeZ);
        Math::Vec3 target(cx, cy, cz);
        Math::Vec3 forward = (target - eye).Normalized();
        out.Position = eye;
        out.Front = forward;
        out.Right = worldUp.Cross(out.Front).Normalized();
        out.Up = out.Front.Cross(out.Right).Normalized();
        out.WorldUp = worldUp;
        out.UseOrthographic = true;
        out.OrthoHalfExtentY = he;
        out.Zoom = fovYDeg;
    }
    (void)aspect;
}

} // namespace

bool EnsureInitialized() {
    if (g_Inited) {
        return true;
    }
    Core::JobSystem::Instance().Initialize();

    // Hidden offscreen target for bgfx (D3D11/Vulkan on Windows). Do not use SDL_WINDOW_OPENGL here — no GL context
    // is created, and flagging OpenGL on a second window can interfere with the main app's GL context.
    g_Window = SDL_CreateWindow("SolsticeEditorPreview", 256, 256, SDL_WINDOW_HIDDEN);
    if (!g_Window) {
        return false;
    }

    // Offscreen capture does not need CPU voxel raytracing; that path uses OpenMP + bgfx uploads and has been fragile on some drivers.
    // Sixth arg: disable backbuffer MSAA on bgfx::reset — Intel UHD + D3D11 has faulted on MSAA swapchains for hidden preview windows.
    g_Renderer = std::make_unique<SoftwareRenderer>(256, 256, 16, g_Window, false, true);
    // Tile raster can use OpenMP when async is enabled; keep preview single-threaded for stability on some GPU/driver combos.
    g_Renderer->SetAsyncRendering(false);
    g_Renderer->SetWireframe(false);
    g_Renderer->SetShowDebugOverlay(false);

    g_Scene.SetMeshLibrary(&g_MeshLib);
    g_Scene.SetMaterialLibrary(&g_MatLib);

    g_PlaneMesh = g_MeshLib.AddMesh(MF::CreatePlane(96.f, 96.f));
    g_CubeMesh = g_MeshLib.AddMesh(MF::CreateCube(1.f));

    g_MatPlane = MaterialForAlbedo(Math::Vec3(0.22f, 0.24f, 0.28f));
    for (size_t j = 0; j < kMaxEntityMats; ++j) {
        g_EntityMatIds[j] = MaterialForAlbedo(Math::Vec3(0.72f, 0.74f, 0.78f));
    }

    g_Inited = true;
    return true;
}

void Shutdown() {
    if (!g_Inited) {
        return;
    }
    if (g_Renderer) {
        ClearEditorPreviewTextureSlots(*g_Renderer, kMaxEntityMats);
    }
    g_Renderer.reset();
    if (g_Window) {
        SDL_DestroyWindow(g_Window);
        g_Window = nullptr;
    }
    g_Inited = false;
}

bool CaptureOrbitRgb(const LibUI::Viewport::OrbitPanZoomState& orbit, float targetX, float targetY, float targetZ,
    float fovYDeg, float viewportAspect, int framebufferWidth, int framebufferHeight, const PreviewEntity* entities,
    size_t entityCount, const Physics::LightSource* lights, size_t lightCount, std::vector<std::byte>& outRgba,
    int& outW, int& outH) {
    outRgba.clear();
    outW = 0;
    outH = 0;
    try {
    PROFILE_SCOPE("EditorEnginePreview.CaptureOrbitRgb");
    if (!EnsureInitialized() || !g_Renderer) {
        return false;
    }
    // Preserve the viewport aspect ratio while fitting into [32,1024] (the old 4:1 clamp broke aspect vs. the UI panel).
    const int rw = std::max(1, framebufferWidth);
    const int rh = std::max(1, framebufferHeight);
    const double ar = static_cast<double>(rw) / static_cast<double>(rh);
    const double s = std::min(1024.0 / static_cast<double>(rw), 1024.0 / static_cast<double>(rh));
    int fbW = static_cast<int>(std::lround(static_cast<double>(rw) * s));
    int fbH = static_cast<int>(std::lround(static_cast<double>(rh) * s));
    fbW = std::clamp(fbW, 32, 1024);
    fbH = std::clamp(fbH, 32, 1024);
    {
        const double ar2 = static_cast<double>(fbW) / static_cast<double>(fbH);
        if (ar2 > ar) {
            fbW = std::clamp(static_cast<int>(std::lround(static_cast<double>(fbH) * ar)), 32, 1024);
        } else if (ar2 < ar) {
            fbH = std::clamp(static_cast<int>(std::lround(static_cast<double>(fbW) / ar)), 32, 1024);
        }
    }
    g_Renderer->Resize(fbW, fbH);

    ClearEditorPreviewTextureSlots(*g_Renderer, kMaxEntityMats);

    ClearSceneObjects();

    const SceneObjectID planeId = g_Scene.AddObject(g_PlaneMesh, Math::Vec3(0.f, 0.f, 0.f));
    g_Scene.SetMaterial(planeId, g_MatPlane);

    const size_t nEnt = std::min(entityCount, kMaxEntityMats);
    Solstice::Render::TextureRegistry& texReg = g_Renderer->GetTextureRegistry();
    for (size_t i = 0; i < nEnt; ++i) {
        const PreviewEntity& e = entities[i];
        if (auto* m = g_MatLib.GetMaterial(g_EntityMatIds[i])) {
            if (e.PreviewSmatPath[0] != '\0') {
                Solstice::Core::Material loaded{};
                Solstice::Core::SmatError smatErr = Solstice::Core::SmatError::None;
                const std::string smatPath(e.PreviewSmatPath);
                if (Solstice::Core::ReadSmat(smatPath, loaded, &smatErr)) {
                    *m = loaded;
                } else {
                    *m = Solstice::Core::Materials::CreateDefault();
                    m->SetAlbedoColor(e.Albedo, 0.35f);
                }
            } else {
                *m = Solstice::Core::Materials::CreateDefault();
                m->SetAlbedoColor(e.Albedo, 0.35f);
            }
            SanitizeMaterialTextureRefs(*m, texReg);
            ApplyPreviewTextureMaps(*g_Renderer, *m, e, static_cast<unsigned>(i));
        }
    }
    for (size_t i = 0; i < nEnt; ++i) {
        const PreviewEntity& e = entities[i];
        const float base = std::max(e.HalfExtent, 0.05f) * 2.f;
        const Math::Vec3 scl{
            base * std::max(e.Scale.x, 0.01f),
            base * std::max(e.Scale.y, 0.01f),
            base * std::max(e.Scale.z, 0.01f),
        };
        constexpr float kDeg = 3.14159265f / 180.f;
        const Math::Quaternion rot = Math::Quaternion::FromEuler(e.PitchDeg * kDeg, e.YawDeg * kDeg, e.RollDeg * kDeg);
        const SceneObjectID oid = g_Scene.AddObject(g_CubeMesh, e.Position, rot, scl);
        g_Scene.SetMaterial(oid, g_EntityMatIds[i]);
    }

    g_Scene.UpdateTransforms();

    Camera cam;
    BuildCameraFromOrbit(orbit, targetX, targetY, targetZ, viewportAspect, fovYDeg, cam);

    std::vector<Physics::LightSource> lightVec;
    if (lightCount > 0 && lights) {
        lightVec.assign(lights, lights + lightCount);
    } else {
        Physics::LightSource sun;
        sun.Type = Physics::LightSource::LightType::Directional;
        sun.Position = Math::Vec3(0.45f, 0.85f, 0.38f).Normalized();
        sun.Color = Math::Vec3(1.f, 0.97f, 0.9f);
        sun.Intensity = 1.25f;
        lightVec.push_back(sun);
    }

    g_Renderer->RenderScene(g_Scene, cam, lightVec);
    g_Renderer->QueueFramebufferCapture();
    g_Renderer->Present();

    // `bgfx::frame()` kicks the render thread and returns; the swapchain screenshot callback runs
    // asynchronously (see bgfx.h). Do not poll with extra `bgfx::frame()` by default — a second present
    // faulted on Intel UHD D3D11. Wait with yield + sleep until TryGet succeeds.
    auto tryGrab = [&]() -> bool {
        if (g_Renderer->TryGetLastFramebufferCaptureRGBA8(outRgba, outW, outH)) {
            return !outRgba.empty() && outW > 0 && outH > 0;
        }
        return false;
    };

    constexpr int kYieldIters = 8000;
    for (int i = 0; i < kYieldIters; ++i) {
        if (tryGrab()) {
            return true;
        }
        std::this_thread::yield();
    }

    constexpr int kSleepIters = 8000;
    constexpr auto kSleep = std::chrono::milliseconds(1);
    for (int spin = 0; spin < kSleepIters; ++spin) {
        if (tryGrab()) {
            return true;
        }
        std::this_thread::sleep_for(kSleep);
    }

    // Optional: one extra API frame (may crash on some Intel iGPUs).
    if (LibUI::Tools::EnvVarTruthy("SOLSTICE_PREVIEW_POST_PRESENT_SYNC_FRAME")) {
        bgfx::frame();
        for (int j = 0; j < 8000; ++j) {
            if (tryGrab()) {
                return true;
            }
            std::this_thread::yield();
        }
        for (int j = 0; j < 2000; ++j) {
            if (tryGrab()) {
                return true;
            }
            std::this_thread::sleep_for(kSleep);
        }
    }

    if (LibUI::Tools::EnvVarTruthy("SOLSTICE_PREVIEW_POLL_EXTRA_FRAMES")) {
        for (int attempt = 0; attempt < 24; ++attempt) {
            if (g_Renderer->TryGetLastFramebufferCaptureRGBA8(outRgba, outW, outH)) {
                return !outRgba.empty() && outW > 0 && outH > 0;
            }
            bgfx::frame();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    return false;
    } catch (const std::bad_alloc&) {
        outRgba.clear();
        outW = 0;
        outH = 0;
        return false;
    } catch (const std::exception&) {
        outRgba.clear();
        outW = 0;
        outH = 0;
        return false;
    }
}

} // namespace Solstice::EditorEnginePreview
