#pragma once

#include "SmmCurveGraphBridge.hxx"

#include <LibUI/CurveEditor/CurveModel.hxx>
#include <LibUI/Timeline/TimelineModel.hxx>

namespace Smm::Editing {

/// Interactive curve editor: channel list + canvas (drag keys, add, delete). Mutates Parallax via bridge + undo.
void DrawCurveEditorSession(const char* windowTitle, bool* visible, AppSessionContext& ctx,
    LibUI::CurveEditor::CurveEditorState& curves, LibUI::Timeline::TimelineState& timeline);

} // namespace Smm::Editing
