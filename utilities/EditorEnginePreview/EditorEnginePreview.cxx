#include "EditorEnginePreview.hxx"

#include <Arzachel/MeshFactory.hxx>
#include <Core/System/Async.hxx>
#include <Material/Material.hxx>
#include <Render/Scene/Camera.hxx>
#include <Render/Scene/Scene.hxx>
#include <Render/SoftwareRenderer.hxx>

#include "LibUI/Viewport/Viewport.hxx"

#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <memory>
#include <thread>

namespace Solstice::EditorEnginePreview {
namespace {

using namespace Solstice::Render;
namespace MF = Solstice::Arzachel::MeshFactory;

SDL_Window* g_Window = nullptr;
std::unique_ptr<SoftwareRenderer> g_Renderer;
Scene g_Scene;
MeshLibrary g_MeshLib;
MaterialLibrary g_MatLib;
uint32_t g_PlaneMesh = 0;
uint32_t g_CubeMesh = 0;
uint32_t g_MatPlane = 0;
static constexpr size_t kMaxEntityMats = 256;
uint32_t g_EntityMatIds[kMaxEntityMats]{};

bool g_Inited = false;

static void ClearSceneObjects() {
    while (g_Scene.GetObjectCount() > 0) {
        g_Scene.RemoveObject(static_cast<SceneObjectID>(g_Scene.GetObjectCount() - 1));
    }
}

static uint32_t MaterialForAlbedo(const Math::Vec3& albedo) {
    uint32_t id = g_MatLib.AddMaterial(Materials::CreateDefault());
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

    g_Window = SDL_CreateWindow("SolsticeEditorPreview", 256, 256,
        SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
    if (!g_Window) {
        return false;
    }

    g_Renderer = std::make_unique<SoftwareRenderer>(256, 256, 16, g_Window);
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
    if (!EnsureInitialized() || !g_Renderer) {
        return false;
    }

    const int fbW = std::clamp(framebufferWidth, 32, 1024);
    const int fbH = std::clamp(framebufferHeight, 32, 1024);
    g_Renderer->Resize(fbW, fbH);

    ClearSceneObjects();

    const SceneObjectID planeId = g_Scene.AddObject(g_PlaneMesh, Math::Vec3(0.f, 0.f, 0.f));
    g_Scene.SetMaterial(planeId, g_MatPlane);

    const size_t nEnt = std::min(entityCount, kMaxEntityMats);
    for (size_t i = 0; i < nEnt; ++i) {
        const PreviewEntity& e = entities[i];
        if (auto* m = g_MatLib.GetMaterial(g_EntityMatIds[i])) {
            m->SetAlbedoColor(e.Albedo, 0.35f);
        }
    }
    for (size_t i = 0; i < nEnt; ++i) {
        const PreviewEntity& e = entities[i];
        const float s = std::max(e.HalfExtent, 0.05f) * 2.f;
        const SceneObjectID oid =
            g_Scene.AddObject(g_CubeMesh, e.Position, Math::Quaternion(), Math::Vec3(s, s, s));
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

    for (int attempt = 0; attempt < 24; ++attempt) {
        if (g_Renderer->TryGetLastFramebufferCaptureRGBA8(outRgba, outW, outH)) {
            return !outRgba.empty() && outW > 0 && outH > 0;
        }
        bgfx::frame();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return false;
}

} // namespace Solstice::EditorEnginePreview
