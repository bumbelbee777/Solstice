// Validates the foundational export-pipeline primitives used by SMM video export:
//  - LockFreeSpscQueue (single producer / consumer correctness, ordering, full/empty edges)
//  - FrameArena (alignment, bounded capacity, reset semantics)
//  - BlendOverRGBA (numeric equivalence to a reference scalar source-over)
//  - TileEqual (SIMD/scalar equality with edge-byte tail handling)
// These tests run as part of the `quick` ctest label so regressions in the export pipeline are
// caught early without needing to launch the full MovieMaker UI.

#include "../utilities/MovieMaker/Export/ExportPipeline.hxx"

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

namespace EP = Solstice::MovieMaker::ExportPipeline;

namespace {

int g_Failures = 0;
const char* g_CurrentTest = nullptr;

#define SMM_REQUIRE(cond)                                                                                              \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::fprintf(stderr, "[FAIL] %s: %s (line %d)\n", g_CurrentTest, #cond, __LINE__);                          \
            ++g_Failures;                                                                                              \
        }                                                                                                              \
    } while (0)

void TestSpscQueueBasic() {
    g_CurrentTest = "SpscQueueBasic";
    EP::LockFreeSpscQueue<int, 8> q;
    SMM_REQUIRE(q.Empty());
    SMM_REQUIRE(q.SizeApprox() == 0);
    SMM_REQUIRE(q.TryPush(1));
    SMM_REQUIRE(q.TryPush(2));
    SMM_REQUIRE(q.SizeApprox() == 2);
    int v = 0;
    SMM_REQUIRE(q.TryPop(v) && v == 1);
    SMM_REQUIRE(q.TryPop(v) && v == 2);
    SMM_REQUIRE(!q.TryPop(v));
}

void TestSpscQueueFullDrops() {
    g_CurrentTest = "SpscQueueFullDrops";
    EP::LockFreeSpscQueue<int, 4> q;
    for (int i = 0; i < 4; ++i) {
        SMM_REQUIRE(q.TryPush(i));
    }
    SMM_REQUIRE(!q.TryPush(99));
    int v = 0;
    SMM_REQUIRE(q.TryPop(v) && v == 0);
    SMM_REQUIRE(q.TryPush(99));
    SMM_REQUIRE(q.TryPop(v) && v == 1);
}

void TestSpscQueueProducerConsumer() {
    g_CurrentTest = "SpscQueueProducerConsumer";
    EP::LockFreeSpscQueue<int, 64> q;
    constexpr int kCount = 4096;
    std::vector<int> received;
    received.reserve(kCount);
    std::atomic<bool> done{false};
    std::thread consumer([&]() {
        int v = 0;
        while (received.size() < static_cast<size_t>(kCount)) {
            if (q.TryPop(v)) {
                received.push_back(v);
            } else {
                std::this_thread::yield();
            }
        }
        done.store(true);
    });
    for (int i = 0; i < kCount; ++i) {
        while (!q.TryPush(i)) {
            std::this_thread::yield();
        }
    }
    consumer.join();
    SMM_REQUIRE(received.size() == static_cast<size_t>(kCount));
    for (int i = 0; i < kCount; ++i) {
        if (received[i] != i) {
            SMM_REQUIRE(false);
            break;
        }
    }
}

void TestFrameArenaBasic() {
    g_CurrentTest = "FrameArenaBasic";
    EP::FrameArena a(1024);
    SMM_REQUIRE(a.Capacity() == 1024);
    SMM_REQUIRE(a.Used() == 0);
    auto* p1 = a.AllocateArray<int>(16);
    SMM_REQUIRE(p1 != nullptr);
    SMM_REQUIRE(a.Used() >= sizeof(int) * 16);
    auto* p2 = a.AllocateArray<double>(16);
    SMM_REQUIRE(p2 != nullptr);
    SMM_REQUIRE((reinterpret_cast<uintptr_t>(p2) % alignof(double)) == 0);
    SMM_REQUIRE(p2 != reinterpret_cast<void*>(p1));
    a.Reset();
    SMM_REQUIRE(a.Used() == 0);
    auto* p3 = a.AllocateArray<int>(8);
    SMM_REQUIRE(p3 != nullptr);
}

void TestFrameArenaOverflow() {
    g_CurrentTest = "FrameArenaOverflow";
    EP::FrameArena a(64);
    auto* p = a.AllocateArray<char>(48);
    SMM_REQUIRE(p != nullptr);
    auto* fail = a.AllocateArray<char>(48);
    SMM_REQUIRE(fail == nullptr);
    auto* small = a.AllocateArray<char>(8);
    SMM_REQUIRE(small != nullptr);
}

// Reference scalar implementation matches the SIMD/scalar paths in `BlendOverRGBA`.
static void ReferenceBlendRGBA(uint8_t* dst, const uint8_t* src, size_t pixelCount, float globalAlpha) {
    if (globalAlpha < 0.f) globalAlpha = 0.f;
    if (globalAlpha > 1.f) globalAlpha = 1.f;
    for (size_t i = 0; i < pixelCount; ++i) {
        const float saSrc = static_cast<float>(src[i * 4 + 3]) * (1.f / 255.f);
        const float sa = saSrc * globalAlpha;
        if (sa < 1e-4f) {
            continue;
        }
        const float inv = 1.f - sa;
        for (int c = 0; c < 3; ++c) {
            const float v = sa * static_cast<float>(src[i * 4 + c]) + inv * static_cast<float>(dst[i * 4 + c]);
            dst[i * 4 + c] = static_cast<uint8_t>(v < 0.f ? 0.f : (v > 255.f ? 255.f : v));
        }
        const float aOut = sa * 255.f + inv * static_cast<float>(dst[i * 4 + 3]);
        dst[i * 4 + 3] = static_cast<uint8_t>(aOut < 0.f ? 0.f : (aOut > 255.f ? 255.f : aOut));
    }
}

