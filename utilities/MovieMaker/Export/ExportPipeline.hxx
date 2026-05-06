#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <thread>
#include <type_traits>
#include <vector>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#include <emmintrin.h>
#define SOLSTICE_HAVE_SSE2 1
#endif

#if defined(__AVX2__)
#include <immintrin.h>
#define SOLSTICE_HAVE_AVX2 1
#endif

namespace Solstice::MovieMaker::ExportPipeline {

/// Cache line size used for false-sharing avoidance in lock-free structures.
inline constexpr std::size_t kCacheLineBytes = 64;

/// Hardware prefetch hint into L2/L3 (`locality == 3`).
inline void Prefetch(const void* p) noexcept {
#if defined(__GNUC__) || defined(__clang__)
    __builtin_prefetch(p, 0, 3);
#elif defined(_MSC_VER)
    _mm_prefetch(reinterpret_cast<const char*>(p), _MM_HINT_T0);
#else
    (void)p;
#endif
}

/// Prefetches a range of memory in cache-line-sized strides.
inline void PrefetchRange(const void* p, std::size_t bytes) noexcept {
    const auto* base = reinterpret_cast<const std::uint8_t*>(p);
    for (std::size_t i = 0; i < bytes; i += kCacheLineBytes) {
        Prefetch(base + i);
    }
}

/// Single-producer single-consumer bounded ring queue. Uses cache-line-aligned indices to
/// avoid false sharing between producer and consumer threads. The data array stores values
/// inline; capacity must be a power of two.
template <typename T, std::size_t CapacityPow2>
class LockFreeSpscQueue {
    static_assert(CapacityPow2 > 0 && (CapacityPow2 & (CapacityPow2 - 1)) == 0,
        "Capacity must be a power of two");
    static_assert(std::is_default_constructible_v<T>, "T must be default-constructible");

public:
    LockFreeSpscQueue() = default;
    LockFreeSpscQueue(const LockFreeSpscQueue&) = delete;
    LockFreeSpscQueue& operator=(const LockFreeSpscQueue&) = delete;
    LockFreeSpscQueue(LockFreeSpscQueue&&) = delete;
    LockFreeSpscQueue& operator=(LockFreeSpscQueue&&) = delete;
    ~LockFreeSpscQueue() = default;

    static constexpr std::size_t Capacity() noexcept { return CapacityPow2; }

    [[nodiscard]] bool Empty() const noexcept {
        return ReadIdx_.load(std::memory_order_acquire) == WriteIdx_.load(std::memory_order_acquire);
    }

    [[nodiscard]] std::size_t SizeApprox() const noexcept {
        const std::uint64_t w = WriteIdx_.load(std::memory_order_acquire);
        const std::uint64_t r = ReadIdx_.load(std::memory_order_acquire);
        return static_cast<std::size_t>(w - r);
    }

    /// Producer-side: push by copy/move; returns false if queue is full.
    bool TryPush(T value) noexcept {
        const std::uint64_t w = WriteIdx_.load(std::memory_order_relaxed);
        const std::uint64_t r = ReadIdx_.load(std::memory_order_acquire);
        if (w - r >= CapacityPow2) {
            return false;
        }
        Slots_[w & (CapacityPow2 - 1)] = std::move(value);
        WriteIdx_.store(w + 1, std::memory_order_release);
        return true;
    }

    /// Consumer-side: pop into out; returns false if queue empty.
    bool TryPop(T& out) noexcept {
        const std::uint64_t r = ReadIdx_.load(std::memory_order_relaxed);
        const std::uint64_t w = WriteIdx_.load(std::memory_order_acquire);
        if (r == w) {
            return false;
        }
        out = std::move(Slots_[r & (CapacityPow2 - 1)]);
        ReadIdx_.store(r + 1, std::memory_order_release);
        return true;
    }

