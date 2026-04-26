#include "SmmParticleEditor.hxx"

#include "SmmCurveGraphBridge.hxx"
#include "SmmParticleScene.hxx"
#include "Media/SmmImage.hxx"
#include "SmmFileOps.hxx"

#include <LibUI/FileDialogs/FileDialogs.hxx>
#include <LibUI/Graphics/PreviewTexture.hxx>
#include <LibUI/Undo/SnapshotStack.hxx>

#include <Parallax/MGRaster.hxx>

#include <SDL3/SDL.h>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <random>
#include <span>
#include <string>
#include <vector>

namespace Smm::Editing {

static LibUI::Undo::SnapshotStack<ParticleEditorState> g_particleEditUndo{32};
static ParticleEditorState s_particleCommitted{};
static bool s_particleCommittedInited{false};
static bool g_particleEditPanelFocused{false};

static void TrackParticleWidget(const ParticleEditorState& baseline) {
    if (ImGui::IsItemActivated()) {
        try {
            g_particleEditUndo.PushBeforeChange(baseline);
        } catch (const std::bad_alloc&) {
        }
    }
}

namespace {
thread_local std::mt19937 s_rng{std::random_device{}()};

static float Rng01() {
    std::uniform_real_distribution<float> d(0.f, 1.f);
    return d(s_rng);
}

static void SpawnOne(ParticleEditorState& st, const Solstice::Math::Vec3& origin) {
    ParticleInstance p{};
    p.position = origin;
    p.velocity.x = st.velMin.x + (st.velMax.x - st.velMin.x) * Rng01();
    p.velocity.y = st.velMin.y + (st.velMax.y - st.velMin.y) * Rng01();
    p.velocity.z = st.velMin.z + (st.velMax.z - st.velMin.z) * Rng01();
    p.lifetime = (std::max)(st.lifetimeSec, 0.05f);
    p.age = 0.f;
    p.size = st.startSize;
    st.particles.push_back(p);
}
} // namespace

bool IsParticleEditPanelFocused() noexcept {
    return g_particleEditPanelFocused;
}

void ResetParticleEditUndo() {
    g_particleEditUndo.Clear();
    s_particleCommittedInited = false;
}

bool ApplyParticleEditUndo(ParticleEditorState& st, bool undoNotRedo) {
    return undoNotRedo ? g_particleEditUndo.Undo(st) : g_particleEditUndo.Redo(st);
}

bool CanParticleEditUndo() noexcept {
    return g_particleEditUndo.CanUndo();
}

bool CanParticleEditRedo() noexcept {
    return g_particleEditUndo.CanRedo();
}

void SampleParticleColorOverLife(const ParticleEditorState& st, float t, float outRgba[4]) {
    t = std::clamp(t, 0.f, 1.f);
    if (!st.useColorGradient) {
        for (int c = 0; c < 4; ++c) {
            outRgba[c] = st.colorStart[c] * (1.f - t) + st.colorEnd[c] * t;
        }
        return;
    }
    const int n = std::clamp(st.gradientStops, 2, st.kMaxParticleGradient);
    if (t <= st.gradT[0]) {
        for (int c = 0; c < 4; ++c) {
            outRgba[c] = st.gradRgba[0][c];
        }
        return;
    }
    if (t >= st.gradT[n - 1]) {
        for (int c = 0; c < 4; ++c) {
            outRgba[c] = st.gradRgba[n - 1][c];
        }
        return;
    }
    for (int s = 0; s < n - 1; ++s) {
        const float t0 = st.gradT[s];
        const float t1 = st.gradT[s + 1];
        if (t < t0 || t > t1) {
            continue;
        }
        const float w = (t1 > t0 + 1e-8f) ? (t - t0) / (t1 - t0) : 0.f;
        for (int c = 0; c < 4; ++c) {
            outRgba[c] = st.gradRgba[s][c] * (1.f - w) + st.gradRgba[s + 1][c] * w;
        }
        return;
    }
    for (int c = 0; c < 4; ++c) {
        outRgba[c] = st.colorEnd[c];
    }
}

void DrawParticleEditorPanel(const char* windowTitle, bool* visible, ParticleEditorState& st, SDL_Window* fileDialogWindow,
    AppSessionContext* session) {
    g_particleEditPanelFocused = false;
    if (visible && !*visible) {
        return;
    }
    const char* title = (windowTitle && windowTitle[0]) ? windowTitle : "Particles";
    if (!ImGui::Begin(title, visible)) {
        ImGui::End();
        return;
    }
    if (!s_particleCommittedInited) {
        s_particleCommitted = st;
        s_particleCommittedInited = true;
    }
    g_particleEditPanelFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    ImGui::Checkbox("Enable preview", &st.enabled);
    TrackParticleWidget(s_particleCommitted);
    ImGui::Checkbox("Attach to selected scene element index", &st.attachToSceneElement);
    TrackParticleWidget(s_particleCommitted);
    ImGui::SetNextItemWidth(120);
    ImGui::InputInt("Element index", &st.attachElementIndex);
    TrackParticleWidget(s_particleCommitted);
    ImGui::DragFloat("Spawn / sec", &st.spawnPerSec, 0.5f, 0.f, 500.f);
    TrackParticleWidget(s_particleCommitted);
    ImGui::DragFloat("Lifetime (s)", &st.lifetimeSec, 0.02f, 0.05f, 20.f);
    TrackParticleWidget(s_particleCommitted);
    ImGui::DragFloat3("Vel min", &st.velMin.x, 0.05f);
    TrackParticleWidget(s_particleCommitted);
    ImGui::DragFloat3("Vel max", &st.velMax.x, 0.05f);
    TrackParticleWidget(s_particleCommitted);
    ImGui::DragFloat3("Gravity", &st.gravity.x, 0.05f);
    TrackParticleWidget(s_particleCommitted);
    ImGui::DragFloat("Linear drag", &st.linearDrag, 0.02f, 0.f, 12.f);
    TrackParticleWidget(s_particleCommitted);
    ImGui::DragFloat("Start size", &st.startSize, 0.005f, 0.001f, 2.f);
    TrackParticleWidget(s_particleCommitted);
    ImGui::DragFloat("End size", &st.endSize, 0.005f, 0.f, 2.f);
    TrackParticleWidget(s_particleCommitted);
    ImGui::DragInt("Max particles", &st.maxParticles, 32.f, 64, 65536);
    TrackParticleWidget(s_particleCommitted);
    ImGui::ColorEdit4("Color start (legacy, when gradient off)", st.colorStart);
    TrackParticleWidget(s_particleCommitted);
    ImGui::ColorEdit4("Color end (legacy)", st.colorEnd);
    TrackParticleWidget(s_particleCommitted);
    if (!st.gradStopsInited) {
        for (int c = 0; c < 4; ++c) {
            st.gradRgba[0][c] = st.colorStart[c];
            st.gradRgba[1][c] = st.colorEnd[c];
        }
        st.gradT[0] = 0.f;
        st.gradT[1] = 1.f;
        st.gradientStops = 2;
        st.gradStopsInited = true;
    }
    ImGui::Checkbox("Multi-stop color over life (viewport)", &st.useColorGradient);
    TrackParticleWidget(s_particleCommitted);
    if (st.useColorGradient) {
        ImGui::SliderInt("Stops (sort T with button below)", &st.gradientStops, 2, st.kMaxParticleGradient);
        TrackParticleWidget(s_particleCommitted);
        for (int i = 0; i < st.gradientStops; ++i) {
            ImGui::PushID(i);
            ImGui::SetNextItemWidth(120.f);
            ImGui::DragFloat("t (0-1 along life)", &st.gradT[i], 0.002f, 0.f, 1.f, "%.3f");
            TrackParticleWidget(s_particleCommitted);
            ImGui::SameLine();
            ImGui::ColorEdit4("rgba", st.gradRgba[i]);
            TrackParticleWidget(s_particleCommitted);
            ImGui::PopID();
        }
        if (ImGui::Button("Sort stops by T##gradsort")) {
            try {
                g_particleEditUndo.PushBeforeChange(s_particleCommitted);
            } catch (const std::bad_alloc&) {
            }
            for (int a = 0; a < st.gradientStops; ++a) {
                for (int b = a + 1; b < st.gradientStops; ++b) {
                    if (st.gradT[b] < st.gradT[a]) {
                        std::swap(st.gradT[a], st.gradT[b]);
                        for (int c = 0; c < 4; ++c) {
                            std::swap(st.gradRgba[a][c], st.gradRgba[b][c]);
                        }
                    }
                }
            }
        }
        ImGui::TextDisabled("Export maps first/last stop to `ColorStart` and `ColorEnd` on write.");
    }
    ImGui::Separator();
    ImGui::Checkbox("Use imported sprite in viewport", &st.useImportedSprite);
    TrackParticleWidget(s_particleCommitted);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 88.0f);
    ImGui::InputTextWithHint("##partsprite", "Sprite image path (PNG, JPEG, …)", st.particleSpritePath,
        sizeof(st.particleSpritePath));
    TrackParticleWidget(s_particleCommitted);
    ImGui::SameLine();
    if (!fileDialogWindow) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Browse##psprite")) {
        if (fileDialogWindow) {
            LibUI::FileDialogs::ShowOpenFile(
                fileDialogWindow, "Open particle sprite",
                [&st](std::optional<std::string> path) {
                    if (path) {
                        std::strncpy(st.particleSpritePath, path->c_str(), sizeof(st.particleSpritePath) - 1);
                        st.particleSpritePath[sizeof(st.particleSpritePath) - 1] = '\0';
                        st.useImportedSprite = true;
                    }
                },
                std::span<const LibUI::FileDialogs::FileFilter>(Smm::Image::kRasterImportFilters));
        }
    }
    if (!fileDialogWindow) {
        ImGui::EndDisabled();
    }
    if (ImGui::Button("Burst +64")) {
        try {
            g_particleEditUndo.PushBeforeChange(s_particleCommitted);
        } catch (const std::bad_alloc&) {
        }
        st.burstPending += 64;
    }
    ImGui::SameLine();
    if (ImGui::Button("Burst +256")) {
        try {
            g_particleEditUndo.PushBeforeChange(s_particleCommitted);
        } catch (const std::bad_alloc&) {
        }
        st.burstPending += 256;
    }
    if (ImGui::Button("Clear particles")) {
        try {
            g_particleEditUndo.PushBeforeChange(s_particleCommitted);
        } catch (const std::bad_alloc&) {
        }
        st.particles.clear();
        st.accum = 0.f;
        st.burstPending = 0;
    }
    if (session && session->scene && session->resolver && session->particleEditor == &st) {
        ImGui::Separator();
        ImGui::TextUnformatted("Parallax (.prlx)");
        if (ImGui::Button("Write emitter to scene")) {
            std::string err;
            Smm::PushSceneUndoSnapshot(*session->scene, session->compressPrlx);
            if (!Smm::Particles::SyncEditorToParallaxScene(*session->scene, *session->particleEditor, *session->resolver, err)) {
                if (session->statusLine) {
                    *session->statusLine = err;
                }
            } else {
                if (session->sceneDirty) {
                    *session->sceneDirty = true;
                }
                if (session->statusLine) {
                    *session->statusLine = "Particle emitter written to Parallax scene (element SMM_ParticleEmitter).";
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Read emitter from scene")) {
            try {
                g_particleEditUndo.PushBeforeChange(s_particleCommitted);
            } catch (const std::bad_alloc&) {
            }
            Smm::Particles::LoadParticleEditorFromScene(*session->scene, *session->particleEditor);
            if (session->statusLine) {
                *session->statusLine = "Loaded particle emitter from scene (if present).";
            }
        }
        ImGui::TextDisabled("Export/save .prlx also merges the editor emitter automatically.");
    }
    ImGui::TextDisabled("Preview: billboards in unified viewport (sprite or gradient disk).");
    if (!ImGui::IsAnyItemActive()) {
        s_particleCommitted = st;
    }
    ImGui::End();
}

void SyncParticleSpritePreview(SDL_Window* window, LibUI::Graphics::PreviewTextureRgba& tex, const ParticleEditorState& st) {
    static std::string s_loadedForPath;
    if (!window) {
        tex.Destroy();
        s_loadedForPath.clear();
        return;
    }
    if (!st.useImportedSprite || st.particleSpritePath[0] == '\0') {
        tex.Destroy();
        s_loadedForPath.clear();
        return;
    }
    const std::string path(st.particleSpritePath);
    if (path == s_loadedForPath && tex.Valid()) {
        return;
    }
    s_loadedForPath.clear();
    tex.Destroy();

    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) {
        return;
    }
    const auto end = f.tellg();
    if (end <= 0) {
        return;
    }
    std::vector<std::byte> fileBytes(static_cast<size_t>(end));
    f.seekg(0, std::ios::beg);
    if (!f.read(reinterpret_cast<char*>(fileBytes.data()), static_cast<std::streamsize>(fileBytes.size()))) {
        return;
    }
    std::vector<std::byte> rgba;
    int iw = 0;
    int ih = 0;
    if (!Solstice::Parallax::DecodeImageBytesToRgba(std::span<const std::byte>(fileBytes.data(), fileBytes.size()), rgba, iw,
            ih)
        || iw <= 0 || ih <= 0 || rgba.size() < static_cast<size_t>(iw) * static_cast<size_t>(ih) * 4u) {
        return;
    }
    if (!tex.SetSizeUpload(window, static_cast<uint32_t>(iw), static_cast<uint32_t>(ih), rgba.data(), rgba.size())) {
        tex.Destroy();
        return;
    }
    s_loadedForPath = path;
}

