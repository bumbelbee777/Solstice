#include <UI/Core/Window.hxx>
#include <Render/DefaultRenderer.hxx>
#include <Render/Scene/Scene.hxx>
#include <Render/Scene/Camera.hxx>
#include <Render/Assets/Mesh.hxx>
#include <Material/Material.hxx>
#include <Arzachel/MeshFactory.hxx>
#include <Math/Vector.hxx>
#include <Math/Quaternion.hxx>
#include <bgfx/bgfx.h>
#include <SDL3/SDL.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

using namespace Solstice;
using namespace Solstice::Render;
using namespace Solstice::Math;
using namespace Solstice::UI;
using namespace Solstice::Core;
namespace MeshFactory = Solstice::Arzachel::MeshFactory;

struct PhaseConfig {
    const char* Name;
    uint32_t TargetTriangles;
    float DurationSeconds;
};

struct PhaseRuntimeStats {
    double SumFps{0.0};
    double SumFrameMs{0.0};
    double SumRendererMs{0.0};
    double SumDrawCalls{0.0};
    uint64_t Samples{0};
    double PeakFrameMs{0.0};
    std::deque<double> RecentFrameMs;
};

static void BuildStressScene(Scene& scene,
                             MeshLibrary& meshLib,
                             Core::MaterialLibrary& matLib,
                             uint32_t meshId,
                             uint32_t materialId,
                             uint32_t trianglesPerMesh,
                             uint32_t targetTriangles) {
    const uint32_t objectCount = std::max<uint32_t>(1u, (targetTriangles + trianglesPerMesh - 1u) / trianglesPerMesh);
    const uint32_t side = static_cast<uint32_t>(std::ceil(std::cbrt(static_cast<float>(objectCount))));
    const float spacing = 1.35f;
    uint32_t created = 0;

    for (uint32_t z = 0; z < side && created < objectCount; ++z) {
        for (uint32_t y = 0; y < side && created < objectCount; ++y) {
            for (uint32_t x = 0; x < side && created < objectCount; ++x) {
                Vec3 pos(
                    (static_cast<float>(x) - static_cast<float>(side) * 0.5f) * spacing,
                    (static_cast<float>(y) - static_cast<float>(side) * 0.5f) * spacing,
                    (static_cast<float>(z) - static_cast<float>(side) * 0.5f) * spacing);
                SceneObjectID id = scene.AddObject(meshId, pos, Quaternion(), Vec3(1.0f, 1.0f, 1.0f), ObjectType_Static);
                scene.SetMaterial(id, materialId);
                ++created;
            }
        }
    }
    scene.UpdateTransforms();
}

