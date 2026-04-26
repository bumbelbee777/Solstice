#include "LibUI/Shell/GlWindow.hxx"

#include "LibUI/Tools/OpenGlDebug.hxx"

#include <cstdint>

namespace LibUI::Shell {

bool InitUtilitySdlVideo() {
    return SDL_Init(SDL_INIT_VIDEO);
}

void ShutdownUtilitySdlVideo() {
    SDL_Quit();
}

void ApplyUtilityDefaultGlAttributes() {
    int glCtxFlags = 0;
    if (LibUI::Tools::GlDebugContextRequested()) {
        glCtxFlags |= SDL_GL_CONTEXT_DEBUG_FLAG;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, glCtxFlags);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
}

bool CreateUtilityGlWindow(GlWindow& out, const char* title, int width, int height, SDL_WindowFlags flags, int swapInterval) {
    out = {};
    ApplyUtilityDefaultGlAttributes();
    const SDL_WindowFlags wflags =
        static_cast<SDL_WindowFlags>(static_cast<std::uint32_t>(flags) | static_cast<std::uint32_t>(SDL_WINDOW_OPENGL));
    out.window = SDL_CreateWindow(title ? title : "", width, height, wflags);
    if (!out.window) {
        return false;
    }
    out.glContext = SDL_GL_CreateContext(out.window);
    if (!out.glContext) {
        SDL_DestroyWindow(out.window);
        out.window = nullptr;
        return false;
    }
    if (!SDL_GL_MakeCurrent(out.window, out.glContext)) {
        SDL_GL_DestroyContext(out.glContext);
        out.glContext = nullptr;
        SDL_DestroyWindow(out.window);
        out.window = nullptr;
        return false;
    }
    SDL_GL_SetSwapInterval(swapInterval);
    return true;
}

void DestroyUtilityGlWindow(GlWindow& w) {
    if (w.glContext) {
        SDL_GL_DestroyContext(w.glContext);
        w.glContext = nullptr;
    }
    if (w.window) {
        SDL_DestroyWindow(w.window);
        w.window = nullptr;
    }
}

bool UtilityGlWindowMakeCurrent(const GlWindow& w) {
    if (!w.window || !w.glContext) {
        return false;
    }
    return SDL_GL_MakeCurrent(w.window, w.glContext);
}

void UtilityGlWindowSetSwapInterval(int interval) {
    SDL_GL_SetSwapInterval(interval);
}

bool VerifyUtilityGlWindowCurrent(const GlWindow& w) {
    return SDL_GL_GetCurrentWindow() == w.window && SDL_GL_GetCurrentContext() == w.glContext;
}

} // namespace LibUI::Shell
