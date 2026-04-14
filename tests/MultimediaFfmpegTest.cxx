/**
 * Decodes a short lavfi-generated H.264 clip (path from argv[1]).
 * Build only when SolsticeEngine links Multimedia (FFmpeg).
 */

#include "TestHarness.hxx"
#include <Multimedia/FfmpegVideoDecoder.hxx>

#include <cstdio>
#include <string>

namespace {

bool RunDecode(const char* path) {
    SOLSTICE_TEST_ASSERT(path && path[0] != '\0', "missing video path argv[1]");

    Solstice::Multimedia::FfmpegVideoDecoder dec;
    std::string err;
    SOLSTICE_TEST_ASSERT(dec.Open(path, err), ("Open failed: " + err).c_str());

    int frames = 0;
    uint32_t w0 = 0;
    uint32_t h0 = 0;
    constexpr int kMaxFrames = 256;
    while (frames < kMaxFrames && dec.DecodeNextFrame(err)) {
        if (frames == 0) {
            w0 = dec.Width();
            h0 = dec.Height();
            SOLSTICE_TEST_ASSERT(w0 > 0 && h0 > 0, "first frame dimensions");
            SOLSTICE_TEST_ASSERT(dec.RgbaData() != nullptr, "RGBA buffer");
            SOLSTICE_TEST_ASSERT(dec.RgbaStrideBytes() == w0 * 4u, "RGBA stride");
        } else {
            SOLSTICE_TEST_ASSERT(dec.Width() == w0 && dec.Height() == h0, "dimension change mid-stream");
            SOLSTICE_TEST_ASSERT(dec.RgbaData() != nullptr, "RGBA after advance");
        }
        ++frames;
    }
    SOLSTICE_TEST_ASSERT(frames >= 2, "expected at least 2 frames from test clip");
    SOLSTICE_TEST_PASS("decoded multiple frames");
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: MultimediaFfmpegTest <path-to-test.mp4>\n");
        return 2;
    }
    if (!RunDecode(argv[1])) {
        return SolsticeTestMainResult("MultimediaFfmpegTest");
    }
    return SolsticeTestMainResult("MultimediaFfmpegTest");
}