int main() {
    std::array<PhaseConfig, 3> phases{{
        {"Warmup-150K", 150000u, 8.0f},
        {"Crunch-500K", 500000u, 8.0f},
        {"Overdrive-1M", 1000000u, 8.0f},
    }};
    std::array<PhaseRuntimeStats, 3> phaseStats{};

    Window window(1280, 720, "Solstice Render Stress Show");
    SoftwareRenderer renderer(1280, 720, 16, window.NativeWindow(), true, false);
    renderer.SetVSync(false);
    renderer.SetShowDebugOverlay(true);
    renderer.SetHybridMode(false);
    renderer.SetMLEnabled(false);
    std::cout << "Renderer initialized, preparing scenes..." << std::endl;

    MeshLibrary meshLib;
    Core::MaterialLibrary matLib;
    auto stressMesh = MeshFactory::CreateSphere(0.5f, 12);
    stressMesh->IsStatic = true;
    const uint32_t trianglesPerMesh = static_cast<uint32_t>(stressMesh->GetTriangleCount());
    const uint32_t meshId = meshLib.AddMesh(std::move(stressMesh));
    const uint32_t materialId = matLib.AddMaterial(Materials::CreateUnlit(Vec3(0.2f, 0.9f, 1.0f)));

    auto makePhaseScene = [&](uint32_t targetTriangles) {
        auto scene = std::make_unique<Scene>();
        scene->SetMeshLibrary(&meshLib);
        scene->SetMaterialLibrary(&matLib);
        BuildStressScene(*scene, meshLib, matLib, meshId, materialId, trianglesPerMesh, targetTriangles);
        return scene;
    };

    Camera camera(Vec3(0.0f, 0.0f, 32.0f));
    size_t phaseIndex = 0;
    std::unique_ptr<Scene> currentScene = makePhaseScene(phases[phaseIndex].TargetTriangles);
    std::cout << "Loaded phase scene: " << phases[phaseIndex].Name << std::endl;
    auto phaseStart = std::chrono::high_resolution_clock::now();
    auto prevTime = phaseStart;
    auto lastCliPrint = phaseStart;

    std::cout << "=== Solstice Render Stress Show ===\n";
    std::cout << "Triangles per mesh: " << trianglesPerMesh << "\n";
    std::cout << "Targets: 150k / 500k / 1M\n";

    while (phaseIndex < phases.size()) {
        window.PollEvents();

        const auto now = std::chrono::high_resolution_clock::now();
        const double dt = std::chrono::duration<double>(now - prevTime).count();
        prevTime = now;

        const PhaseConfig& phase = phases[phaseIndex];
        Scene& scene = *currentScene;

        renderer.Clear(Vec4(0.05f, 0.08f, 0.12f, 1.0f));
        renderer.RenderScene(scene, camera);

        const auto& rs = renderer.GetStats();
        const double drawCalls = 0.0;
        const double fps = dt > 0.0 ? 1.0 / dt : 0.0;

        PhaseRuntimeStats& stats = phaseStats[phaseIndex];
        stats.SumFps += fps;
        stats.SumFrameMs += dt * 1000.0;
        stats.SumRendererMs += rs.TotalTimeMs;
        stats.SumDrawCalls += drawCalls;
        stats.Samples++;
        stats.PeakFrameMs = std::max(stats.PeakFrameMs, dt * 1000.0);
        stats.RecentFrameMs.push_back(dt * 1000.0);
        if (stats.RecentFrameMs.size() > 600) {
            stats.RecentFrameMs.pop_front();
        }

        const double elapsed = std::chrono::duration<double>(now - phaseStart).count();
        const double avgFps = stats.Samples ? stats.SumFps / static_cast<double>(stats.Samples) : 0.0;
        const double avgFrameMs = stats.Samples ? stats.SumFrameMs / static_cast<double>(stats.Samples) : 0.0;
        const double avgRendererMs = stats.Samples ? stats.SumRendererMs / static_cast<double>(stats.Samples) : 0.0;
        const double avgDrawCalls = stats.Samples ? stats.SumDrawCalls / static_cast<double>(stats.Samples) : 0.0;

        char title[256];
        std::snprintf(title, sizeof(title),
            "RenderStress | %s | FPS %.1f avg %.1f | frame %.2fms avg %.2fms | tri %u",
            phase.Name, fps, avgFps, dt * 1000.0, avgFrameMs, rs.TrianglesSubmitted);
        SDL_SetWindowTitle(window.NativeWindow(), title);

        if (std::chrono::duration<double>(now - lastCliPrint).count() >= 1.0) {
            lastCliPrint = now;
            std::vector<double> sorted(stats.RecentFrameMs.begin(), stats.RecentFrameMs.end());
            std::sort(sorted.begin(), sorted.end());
            const auto pickPercentile = [&](double p) {
                if (sorted.empty()) return 0.0;
                const size_t idx = static_cast<size_t>(std::clamp(p, 0.0, 1.0) * static_cast<double>(sorted.size() - 1));
                return sorted[idx];
            };
            const double p50 = pickPercentile(0.50);
            const double p95 = pickPercentile(0.95);
            const double p99 = pickPercentile(0.99);
            std::printf("[Stress][%s] tri=%u fps=%.1f avg_fps=%.1f frame_ms=%.2f avg_ms=%.2f draw=%.1f\n",
                phase.Name,
                rs.TrianglesSubmitted,
                fps,
                avgFps,
                dt * 1000.0,
                avgFrameMs,
                avgDrawCalls);
            std::printf("  precise: renderer_ms=%.4f total_ms=%.4f p50=%.4f p95=%.4f p99=%.4f samples=%llu\n",
                rs.TotalTimeMs,
                dt * 1000.0,
                p50,
                p95,
                p99,
                static_cast<unsigned long long>(stats.Samples));
        }

        renderer.Present();

        if (elapsed >= phase.DurationSeconds) {
            phaseIndex++;
            if (phaseIndex < phases.size()) {
                std::cout << "[Stress] Switching to phase " << phases[phaseIndex].Name << "...\n";
                currentScene = makePhaseScene(phases[phaseIndex].TargetTriangles);
            }
            phaseStart = now;
        }
    }

    std::cout << "\n=== Stress Summary ===\n";
    std::cout << std::fixed << std::setprecision(4);
    for (size_t i = 0; i < phases.size(); ++i) {
        const auto& p = phases[i];
        const auto& s = phaseStats[i];
        const double avgFps = s.Samples ? s.SumFps / static_cast<double>(s.Samples) : 0.0;
        const double avgFrameMs = s.Samples ? s.SumFrameMs / static_cast<double>(s.Samples) : 0.0;
        const double avgDrawCalls = s.Samples ? s.SumDrawCalls / static_cast<double>(s.Samples) : 0.0;
        std::cout << p.Name
                  << " target=" << p.TargetTriangles
                  << " avg_fps=" << avgFps
                  << " avg_ms=" << avgFrameMs
                  << " peak_ms=" << s.PeakFrameMs
                  << " avg_draw=" << avgDrawCalls << "\n";
    }

    return 0;
}
