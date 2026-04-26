#pragma once

#include "LibUI/Core/Core.hxx"

#include <SDL3/SDL.h>

namespace LibUI::Shell {

/// OpenGL window + context created with the shared utility profile (Jackhammer defaults: GL 3.3 core, 24/8 depth/stencil).
struct GlWindow {
    SDL_Window* window = nullptr;
    SDL_GLContext glContext = nullptr;
};

/// ``SDL_INIT_VIDEO`` once per process before ``CreateUtilityGlWindow``.
LIBUI_API bool InitUtilitySdlVideo();

/// Call once at normal exit after ``DestroyUtilityGlWindow`` (matches ``SDL_Quit``).
LIBUI_API void ShutdownUtilitySdlVideo();

/// Apply GL attribute block; invoked by ``CreateUtilityGlWindow`` (also available if a tool creates the window itself).
LIBUI_API void ApplyUtilityDefaultGlAttributes();

/// Create window (``SDL_WINDOW_OPENGL`` is OR'd in), GL context, ``MakeCurrent``, and ``SDL_GL_SetSwapInterval``.
/// On failure, partially created resources are torn down and fields are cleared.
LIBUI_API bool CreateUtilityGlWindow(GlWindow& out, const char* title, int width, int height, SDL_WindowFlags flags,
    int swapInterval = 1);

LIBUI_API void DestroyUtilityGlWindow(GlWindow& w);

LIBUI_API bool UtilityGlWindowMakeCurrent(const GlWindow& w);

LIBUI_API void UtilityGlWindowSetSwapInterval(int interval);

/// ``true`` when this window and context match the current GL thread state.
LIBUI_API bool VerifyUtilityGlWindowCurrent(const GlWindow& w);

} // namespace LibUI::Shell