void TestBlendOverRGBAEquivalence() {
    g_CurrentTest = "BlendOverRGBAEquivalence";
    constexpr size_t kPixels = 1024;
    std::vector<uint8_t> dstA(kPixels * 4);
    std::vector<uint8_t> dstB(kPixels * 4);
    std::vector<uint8_t> src(kPixels * 4);
    for (size_t i = 0; i < kPixels; ++i) {
        dstA[i * 4 + 0] = static_cast<uint8_t>((i * 7) & 0xFF);
        dstA[i * 4 + 1] = static_cast<uint8_t>((i * 11) & 0xFF);
        dstA[i * 4 + 2] = static_cast<uint8_t>((i * 13) & 0xFF);
        dstA[i * 4 + 3] = static_cast<uint8_t>(64 + (i % 192));
        dstB[i * 4 + 0] = dstA[i * 4 + 0];
        dstB[i * 4 + 1] = dstA[i * 4 + 1];
        dstB[i * 4 + 2] = dstA[i * 4 + 2];
        dstB[i * 4 + 3] = dstA[i * 4 + 3];
        src[i * 4 + 0] = static_cast<uint8_t>((i * 17) & 0xFF);
        src[i * 4 + 1] = static_cast<uint8_t>((i * 19) & 0xFF);
        src[i * 4 + 2] = static_cast<uint8_t>((i * 23) & 0xFF);
        src[i * 4 + 3] = static_cast<uint8_t>((i * 29) & 0xFF);
    }
    EP::BlendOverRGBA(dstA.data(), src.data(), kPixels, 1.0f);
    ReferenceBlendRGBA(dstB.data(), src.data(), kPixels, 1.0f);
    bool match = std::memcmp(dstA.data(), dstB.data(), dstA.size()) == 0;
    SMM_REQUIRE(match);
}

void TestTileEqual() {
    g_CurrentTest = "TileEqual";
    std::vector<uint8_t> a(256, 0xAB);
    std::vector<uint8_t> b(256, 0xAB);
    SMM_REQUIRE(EP::TileEqual(a.data(), b.data(), a.size()));
    b[123] = 0xCD;
    SMM_REQUIRE(!EP::TileEqual(a.data(), b.data(), a.size()));
    // Tail-byte path (size not multiple of vector width).
    std::vector<uint8_t> c(33, 0x42);
    std::vector<uint8_t> d(33, 0x42);
    SMM_REQUIRE(EP::TileEqual(c.data(), d.data(), c.size()));
    d[32] = 0x43;
    SMM_REQUIRE(!EP::TileEqual(c.data(), d.data(), c.size()));
}

// Throughput guardrail for the export pipeline composite blend. A 4K frame is 33MP * 4B =
// 132MB; modern CPUs sustain >5GB/s on streamed memcpy-class work, so a single composite blend
// pass on 4K should finish in well under 100 ms. We assert a generous 500 ms ceiling on the
// equivalent of a single 1080p frame so CI machines under load still pass. This is a
// regression guardrail, not a performance characterization.
void TestBlendOverRGBAThroughputGuardrail() {
    g_CurrentTest = "BlendOverRGBAThroughputGuardrail";
    constexpr size_t kPixels = 1920u * 1080u;
    std::vector<uint8_t> dst(kPixels * 4, 0x10);
    std::vector<uint8_t> src(kPixels * 4, 0x80);
    const auto t0 = std::chrono::steady_clock::now();
    EP::BlendOverRGBA(dst.data(), src.data(), kPixels, 1.0f);
    const auto t1 = std::chrono::steady_clock::now();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    SMM_REQUIRE(ms < 500);
    std::printf("[INFO] blend 1080p elapsed=%lld ms (simd=%s)\n", static_cast<long long>(ms), EP::SimdFeatureString());
}

void TestTileEqualThroughputGuardrail() {
    g_CurrentTest = "TileEqualThroughputGuardrail";
    constexpr size_t kBytes = 1920u * 1080u * 4u;
    std::vector<uint8_t> a(kBytes, 0x11);
    std::vector<uint8_t> b(kBytes, 0x11);
    const auto t0 = std::chrono::steady_clock::now();
    const bool eq = EP::TileEqual(a.data(), b.data(), a.size());
    const auto t1 = std::chrono::steady_clock::now();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    SMM_REQUIRE(eq);
    SMM_REQUIRE(ms < 200);
    std::printf("[INFO] tile-eq 1080p elapsed=%lld ms (simd=%s)\n", static_cast<long long>(ms), EP::SimdFeatureString());
}

} // namespace

int main() {
    TestSpscQueueBasic();
    TestSpscQueueFullDrops();
    TestSpscQueueProducerConsumer();
    TestFrameArenaBasic();
    TestFrameArenaOverflow();
    TestBlendOverRGBAEquivalence();
    TestTileEqual();
    TestBlendOverRGBAThroughputGuardrail();
    TestTileEqualThroughputGuardrail();
    if (g_Failures == 0) {
        std::printf("[OK] SmmExportPipelineTest: all checks passed (simd=%s).\n", EP::SimdFeatureString());
        return 0;
    }
    std::fprintf(stderr, "[FAIL] SmmExportPipelineTest: %d failure(s).\n", g_Failures);
    return 1;
}
