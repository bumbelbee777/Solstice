#pragma once

#include <Parallax/ParallaxTypes.hxx>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Solstice::Parallax {

class IAssetResolver;

/** Decode image bytes (PNG/JPEG/…) to RGBA8 via stb_image. */
bool DecodeImageBytesToRgba(std::span<const std::byte> bytes, std::vector<std::byte>& outRgba, int& outW, int& outH);

/**
 * CPU raster of MGDisplayList: matches MotionGraphicsCompositor layout (origin offset, text via stb_easy_font,
 * sprites with optional IAssetResolver, placeholder rects when texture missing).
 * Root entries are drawn in ascending **Depth** attribute (float); higher Depth composites later (in front).
 * Output rows are top-down in rgbaBuffer (size width * height * 4). Clears to opaque black first.
 */
void RasterizeMGDisplayList(const MGDisplayList& list, IAssetResolver* assets, uint32_t width, uint32_t height,
    std::span<std::byte> rgbaBuffer);

/**
 * Full-frame post from `MotionGraphicsRootElement` (after composite): chromatic aberration + color grade.
 * Does not include screen shake (that is a draw origin offset, applied in raster/compositor). Safe to no-op
 * when values are at defaults. Used by `RasterizeMGDisplayList` and optionally after GPU MG readback (e.g. ffmpeg path).
 */
void ApplyMGPostProcessRgba(
    const MGPostProcessSettings& post, uint32_t width, uint32_t height, std::span<std::byte> rgbaBuffer);

} // namespace Solstice::Parallax