    /// Block for up to maxIterations on a busy-wait loop, yielding between attempts.
    /// Returns true on success.
    bool BlockingPop(T& out, std::atomic<bool>& cancelled,
        std::chrono::milliseconds totalBudget = std::chrono::milliseconds(5000)) noexcept {
        const auto deadline = std::chrono::steady_clock::now() + totalBudget;
        while (!cancelled.load(std::memory_order_acquire)) {
            if (TryPop(out)) {
                return true;
            }
            if (std::chrono::steady_clock::now() > deadline) {
                return false;
            }
            std::this_thread::yield();
        }
        return false;
    }

private:
    alignas(kCacheLineBytes) std::atomic<std::uint64_t> WriteIdx_{0};
    alignas(kCacheLineBytes) std::atomic<std::uint64_t> ReadIdx_{0};
    alignas(kCacheLineBytes) T Slots_[CapacityPow2]{};
};

/// Bounded arena allocator: pre-commits `Capacity()` bytes; resets in O(1).
/// Suitable for short-lived per-frame transient allocations within an export run.
class FrameArena {
public:
    FrameArena() = default;
    explicit FrameArena(std::size_t capacityBytes) { Reserve(capacityBytes); }
    FrameArena(const FrameArena&) = delete;
    FrameArena& operator=(const FrameArena&) = delete;
    FrameArena(FrameArena&&) noexcept = default;
    FrameArena& operator=(FrameArena&&) noexcept = default;

    void Reserve(std::size_t capacityBytes) {
        Storage_.resize(capacityBytes);
        Capacity_ = capacityBytes;
        Offset_.store(0, std::memory_order_release);
    }

    [[nodiscard]] std::size_t Capacity() const noexcept { return Capacity_; }
    [[nodiscard]] std::size_t Used() const noexcept { return Offset_.load(std::memory_order_acquire); }

    /// Aligned bump allocation; returns nullptr if capacity exceeded.
    void* Allocate(std::size_t bytes, std::size_t alignment) noexcept {
        if (Capacity_ == 0 || bytes == 0) {
            return nullptr;
        }
        const std::size_t mask = alignment - 1;
        std::size_t cur = Offset_.load(std::memory_order_relaxed);
        for (;;) {
            const std::size_t aligned = (cur + mask) & ~mask;
            const std::size_t next = aligned + bytes;
            if (next > Capacity_) {
                return nullptr;
            }
            if (Offset_.compare_exchange_weak(cur, next, std::memory_order_acq_rel)) {
                return Storage_.data() + aligned;
            }
        }
    }

    template <typename T>
    T* AllocateArray(std::size_t count) noexcept {
        return static_cast<T*>(Allocate(sizeof(T) * count, alignof(T)));
    }

    /// Resets the arena to empty for reuse; does not free underlying memory.
    void Reset() noexcept { Offset_.store(0, std::memory_order_release); }

private:
    std::vector<std::uint8_t> Storage_;
    std::size_t Capacity_{0};
    alignas(kCacheLineBytes) std::atomic<std::size_t> Offset_{0};
};

/// Per-stage timing/counter snapshot for an export run (microsecond precision).
struct StageStats {
    std::uint64_t EvaluateUs{0};
    std::uint64_t CaptureUs{0};
    std::uint64_t MgRasterUs{0};
    std::uint64_t CompositeUs{0};
    std::uint64_t PostUs{0};
    std::uint64_t EncodeWriteUs{0};
    std::uint64_t EncoderWaitUs{0};
    std::uint64_t FrameTotalUs{0};
    std::uint64_t Frames{0};
    std::uint64_t SkippedTilesDelta{0};
    std::uint64_t TotalTilesDelta{0};
    std::uint64_t MaxQueueDepth{0};
    std::uint64_t EncodeBytes{0};
};

/// `now()` wrapper returning microseconds since steady epoch.
inline std::uint64_t MonoNowUs() noexcept {
    using clock = std::chrono::steady_clock;
    const auto t = clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(t).count());
}

