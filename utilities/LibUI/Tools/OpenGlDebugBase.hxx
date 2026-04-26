#pragma once

#include <SDL3/SDL_opengl.h>

#include "LibUI/Tools/DiagLog.hxx"

#include <cstdio>
#include <cstdlib>

namespace LibUI::Tools {

/// Drain all pending `glGetError()` values; logs each line (see `DiagLogLine` — works from DLL + GUI builds).
inline void GlFlushErrors(const char* where) {
    for (;;) {
        const GLenum e = glGetError();
        if (e == GL_NO_ERROR) {
            break;
        }
        char buf[256]{};
        std::snprintf(buf, sizeof(buf), "LibUI GL [%s] glGetError -> 0x%X", where ? where : "?", static_cast<unsigned>(e));
        DiagLogLine(buf);
    }
}

/// Non-zero if tools should request an OpenGL debug context (more driver checks; may be slower).
inline bool GlDebugContextRequested() {
#if defined(_DEBUG)
    return true;
#else
#if defined(_WIN32)
    return EnvVarTruthy("SOLSTICE_GL_DEBUG");
#else
    const char* e = std::getenv("SOLSTICE_GL_DEBUG");
    return e && e[0] != '\0' && e[0] != '0';
#endif
#endif
}

} // namespace LibUI::Tools