void TickParticlePreview(ParticleEditorState& st, const Solstice::Math::Vec3& origin, float dt) {
    if (!st.enabled || dt <= 0.f) {
        return;
    }
    const int cap = std::max(64, st.maxParticles);
    const int burstBudget = std::clamp(st.burstPending, 0, 512);
    st.burstPending -= burstBudget;
    for (int i = 0; i < burstBudget; ++i) {
        SpawnOne(st, origin);
    }

    st.accum += dt * (std::max)(st.spawnPerSec, 0.f);
    while (st.accum >= 1.f) {
        st.accum -= 1.f;
        SpawnOne(st, origin);
    }
    if (st.particles.size() > static_cast<size_t>(cap)) {
        st.particles.erase(st.particles.begin(),
            st.particles.begin() + static_cast<std::ptrdiff_t>(st.particles.size() - static_cast<size_t>(cap)));
    }
    const float drag = std::max(0.f, st.linearDrag);
    const float dragK = std::exp(-drag * dt);
    for (auto& p : st.particles) {
        p.age += dt;
        p.velocity = p.velocity + st.gravity * dt;
        if (dragK < 0.9999f) {
            p.velocity = p.velocity * dragK;
        }
        p.position = p.position + p.velocity * dt;
        const float t = std::clamp(p.age / (std::max)(p.lifetime, 1e-4f), 0.f, 1.f);
        p.size = st.startSize + (st.endSize - st.startSize) * t;
    }
    st.particles.erase(std::remove_if(st.particles.begin(), st.particles.end(),
                           [](const ParticleInstance& p) { return p.age >= p.lifetime; }),
        st.particles.end());
}

} // namespace Smm::Editing
