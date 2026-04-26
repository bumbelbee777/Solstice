#include "LibUI/Shell/Frame.hxx"

#include <SDL3/SDL.h>

#include <algorithm>

namespace LibUI::Shell {

void BeginUtilityImGuiFrame(SDL_Window* window, int& lastDrawableW, int& lastDrawableH) {
    if (!window) {
        return;
    }
    int pw = 0;
    int ph = 0;
    SDL_GetWindowSizeInPixels(window, &pw, &ph);
    if (pw > 0 && ph > 0) {
        lastDrawableW = pw;
        lastDrawableH = ph;
    }
    lastDrawableW = std::max(1, lastDrawableW);
    lastDrawableH = std::max(1, lastDrawableH);
    LibUI::Core::SetFramebufferPixelFallback(lastDrawableW, lastDrawableH);
    LibUI::Core::NewFrame();
}

} // namespace LibUI::Shell
