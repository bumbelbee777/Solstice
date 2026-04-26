#pragma once

#include "LibUI/Core/Core.hxx"
#include "LibUI/CurveEditor/CurveModel.hxx"

namespace LibUI::CurveEditor {

LIBUI_API void DrawCurveEditor(const char* title, CurveEditorState& state, bool* visible = nullptr);

} // namespace LibUI::CurveEditor

