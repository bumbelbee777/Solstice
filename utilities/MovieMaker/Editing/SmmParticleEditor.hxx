#pragma once

#include <Math/Vector.hxx>

#include <cstdint>
#include <vector>

struct SDL_Window;

namespace LibUI::Graphics {
struct PreviewTextureRgba;
}

namespace Smm::Editing {

struct AppSessionContext;

struct ParticleInstance {
    Solstice::Math::Vec3 position{};
    Solstice::Math::Vec3 velocity{};
    float age{0.f};
    float lifetime{1.f};
    float size{0.1f};
};

struct ParticleEditorState {
    bool enabled{true};
    bool attachToSceneElement{true};
    int attachElementIndex{0};
    float spawnPerSec{24.f};
    float lifetimeSec{1.5f};
    Solstice::Math::Vec3 velMin{-0.8f, 0.4f, -0.8f};
    Solstice::Math::Vec3 velMax{0.8f, 1.6f, 0.8f};
    float startSize{0.12f};
    float endSize{0.04f};
    /// World units / s²; applied each tick in preview.
    Solstice::Math::Vec3 gravity{0.f, -0.9f, 0.f};
    /// Approximate linear damping (higher = quicker slowdown).
    float linearDrag{0.35f};
    int maxParticles{4096};
    /// Immediate burst particles; drained inside `TickParticlePreview` (capped per frame).
    int burstPending{0};
    float colorStart[4]{1.f, 0.88f, 0.55f, 1.f};
    float colorEnd[4]{1.f, 0.35f, 0.08f, 0.15f};
    static constexpr int kMaxParticleGradient = 6;
    /// When true, preview uses `gradient*` stops; Parallax `ColorStart` / `ColorEnd` stay first/last on write.
    bool useColorGradient{false};
    bool gradStopsInited{false};
    int gradientStops{2};
    float gradT[kMaxParticleGradient]{0.f, 1.f, 1.f, 1.f, 1.f, 1.f};
    float gradRgba[kMaxParticleGradient][4]{};
    /// When set, unified viewport draws this raster per particle (tinted by color start/end) instead of a disk.
    bool useImportedSprite{false};
    char particleSpritePath[768]{};
    std::vector<ParticleInstance> particles;
    float accum{0.f};
};

void DrawParticleEditorPanel(const char* windowTitle, bool* visible, ParticleEditorState& st, SDL_Window* fileDialogWindow,
    AppSessionContext* session = nullptr);

/// True when the Particles window has focus (so Ctrl+Z targets particle undo instead of the Parallax scene).
bool IsParticleEditPanelFocused() noexcept;
void ResetParticleEditUndo();
bool ApplyParticleEditUndo(ParticleEditorState& st, bool undoNotRedo);
bool CanParticleEditUndo() noexcept;
bool CanParticleEditRedo() noexcept;

/// t in [0,1] over particle lifetime. Writes RGBA; uses `useColorGradient` or legacy `colorStart`/`colorEnd`.
void SampleParticleColorOverLife(const ParticleEditorState& st, float t, float outRgba[4]);

/// Load or refresh `tex` from `st.particleSpritePath` when the path or toggle changes (RGBA → GL for ImGui).
void SyncParticleSpritePreview(SDL_Window* window, LibUI::Graphics::PreviewTextureRgba& tex, const ParticleEditorState& st);

/// Advance simple CPU particles; `origin` is emitter world position.
void TickParticlePreview(ParticleEditorState& st, const Solstice::Math::Vec3& origin, float dt);

} // namespace Smm::Editing
