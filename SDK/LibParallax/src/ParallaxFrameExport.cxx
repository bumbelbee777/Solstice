#include <Parallax/MGRaster.hxx>
#include <Parallax/ParallaxScene.hxx>
#include <Parallax/ParallaxTypes.hxx>
#include <Render/SoftwareRenderer.hxx>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <span>
#include <vector>

namespace Solstice::Parallax {

namespace {

bool WritePpmRGB(const std::filesystem::path& path, const uint8_t* rgba, uint32_t w, uint32_t h) {
    std::ofstream o(path, std::ios::binary);
    if (!o) {
        return false;
    }
    o << "P6\n" << w << " " << h << "\n255\n";
    for (uint32_t y = 0; y < h; ++y) {
        const uint32_t row = h - 1 - y;
        for (uint32_t x = 0; x < w; ++x) {
            const size_t i = (static_cast<size_t>(row) * w + x) * 4;
            o.put(rgba[i]);
            o.put(rgba[i + 1]);
            o.put(rgba[i + 2]);
        }
    }
    return static_cast<bool>(o);
}

bool WriteRgbaRaw(const std::filesystem::path& path, const uint8_t* buf, size_t n) {
    std::ofstream o(path, std::ios::binary);
    if (!o) {
        return false;
    }
    o.write(reinterpret_cast<const char*>(buf), static_cast<std::streamsize>(n));
    return static_cast<bool>(o);
}

} // namespace

void ExecuteRenderJob(const ParallaxScene& scene, const RenderJobParams& params, Solstice::Render::SoftwareRenderer& renderer,
                      RenderProgressCallback progress) {
    (void)renderer;

    const uint32_t w = std::max(1u, params.Width);
    const uint32_t h = std::max(1u, params.Height);
    const uint32_t fps = std::max(1u, params.FrameRate);
    uint64_t tps = scene.GetTicksPerSecond();
    if (tps == 0) {
        tps = 6000;
    }

    if (params.EndTick <= params.StartTick) {
        if (progress) {
            progress(0, 1, 1.f);
        }
        return;
    }

    const double durationSec = static_cast<double>(params.EndTick - params.StartTick) / static_cast<double>(tps);
    uint32_t totalFrames = static_cast<uint32_t>(std::ceil(durationSec * static_cast<double>(fps)));
    if (totalFrames == 0) {
        totalFrames = 1;
    }

    std::vector<uint8_t> rgba(static_cast<size_t>(w) * static_cast<size_t>(h) * 4);

    std::error_code ec;
    std::filesystem::create_directories(params.OutputPath, ec);

    for (uint32_t fi = 0; fi < totalFrames; ++fi) {
        const double t = static_cast<double>(fi) / static_cast<double>(fps);
        uint64_t tick = params.StartTick + static_cast<uint64_t>(std::llround(t * static_cast<double>(tps)));
        if (tick >= params.EndTick) {
            tick = params.EndTick - 1;
        }

        SceneEvaluationResult eval;
        EvaluateScene(scene, tick, eval);
        if (params.CompositeMG) {
            RasterizeMGDisplayList(eval.MotionGraphics, params.AssetResolver, w, h,
                std::span(reinterpret_cast<std::byte*>(rgba.data()), rgba.size()));
        } else {
            for (size_t i = 0; i < rgba.size(); i += 4) {
                rgba[i] = 0;
                rgba[i + 1] = 0;
                rgba[i + 2] = 0;
                rgba[i + 3] = 255;
            }
        }

        char name[96] = {};
        if (params.Format == OutputFormat::PNGSequence || params.Format == OutputFormat::EXRSequence) {
            std::snprintf(name, sizeof(name), "frame_%05u.ppm", fi);
            WritePpmRGB(params.OutputPath / name, rgba.data(), w, h);
        } else if (params.Format == OutputFormat::RawFrames) {
            std::snprintf(name, sizeof(name), "frame_%05u.rgba", fi);
            WriteRgbaRaw(params.OutputPath / name, rgba.data(), rgba.size());
        }

        if (progress) {
            progress(fi + 1, totalFrames, static_cast<float>(fi + 1) / static_cast<float>(totalFrames));
        }
    }
}

} // namespace Solstice::Parallax
