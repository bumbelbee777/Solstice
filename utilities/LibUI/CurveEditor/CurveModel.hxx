#pragma once

#include "LibUI/Core/Core.hxx"

#include <cstdint>
#include <string>
#include <vector>

namespace LibUI::CurveEditor {

struct CurveKey {
    float time{0.0f};
    float value{0.0f};
    /// Ease-in to this key (parametric segment ending here).
    uint8_t easing{0};
    /// Ease on segment leaving this key; `0xFF` = runtime inherits next key’s ease-in.
    uint8_t easeOut{0xFF};
    /// `KeyframeInterpolation` (Parallax) — hold / linear / bezier / eased.
    uint8_t interp{0};
    /// Bezier handle weights in value space (0..1 typical); used for float+Bezier in Parallax.
    float tangentIn{0.333f};
    float tangentOut{0.333f};
};

struct CurveChannel {
    std::string name;
    bool visible{true};
    std::vector<CurveKey> keys;
};

struct CurveEditorState {
    std::vector<CurveChannel> channels;
    int selectedChannel{-1};
    int selectedKeyIndex{-1};
    /// **Sorted-unique** key indices (same `selectedChannel` row). When non-empty, batch ops apply to these keys; `selectedKeyIndex` is kept in sync for copy/delete when one key is focused.
    std::vector<int> selectedKeyIndices{};
    float zoomX{1.0f};
    float zoomY{1.0f};
    /// If true, `valueFitMin` / `valueFitMax` set the value-axis span for the plot (SMM “fit”).
    bool valueFitOverride{false};
    float valueFitMin{0.f};
    float valueFitMax{1.f};
    /// Set by SMM’s Parallax↔curve bridge: largest key count on any synced track (UI perf hint).
    size_t maxKeyframeCountHint{0};
};

} // namespace LibUI::CurveEditor

