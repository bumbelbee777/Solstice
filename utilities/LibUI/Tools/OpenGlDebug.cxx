#include "LibUI/Tools/OpenGlDebug.hxx"
#include "LibUI/Tools/DiagLog.hxx"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl_glext.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace LibUI::Tools {
namespace {

void APIENTRY GlDebugCallback(GLenum /*source*/, GLenum type, GLuint /*id*/, GLenum severity, GLsizei length,
    const GLchar* message, const void* /*userParam*/) {
    const char* sev = "unknown";
    if (severity == GL_DEBUG_SEVERITY_HIGH) {
        sev = "high";
    } else if (severity == GL_DEBUG_SEVERITY_MEDIUM) {
        sev = "medium";
    } else if (severity == GL_DEBUG_SEVERITY_LOW) {
        sev = "low";
    } else if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) {
        sev = "note";
    }
    const char* typ = "other";
    if (type == GL_DEBUG_TYPE_ERROR) {
        typ = "error";
    } else if (type == GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR) {
        typ = "deprecated";
    } else if (type == GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR) {
        typ = "undefined";
    } else if (type == GL_DEBUG_TYPE_PERFORMANCE) {
        typ = "perf";
    }
    const int msgLen = length > 0 ? static_cast<int>(length) : (message ? static_cast<int>(std::strlen(message)) : 0);
    char buf[4096]{};
    if (message && msgLen > 0) {
        const int n = static_cast<int>(std::min<GLsizei>(msgLen, static_cast<GLsizei>(3200)));
        std::snprintf(buf, sizeof(buf), "[GL debug %s / %s] %.*s", sev, typ, n, message);
    } else {
        std::snprintf(buf, sizeof(buf), "[GL debug %s / %s] (no message)", sev, typ);
    }
    DiagLogLine(buf);
}

} // namespace

void GlLogDriverInfo() {
    const GLubyte* ver = glGetString(GL_VERSION);
    const GLubyte* ren = glGetString(GL_RENDERER);
    const GLubyte* ven = glGetString(GL_VENDOR);
    char buf[1024]{};
    std::snprintf(buf, sizeof(buf), "[GL] version=%s renderer=%s vendor=%s",
        ver ? reinterpret_cast<const char*>(ver) : "?",
        ren ? reinterpret_cast<const char*>(ren) : "?",
        ven ? reinterpret_cast<const char*>(ven) : "?");
    DiagLogLine(buf);
}

void GlTryInstallDebugMessenger() {
    auto* proc = reinterpret_cast<PFNGLDEBUGMESSAGECALLBACKPROC>(SDL_GL_GetProcAddress("glDebugMessageCallback"));
    if (!proc) {
        proc = reinterpret_cast<PFNGLDEBUGMESSAGECALLBACKPROC>(SDL_GL_GetProcAddress("glDebugMessageCallbackARB"));
    }
    if (!proc) {
        DiagLogLine("[GL] glDebugMessageCallback not available (need KHR_debug / GL 4.3+).");
        return;
    }

    proc(GlDebugCallback, nullptr);

    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

    auto* controlFn = reinterpret_cast<PFNGLDEBUGMESSAGECONTROLPROC>(SDL_GL_GetProcAddress("glDebugMessageControl"));
    if (controlFn) {
        // Mute notification spam; keep errors/warnings from the driver.
        controlFn(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
    }

    DiagLogLine("[GL] Debug message callback installed.");
    GlFlushErrors("after GlTryInstallDebugMessenger");
}

void GlMaybeFlushErrorsAfterFrame(const char* where) {
    if (!EnvVarTruthy("SOLSTICE_GL_LOG_ERRORS")) {
        return;
    }
    GlFlushErrors(where);
}

} // namespace LibUI::Tools
