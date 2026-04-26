#pragma once

#include "SmmCurveGraphBridge.hxx"

#include <LibUI/Timeline/TimelineModel.hxx>

#include <cstdint>
#include <vector>

namespace Smm::Editing {

/// Logical driver link: driven track receives scaled/offset copy of driver float at playhead (bake / preview).
struct GraphDriverLink {
    uint32_t driverTrackIndex{0};
    uint32_t drivenTrackIndex{0};
    float scale{1.f};
    float offset{0.f};
};

struct GraphEditorState {
    std::vector<GraphDriverLink> links;
    int selectedLink{-1};
};

/// Minimal node-link editor for animation track bindings. Mutates scene via bridge (bake at playhead).
void DrawGraphEditorSession(const char* windowTitle, bool* visible, AppSessionContext& ctx, GraphEditorState& graph,
    LibUI::Timeline::TimelineState& timeline, const std::vector<EditorTrackBinding>& bindings);

} // namespace Smm::Editing
