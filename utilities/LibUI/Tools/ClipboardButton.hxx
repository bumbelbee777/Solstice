#pragma once

#include "LibUI/Core/Core.hxx"

#include <imgui.h>

namespace LibUI::Tools {

/// Small **Copy** control; copies ``textUtf8`` (NUL-terminated) to the system clipboard on success.
LIBUI_API bool CopyTextButton(const char* id, const char* textUtf8, const char* buttonLabel = "Copy");

} // namespace LibUI::Tools