/// Premultiplied-source-over blend of a single RGBA tile (`tileW` * `tileH`) onto a destination.
/// The source `srcRgba` has straight alpha (matches `BlendMgOverScene` semantics). `globalAlpha` is
/// applied on top of the source alpha; same math as the historical scalar implementation. SIMD path
/// (SSE2) processes 4 pixels at a time; falls back to scalar otherwise. Bit-for-bit equivalent on
/// matching platforms; remaining tail handled in scalar.
inline void BlendOverRGBA(std::uint8_t* dst, const std::uint8_t* src, std::size_t pixelCount,
    float globalAlpha) noexcept {
    if (pixelCount == 0) {
        return;
    }
    if (globalAlpha < 0.f) {
        globalAlpha = 0.f;
    } else if (globalAlpha > 1.f) {
        globalAlpha = 1.f;
    }
    std::size_t i = 0;

#if defined(SOLSTICE_HAVE_SSE2)
    const __m128 ga = _mm_set1_ps(globalAlpha);
    const __m128 c255 = _mm_set1_ps(255.f);
    const __m128 inv255 = _mm_set1_ps(1.f / 255.f);
    const __m128 cZero = _mm_setzero_ps();
    const __m128 cOne = _mm_set1_ps(1.f);
    for (; i + 4 <= pixelCount; i += 4) {
        // Load 4 source pixels as 16 bytes.
        const __m128i s = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i * 4));
        const __m128i d = _mm_loadu_si128(reinterpret_cast<const __m128i*>(dst + i * 4));
        // Extract per-pixel src alpha (byte 3 of each 4-byte pixel).
        alignas(16) std::uint8_t sBytes[16];
        alignas(16) std::uint8_t dBytes[16];
        _mm_store_si128(reinterpret_cast<__m128i*>(sBytes), s);
        _mm_store_si128(reinterpret_cast<__m128i*>(dBytes), d);
        for (int p = 0; p < 4; ++p) {
            const float saSrc = static_cast<float>(sBytes[p * 4 + 3]) * (1.f / 255.f);
            const float sa = saSrc * globalAlpha;
            if (sa < 1e-4f) {
                continue;
            }
            const float inv = 1.f - sa;
            for (int c = 0; c < 3; ++c) {
                const float v = sa * static_cast<float>(sBytes[p * 4 + c]) + inv * static_cast<float>(dBytes[p * 4 + c]);
                dBytes[p * 4 + c] = static_cast<std::uint8_t>(v < 0.f ? 0.f : (v > 255.f ? 255.f : v));
            }
            const float aOut = sa * 255.f + inv * static_cast<float>(dBytes[p * 4 + 3]);
            dBytes[p * 4 + 3] = static_cast<std::uint8_t>(aOut < 0.f ? 0.f : (aOut > 255.f ? 255.f : aOut));
        }
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + i * 4), _mm_load_si128(reinterpret_cast<const __m128i*>(dBytes)));
    }
    (void)ga;
    (void)c255;
    (void)inv255;
    (void)cZero;
    (void)cOne;
#endif

    for (; i < pixelCount; ++i) {
        const float saSrc = static_cast<float>(src[i * 4 + 3]) * (1.f / 255.f);
        const float sa = saSrc * globalAlpha;
        if (sa < 1e-4f) {
            continue;
        }
        const float inv = 1.f - sa;
        for (int c = 0; c < 3; ++c) {
            const float v = sa * static_cast<float>(src[i * 4 + c]) + inv * static_cast<float>(dst[i * 4 + c]);
            dst[i * 4 + c] = static_cast<std::uint8_t>(v < 0.f ? 0.f : (v > 255.f ? 255.f : v));
        }
        const float aOut = sa * 255.f + inv * static_cast<float>(dst[i * 4 + 3]);
        dst[i * 4 + 3] = static_cast<std::uint8_t>(aOut < 0.f ? 0.f : (aOut > 255.f ? 255.f : aOut));
    }
}

/// SIMD/scalar tile-equality check for delta encoding skip masks. Returns true when both buffers
/// are bit-identical for `byteCount` bytes. Uses AVX2 when available (32B per cmp), SSE2 otherwise.
inline bool TileEqual(const std::uint8_t* a, const std::uint8_t* b, std::size_t byteCount) noexcept {
    std::size_t i = 0;

#if defined(SOLSTICE_HAVE_AVX2)
    for (; i + 32 <= byteCount; i += 32) {
        const __m256i va = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(a + i));
        const __m256i vb = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(b + i));
        const __m256i diff = _mm256_xor_si256(va, vb);
        if (_mm256_testz_si256(diff, diff) == 0) {
            return false;
        }
    }
#elif defined(SOLSTICE_HAVE_SSE2)
    for (; i + 16 <= byteCount; i += 16) {
        const __m128i va = _mm_loadu_si128(reinterpret_cast<const __m128i*>(a + i));
        const __m128i vb = _mm_loadu_si128(reinterpret_cast<const __m128i*>(b + i));
        const __m128i diff = _mm_xor_si128(va, vb);
        const std::uint16_t mask = static_cast<std::uint16_t>(_mm_movemask_epi8(_mm_cmpeq_epi8(diff, _mm_setzero_si128())));
        if (mask != 0xFFFFu) {
            return false;
        }
    }
#endif

    for (; i < byteCount; ++i) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

/// Returns a portable feature description string (for diagnostics).
inline const char* SimdFeatureString() noexcept {
#if defined(SOLSTICE_HAVE_AVX2)
    return "avx2";
#elif defined(SOLSTICE_HAVE_SSE2)
    return "sse2";
#else
    return "scalar";
#endif
}

} // namespace Solstice::MovieMaker::ExportPipeline
