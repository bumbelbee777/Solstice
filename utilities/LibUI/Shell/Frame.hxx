#pragma once

#include "LibUI/Core/Core.hxx"

struct SDL_Window;

namespace LibUI::Shell {

/// After ``SDL_GL_MakeCurrent(window, glContext)``: refresh last drawable pixel size (when SDL reports valid
/// dimensions), apply framebuffer fallback, and advance ImGui (`NewFrame`).
LIBUI_API void BeginUtilityImGuiFrame(SDL_Window* window, int& lastDrawableW, int& lastDrawableH);

} // namespace LibUI::Shell
