#pragma once

#include "LibUI/Core/Core.hxx"
#include "LibUI/Timeline/TimelineModel.hxx"

namespace LibUI::Timeline {

LIBUI_API bool DrawAnimationTimeline(const char* strId, TimelineState& state, const ImVec2& size = ImVec2(0, 160));
LIBUI_API void DrawTimelineWidget(const char* title, TimelineState& state, bool* visible = nullptr);

} // namespace LibUI::Timeline

