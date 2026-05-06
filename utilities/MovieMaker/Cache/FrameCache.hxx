#pragma once

#include <Core/Platform/Mmap.hxx>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace Solstice::MovieMaker::Cache {

/// Options controlling on-disk persistence, eviction, and weighted LRU/LFU balance for the
/// SMM frame cache. The defaults are sized for a "few minutes of 1080p60" working set; callers
/// at higher resolutions should bump `BudgetBytes` proportionally (frames compress aggressively
/// for static segments but are bounded by the LZX worst case ~= source size).
struct FrameCacheOptions {
    /// Hard cap on the live (referenced) compressed bytes. Eviction enforces this; orphaned
    /// blob bytes from evicted entries persist on disk until `Compact()` reclaims them.
    std::uint64_t BudgetBytes{4ull * 1024ull * 1024ull * 1024ull}; // 4 GiB

    /// Time-to-live in seconds since `CreatedEpochS`. Entries older than this are evicted
    /// unconditionally on `Prune()` (and on `Open()` when reloading the index).
    /// `0` disables TTL eviction.
    std::uint64_t TtlSeconds{7ull * 24ull * 60ull * 60ull}; // 7 days

    /// Weighted-LRU/LFU eviction balance. The eviction score for an entry is
    ///   score = WLru * recency + WLfu * log2(1 + AccessCount)
    /// where `recency` is in `[0,1]` (1 = most recent). Higher score = harder to evict.
    /// Defaults bias toward recency so a re-export of a recent timeline keeps its frames.
    float WLru{0.6f};
    float WLfu{0.4f};

    /// When inserting a frame, compute the XOR delta against `BaseKeyHash`; if the compressed
    /// delta is smaller than the compressed full frame by at least `DeltaWinThreshold * 100` %,
    /// the entry is stored as a delta (otherwise as a full frame). Set to `1.f` to disable
    /// delta encoding; set to `0.f` to always prefer a delta when smaller.
    float DeltaWinThreshold{0.05f};

    /// When true, mmap the blob file for reads. Enables zero-copy decompression-from-mapped-bytes
    /// for callers that hold the cache open (e.g. an active export session).
    bool UseMmap{true};

    /// When true, lookup misses do not retain any state; useful for read-only inspection.
    bool ReadOnly{false};
};

/// Aggregate counters for an export run; reset by `ResetStats()` and surfaced via DiagLog.
struct FrameCacheStats {
    std::uint64_t Lookups{0};
    std::uint64_t Hits{0};        // total hits (full + delta)
    std::uint64_t HitsDelta{0};   // hits that resolved through a base + delta XOR
    std::uint64_t Misses{0};
    std::uint64_t Inserts{0};     // entries newly stored
    std::uint64_t InsertsDelta{0};// entries stored as deltas
    std::uint64_t Evicted{0};     // entries removed by Prune()
    std::uint64_t EvictedTtl{0};  // entries removed by TTL specifically
    std::uint64_t BytesStored{0}; // current live compressed bytes
    std::uint64_t BytesSavedByDelta{0}; // (full-compressed - delta-compressed), accumulated
    std::uint64_t BytesReadCompressed{0};
    std::uint64_t BytesWrittenCompressed{0};

    [[nodiscard]] double HitRate() const noexcept {
        return Lookups == 0 ? 0.0 : static_cast<double>(Hits) / static_cast<double>(Lookups);
    }
};

/// Standard FNV-1a 64-bit content hash. Stable across platforms; used for cache keys and
/// integrity checks on uncompressed payloads.
[[nodiscard]] std::uint64_t Fnv1a64(std::span<const std::byte> data) noexcept;
[[nodiscard]] std::uint64_t Fnv1a64Update(std::uint64_t seed, std::span<const std::byte> data) noexcept;

/// Helper that derives a stable cache key from the inputs that determine the rendered frame.
/// Caller is responsible for ensuring the `sceneFingerprint` reflects the relevant authoring
/// state (e.g. the .prlx bytes hash, plus any transient SMM authoring overrides).
[[nodiscard]] std::uint64_t BuildFrameKey(std::uint64_t sceneFingerprint, std::uint64_t tick,
    std::uint32_t width, std::uint32_t height, std::uint32_t fps,
    std::uint64_t postProcessFingerprint) noexcept;

/// Persistent disk-backed cache for SMM rendered frames. Storage is `index.bin` + `blob.dat`
/// under `RootPath`; the index is loaded fully into memory while the blob file is read by
/// random-access `pread` (POSIX) or memory mapping (`MmapFile`). Inserts append-only; eviction
/// rebuilds the blob during `Compact()` when fragmentation exceeds a threshold.
///
/// Threading model: the public methods are guarded by an internal mutex so concurrent
/// `Lookup()`/`Insert()` from a producer/consumer pipeline are safe. The mutex is per-instance,
/// not global; multiple `FrameCache` objects on different paths run independently.
class FrameCache {
public:
    FrameCache();
    ~FrameCache();
    FrameCache(const FrameCache&) = delete;
    FrameCache& operator=(const FrameCache&) = delete;

