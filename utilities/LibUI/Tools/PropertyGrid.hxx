#pragma once

#include "LibUI/Core/Core.hxx"

namespace LibUI::Tools {

// Two-column property grid helpers for editor forms.
LIBUI_API bool BeginPropertyGrid(const char* id, float labelColumnWidth = 220.0f);
LIBUI_API void EndPropertyGrid();
LIBUI_API void PropertyLabel(const char* label, const char* help = nullptr);
LIBUI_API bool PropertyBool(const char* label, bool* value, const char* help = nullptr);
LIBUI_API bool PropertyInt(const char* label, int* value, int min = 0, int max = 0, const char* help = nullptr);
LIBUI_API bool PropertyFloat(const char* label, float* value, float speed = 0.01f, float min = 0.0f, float max = 0.0f,
    const char* format = "%.3f", const char* help = nullptr);
LIBUI_API bool PropertyFloat3(const char* label, float value[3], float speed = 0.01f, const char* help = nullptr);

} // namespace LibUI::Tools
