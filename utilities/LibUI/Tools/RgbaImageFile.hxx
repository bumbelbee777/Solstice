#pragma once

#include "LibUI/Core/Core.hxx"

#include <cstddef>
#include <string>
#include <vector>

namespace LibUI::Tools {

/// Decode a raster file (PNG/JPEG/… via stb_image) to **RGBA8**, top-down rows.
LIBUI_API bool LoadImageFileToRgba8(const std::string& pathUtf8, std::vector<std::byte>& outRgba, int& outW, int& outH);

/// Writes **RGBA8** top-down rows to a PNG file (parent directory must exist).
LIBUI_API bool SaveRgba8ToPngFile(const std::string& pathUtf8, const std::byte* rgbaTopDown, int w, int h);

/// Average RGB (sRGB-ish, no explicit gamma) for cheap preview / tinting.
LIBUI_API void AverageRgbFromRgba8(const std::byte* rgba, int w, int h, float outRgb[3]);

} // namespace LibUI::Tools