    /// Opens (or creates) a cache rooted at `path`. Loads the existing manifest if present
    /// and prunes TTL-expired entries before returning. Returns false on I/O error; in that
    /// case the cache is still usable in-memory but will not persist.
    bool Open(const std::filesystem::path& path, const FrameCacheOptions& options);

    /// Flushes any pending index changes and closes the underlying files. Idempotent.
    void Close();

    /// Returns true if the cache file backing has been opened (mmap or stream).
    [[nodiscard]] bool IsOpen() const noexcept { return m_Open; }

    /// Looks up a frame by key. On hit, decompresses (and applies delta if needed) into
    /// `outBytes`. The returned data is RGBA8 packed top-down at the resolution that was
    /// stored. Returns false on miss.
    bool Lookup(std::uint64_t keyHash, std::vector<std::uint8_t>& outBytes);

    /// Inserts a frame. If `baseKeyHash` is non-zero and refers to an existing full-frame entry
    /// in the cache, a delta encoding is attempted; the smaller of (delta, full) is stored.
    /// `bytes` must be a full RGBA8 frame.
    /// Returns true on successful insert, false on I/O error or capacity exhaustion that
    /// could not be resolved by eviction.
    bool Insert(std::uint64_t keyHash, std::uint64_t baseKeyHash,
        std::uint32_t width, std::uint32_t height,
        std::span<const std::uint8_t> bytes);

    /// Drops TTL-expired entries and (if needed) evicts low-scoring entries until the live
    /// bytes drop under `BudgetBytes`. Returns the number of entries removed.
    std::size_t Prune();

    /// Rewrites the blob file with only live entries. Reduces fragmentation after many evictions.
    /// Safe to call from a single thread; not concurrent with Lookup/Insert. Returns false on
    /// I/O error.
    bool Compact();

    /// Persists the current index to disk via tmp + rename. Called automatically by Close()
    /// and on `BytesStored` crossing the budget; callers can flush eagerly for crash-safety.
    bool Flush();

    /// Returns the current statistics (counters for the lifetime of this object).
    FrameCacheStats Stats() const;
    void ResetStats();

    /// Total entries currently in the index (including ones whose blob bytes were evicted
    /// from the file backing).
    std::size_t EntryCount() const;

    /// Returns the on-disk root directory.
    [[nodiscard]] const std::filesystem::path& RootPath() const noexcept { return m_Root; }

    /// Test/diagnostic helper: returns true if `keyHash` exists as either a full entry or a
    /// resolvable delta entry. Does not bump access counters.
    [[nodiscard]] bool Contains(std::uint64_t keyHash) const;

private:
    struct Entry {
        std::uint64_t KeyHash{0};
        std::uint64_t BaseKeyHash{0};
        std::uint64_t BlobOffset{0};
        std::uint64_t CompressedSize{0};
        std::uint64_t UncompressedSize{0};
        std::uint64_t ContentChecksum{0};
        std::uint64_t CreatedEpochS{0};
        std::uint64_t LastAccessEpochS{0};
        std::uint32_t Width{0};
        std::uint32_t Height{0};
        std::uint32_t AccessCount{0};
        std::uint8_t IsDelta{0};
        std::uint8_t Reserved[7]{0, 0, 0, 0, 0, 0, 0};
    };
    static_assert(sizeof(Entry) % 8 == 0, "Entry must be 8-byte aligned for compact serialization");

    bool LoadIndex_();
    bool WriteIndexAtomic_();
    void RemapBlob_();
    bool ReadCompressedBytes_(const Entry& e, std::vector<std::byte>& out) const;
    bool DecompressInto_(const Entry& e, std::span<const std::byte> compressed,
        std::vector<std::uint8_t>& out) const;
    void TouchAccess_(Entry& e);
    bool EvictUntilFits_(std::uint64_t pendingExtra);
    float ScoreEntry_(const Entry& e, std::uint64_t nowS) const noexcept;
    static std::uint64_t NowEpochS_() noexcept;

    mutable std::mutex m_Mutex;
    std::filesystem::path m_Root;
    std::filesystem::path m_IndexPath;
    std::filesystem::path m_BlobPath;
    FrameCacheOptions m_Options{};
    std::unordered_map<std::uint64_t, Entry> m_Entries;
    std::uint64_t m_NextBlobOffset{0};
    bool m_Open{false};
    bool m_DirtyIndex{false};

    Solstice::Core::MmapFile m_BlobMmap;
    std::uint64_t m_MmapValidSize{0};

    mutable FrameCacheStats m_Stats{};
};

} // namespace Solstice::MovieMaker::Cache
