#pragma once

#include "LibUI/Core/Core.hxx"
#include "LibUI/Tools/OpenGlDebugBase.hxx"

namespace LibUI::Tools {

/// Log GL version/renderer/vendor once to stderr.
LIBUI_API void GlLogDriverInfo();

/// If supported, enable `GL_DEBUG_OUTPUT` and install a debug callback (KHR / 4.3+ entry points).
/// Call with a current context after `ImGui_ImplOpenGL3_Init` succeeds.
LIBUI_API void GlTryInstallDebugMessenger();

/// If `SOLSTICE_GL_LOG_ERRORS` is set, flush errors after significant GL work (e.g. ImGui render).
LIBUI_API void GlMaybeFlushErrorsAfterFrame(const char* where);

} // namespace LibUI::Tools
