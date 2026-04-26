#pragma once

#include <LibUI/CurveEditor/CurveModel.hxx>
#include <LibUI/Timeline/TimelineModel.hxx>

#include <Parallax/DevSessionAssetResolver.hxx>
#include <Parallax/ParallaxScene.hxx>

#include <cstdint>
#include <string>
#include <vector>

#include "SmmKeyframePresets.hxx"

struct SDL_Window;

namespace Smm::Editing {

struct ParticleEditorState;

enum class TrackBindingKind : uint8_t {
    SceneChannel = 0,
    MgTrack = 1,
};

/// Stable mapping between a timeline/curve row and Parallax channel or MG track data.
struct EditorTrackBinding {
    TrackBindingKind kind{TrackBindingKind::SceneChannel};
    uint32_t index{0}; ///< `ChannelIndex` or MG track index in `scene.GetMGTracks()`.
    int component{-1}; ///< -1 = whole scalar (float), 0..2 = Vec3 lane.
};

struct KeyframeEditUiState {
    bool snapEnabled{true};
    int snapFps{60};
    bool clipValid{false};
    uint64_t clipTick{0};
    float clipValue{0.f};
    uint8_t clipEasing{0};
    int clipSourceTrack{-1};
};

struct AppSessionContext {
    SDL_Window* mainWindow{nullptr};
    Solstice::Parallax::ParallaxScene* scene{nullptr};
    Solstice::Parallax::DevSessionAssetResolver* resolver{nullptr};
    ParticleEditorState* particleEditor{nullptr};
    std::vector<EditorTrackBinding>* bindings{nullptr};
    bool compressPrlx{false};
    bool* sceneDirty{nullptr};
    uint64_t* timeTicks{nullptr};
    std::string* statusLine{nullptr};
    KeyframeEditUiState* keyframeEdit{nullptr};
    /// INI keyframe presets from `presets/Keyframe` (search roots: exe dir, project folder); optional.
    const std::vector<Smm::Keyframe::KeyframeCurvePreset>* keyframePresets{nullptr};
};

/// Rebuild timeline tracks, curve channels, and bindings from the Parallax scene (float + vec3 lanes only).
void BridgeSyncFromScene(const Solstice::Parallax::ParallaxScene& scene, LibUI::Timeline::TimelineState& timeline,
    LibUI::CurveEditor::CurveEditorState& curves, std::vector<EditorTrackBinding>& bindings);

/// Move a keyframe (by sorted key index on the binding) to new tick/value. Uses Remove+Add for Parallax semantics.
/// Call `Smm::PushSceneUndoSnapshot` before mutating if the edit should be undoable.
bool BridgeMoveKeyframe(Solstice::Parallax::ParallaxScene& scene, const EditorTrackBinding& binding, size_t sortedKeyIndex,
    uint64_t newTick, float newValue, std::string& errOut);

bool BridgeAddKeyframeAtTick(Solstice::Parallax::ParallaxScene& scene, const EditorTrackBinding& binding, uint64_t tick,
    float value, std::string& errOut);

bool BridgeAddKeyframeAtTickEasing(Solstice::Parallax::ParallaxScene& scene, const EditorTrackBinding& binding, uint64_t tick,
    float value, Solstice::Parallax::EasingType ease, std::string& errOut);

bool BridgeDeleteKeyframe(Solstice::Parallax::ParallaxScene& scene, const EditorTrackBinding& binding, size_t sortedKeyIndex,
    std::string& errOut);

/// Read a keyframe (sorted order) for copy/paste. `outEasing` is Parallax `Easing` byte.
bool BridgeReadKeyframe(Solstice::Parallax::ParallaxScene& scene, const EditorTrackBinding& binding, size_t sortedKeyIndex,
    uint64_t& outTick, float& outValue, uint8_t& outEasing, std::string& errOut);

/// Change easing only (segment ending at this key). Does not move time or value.
bool BridgeSetKeyframeEasing(Solstice::Parallax::ParallaxScene& scene, const EditorTrackBinding& binding, size_t sortedKeyIndex,
    Solstice::Parallax::EasingType ease, std::string& errOut);

} // namespace Smm::Editing
