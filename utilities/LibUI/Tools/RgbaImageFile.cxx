#include "LibUI/Tools/RgbaImageFile.hxx"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <cmath>
#include <cstring>
#include <filesystem>

namespace LibUI::Tools {

bool LoadImageFileToRgba8(const std::string& pathUtf8, std::vector<std::byte>& outRgba, int& outW, int& outH) {
    outRgba.clear();
    outW = 0;
    outH = 0;
    if (pathUtf8.empty()) {
        return false;
    }
    const std::filesystem::path p(pathUtf8);
    if (!std::filesystem::is_regular_file(p)) {
        return false;
    }
    const std::string narrow = p.string();
    int w = 0;
    int h = 0;
    int comp = 0;
    unsigned char* pix = stbi_load(narrow.c_str(), &w, &h, &comp, 4);
    if (!pix || w <= 0 || h <= 0) {
        if (pix) {
            stbi_image_free(pix);
        }
        return false;
    }
    const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h) * 4;
    outRgba.resize(n);
    std::memcpy(outRgba.data(), pix, n);
    stbi_image_free(pix);
    outW = w;
    outH = h;
    return true;
}

bool SaveRgba8ToPngFile(const std::string& pathUtf8, const std::byte* rgbaTopDown, int w, int h) {
    if (pathUtf8.empty() || !rgbaTopDown || w <= 0 || h <= 0) {
        return false;
    }
    const std::filesystem::path parent = std::filesystem::path(pathUtf8).parent_path();
    if (!parent.empty() && !std::filesystem::exists(parent)) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            return false;
        }
    }
    const std::string narrow = std::filesystem::path(pathUtf8).string();
    const auto* pix = reinterpret_cast<const unsigned char*>(rgbaTopDown);
    const int stride = w * 4;
    return stbi_write_png(narrow.c_str(), w, h, 4, pix, stride) != 0;
}

void AverageRgbFromRgba8(const std::byte* rgba, int w, int h, float outRgb[3]) {
    outRgb[0] = 0.65f;
    outRgb[1] = 0.65f;
    outRgb[2] = 0.65f;
    if (!rgba || w <= 0 || h <= 0) {
        return;
    }
    double sr = 0.;
    double sg = 0.;
    double sb = 0.;
    const size_t count = static_cast<size_t>(w) * static_cast<size_t>(h);
    const auto* p = reinterpret_cast<const unsigned char*>(rgba);
    for (size_t i = 0; i < count; ++i) {
        sr += static_cast<double>(p[i * 4 + 0]);
        sg += static_cast<double>(p[i * 4 + 1]);
        sb += static_cast<double>(p[i * 4 + 2]);
    }
    const double inv = 1.0 / static_cast<double>(count);
    outRgb[0] = static_cast<float>(sr * inv / 255.0);
    outRgb[1] = static_cast<float>(sg * inv / 255.0);
    outRgb[2] = static_cast<float>(sb * inv / 255.0);
}

} // namespace LibUI::Tools
