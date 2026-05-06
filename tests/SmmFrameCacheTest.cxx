// Validates the SMM frame cache (LZX-compressed, mmap-backed disk cache):
//   - Round-trip insert + lookup of a full RGBA frame.
//   - Delta encoding: a near-identical frame stored as XOR-against-base resolves correctly.
//   - TTL eviction: entries past their TTL are dropped on Open() / Prune().
//   - Budget eviction: weighted LRU/LFU score evicts cold entries first.
//   - Persistence: Close() then Open() preserves the index (and hits replay).
//   - Recovery from a corrupt index: Open() drops the index but keeps the cache usable.
//
// All tests use a unique temp directory per case so concurrent ctest runs do not collide.

#include "MovieMaker/Cache/FrameCache.hxx"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace FC = Solstice::MovieMaker::Cache;

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

std::filesystem::path MakeTempRoot(const char* tag) {
    static std::atomic<std::uint64_t> sCounter{0};
    const std::uint64_t n = sCounter.fetch_add(1, std::memory_order_relaxed);
    const auto unique = std::to_string(static_cast<unsigned long long>(
        std::chrono::system_clock::now().time_since_epoch().count())) + "_" + std::to_string(n);
    auto root = std::filesystem::temp_directory_path() / "Solstice" / "FrameCacheTest" / (std::string(tag) + "_" + unique);
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root, ec);
    return root;
}

std::vector<std::uint8_t> MakeFrame(std::uint32_t w, std::uint32_t h, std::uint32_t seed) {
    std::vector<std::uint8_t> out(static_cast<std::size_t>(w) * h * 4);
    std::mt19937 rng(seed);
    for (std::size_t i = 0; i < out.size(); ++i) {
        out[i] = static_cast<std::uint8_t>(rng() & 0xFF);
    }
    return out;
}

std::vector<std::uint8_t> MakeNearlyIdentical(const std::vector<std::uint8_t>& src, std::uint32_t flips) {
    auto out = src;
    std::mt19937 rng(0xC0FFEE);
    for (std::uint32_t i = 0; i < flips; ++i) {
        const std::size_t idx = rng() % out.size();
        out[idx] ^= 0xA5;
    }
    return out;
}

void TestRoundTrip() {
    g_CurrentTest = "RoundTrip";
    const auto root = MakeTempRoot("rt");
    FC::FrameCache cache;
    SMM_REQUIRE(cache.Open(root, FC::FrameCacheOptions{}));
    auto frame = MakeFrame(64, 36, 1);
    const auto key = FC::BuildFrameKey(0xABC, 100, 64, 36, 30, 0);
    SMM_REQUIRE(cache.Insert(key, /*baseKey=*/0, 64, 36,
        std::span<const std::uint8_t>(frame.data(), frame.size())));
    std::vector<std::uint8_t> out;
    SMM_REQUIRE(cache.Lookup(key, out));
    SMM_REQUIRE(out.size() == frame.size());
    SMM_REQUIRE(std::memcmp(out.data(), frame.data(), frame.size()) == 0);
    const auto stats = cache.Stats();
    SMM_REQUIRE(stats.Hits == 1);
    SMM_REQUIRE(stats.Misses == 0);
    SMM_REQUIRE(stats.Inserts == 1);
}

void TestMissReturnsFalse() {
    g_CurrentTest = "MissReturnsFalse";
    const auto root = MakeTempRoot("miss");
    FC::FrameCache cache;
    SMM_REQUIRE(cache.Open(root, FC::FrameCacheOptions{}));
    std::vector<std::uint8_t> out;
    SMM_REQUIRE(!cache.Lookup(0xDEAD, out));
    SMM_REQUIRE(out.empty());
    SMM_REQUIRE(cache.Stats().Misses == 1);
}

void TestDeltaEncoding() {
    g_CurrentTest = "DeltaEncoding";
    const auto root = MakeTempRoot("delta");
    FC::FrameCacheOptions opts{};
    // Lower the threshold so even modest delta wins are taken.
    opts.DeltaWinThreshold = 0.0f;
    FC::FrameCache cache;
    SMM_REQUIRE(cache.Open(root, opts));

    auto base = MakeFrame(128, 64, 7);
    auto next = MakeNearlyIdentical(base, /*flips=*/16); // Heavily correlated -> XOR delta is near-zero.

    const auto k1 = FC::BuildFrameKey(0xABC, 1, 128, 64, 30, 0);
    const auto k2 = FC::BuildFrameKey(0xABC, 2, 128, 64, 30, 0);
    SMM_REQUIRE(cache.Insert(k1, 0, 128, 64,
        std::span<const std::uint8_t>(base.data(), base.size())));
    SMM_REQUIRE(cache.Insert(k2, k1, 128, 64,
        std::span<const std::uint8_t>(next.data(), next.size())));

    const auto stats = cache.Stats();
    SMM_REQUIRE(stats.InsertsDelta == 1);
    SMM_REQUIRE(stats.BytesSavedByDelta > 0);

    std::vector<std::uint8_t> out2;
    SMM_REQUIRE(cache.Lookup(k2, out2));
    SMM_REQUIRE(out2.size() == next.size());
    SMM_REQUIRE(std::memcmp(out2.data(), next.data(), next.size()) == 0);
    SMM_REQUIRE(cache.Stats().HitsDelta == 1);
}

void TestPersistenceAcrossOpen() {
    g_CurrentTest = "PersistenceAcrossOpen";
    const auto root = MakeTempRoot("persist");
    auto frame = MakeFrame(48, 27, 42);
    const auto key = FC::BuildFrameKey(0x1234, 5, 48, 27, 60, 0);
    {
        FC::FrameCache cache;
        SMM_REQUIRE(cache.Open(root, FC::FrameCacheOptions{}));
        SMM_REQUIRE(cache.Insert(key, 0, 48, 27,
            std::span<const std::uint8_t>(frame.data(), frame.size())));
        cache.Flush();
    }
    {
        FC::FrameCache cache;
        SMM_REQUIRE(cache.Open(root, FC::FrameCacheOptions{}));
        SMM_REQUIRE(cache.EntryCount() == 1);
        std::vector<std::uint8_t> out;
        SMM_REQUIRE(cache.Lookup(key, out));
        SMM_REQUIRE(out.size() == frame.size());
        SMM_REQUIRE(std::memcmp(out.data(), frame.data(), frame.size()) == 0);
    }
}

void TestTtlEvictsOnOpen() {
    g_CurrentTest = "TtlEvictsOnOpen";
    const auto root = MakeTempRoot("ttl");
    auto frame = MakeFrame(32, 18, 99);
    const auto key = FC::BuildFrameKey(0xABCD, 1, 32, 18, 30, 0);
    {
        FC::FrameCache cache;
        FC::FrameCacheOptions opts{};
        // Set TTL very large so insert is kept.
        opts.TtlSeconds = 3600;
        SMM_REQUIRE(cache.Open(root, opts));
        SMM_REQUIRE(cache.Insert(key, 0, 32, 18,
            std::span<const std::uint8_t>(frame.data(), frame.size())));
    }
    // Reopen with TTL=1 second and sleep so the entry is expired on the next Open.
    std::this_thread::sleep_for(std::chrono::seconds(2));
    {
        FC::FrameCache cache;
        FC::FrameCacheOptions opts{};
        opts.TtlSeconds = 1;
        SMM_REQUIRE(cache.Open(root, opts));
        SMM_REQUIRE(cache.EntryCount() == 0);
        std::vector<std::uint8_t> out;
        SMM_REQUIRE(!cache.Lookup(key, out));
    }
}

void TestBudgetEviction() {
    g_CurrentTest = "BudgetEviction";
    const auto root = MakeTempRoot("budget");
    FC::FrameCache cache;
    FC::FrameCacheOptions opts{};
    // Budget tight enough to evict after a few inserts.
    opts.BudgetBytes = 64u * 1024u;
    opts.TtlSeconds = 0; // disable TTL eviction
    SMM_REQUIRE(cache.Open(root, opts));

    // Insert 8 frames of distinct random content, each ~64 KiB raw.
    std::vector<std::uint64_t> keys;
    for (std::uint32_t i = 0; i < 8; ++i) {
        auto frame = MakeFrame(128, 32, 100 + i);
        const auto k = FC::BuildFrameKey(0xC0DE, i, 128, 32, 30, 0);
        keys.push_back(k);
        cache.Insert(k, 0, 128, 32,
            std::span<const std::uint8_t>(frame.data(), frame.size()));
    }
    // After all inserts, budget enforcement must have evicted at least some entries.
    const auto stats = cache.Stats();
    SMM_REQUIRE(stats.Evicted > 0);
    SMM_REQUIRE(cache.EntryCount() < keys.size());
    SMM_REQUIRE(stats.BytesStored <= opts.BudgetBytes);
}

void TestCorruptIndexIsRecoverable() {
    g_CurrentTest = "CorruptIndexIsRecoverable";
    const auto root = MakeTempRoot("corrupt");
    {
        FC::FrameCache cache;
        SMM_REQUIRE(cache.Open(root, FC::FrameCacheOptions{}));
        auto frame = MakeFrame(16, 9, 1);
        const auto key = FC::BuildFrameKey(0x1, 1, 16, 9, 30, 0);
        SMM_REQUIRE(cache.Insert(key, 0, 16, 9,
            std::span<const std::uint8_t>(frame.data(), frame.size())));
        cache.Flush();
    }
    // Truncate / corrupt the index.
    {
        std::ofstream f(root / "index.bin", std::ios::binary | std::ios::trunc);
        const char garbage[] = "NOT_A_VALID_INDEX_HEADER";
        f.write(garbage, sizeof(garbage) - 1);
    }
    {
        FC::FrameCache cache;
        SMM_REQUIRE(cache.Open(root, FC::FrameCacheOptions{}));
        // Index discarded; cache should still be usable for new inserts.
        SMM_REQUIRE(cache.EntryCount() == 0);
        auto frame = MakeFrame(16, 9, 2);
        const auto key = FC::BuildFrameKey(0x1, 2, 16, 9, 30, 0);
        SMM_REQUIRE(cache.Insert(key, 0, 16, 9,
            std::span<const std::uint8_t>(frame.data(), frame.size())));
        std::vector<std::uint8_t> out;
        SMM_REQUIRE(cache.Lookup(key, out));
        SMM_REQUIRE(out.size() == frame.size());
        SMM_REQUIRE(std::memcmp(out.data(), frame.data(), frame.size()) == 0);
    }
}

void TestCompactReclaimsSpace() {
    g_CurrentTest = "CompactReclaimsSpace";
    const auto root = MakeTempRoot("compact");
    FC::FrameCache cache;
    FC::FrameCacheOptions opts{};
    opts.BudgetBytes = 0; // unbounded so we control eviction explicitly.
    SMM_REQUIRE(cache.Open(root, opts));
    std::vector<std::uint64_t> keys;
    for (std::uint32_t i = 0; i < 6; ++i) {
        auto frame = MakeFrame(64, 36, 200 + i);
        const auto k = FC::BuildFrameKey(0xBAD, i, 64, 36, 30, 0);
        keys.push_back(k);
        SMM_REQUIRE(cache.Insert(k, 0, 64, 36,
            std::span<const std::uint8_t>(frame.data(), frame.size())));
    }
    SMM_REQUIRE(cache.Compact());
    // All entries still resolvable after compaction.
    for (auto k : keys) {
        std::vector<std::uint8_t> out;
        SMM_REQUIRE(cache.Lookup(k, out));
    }
}

void TestFnv1aStable() {
    g_CurrentTest = "Fnv1aStable";
    const std::string s = "solstice-frame-cache";
    const auto h = FC::Fnv1a64(std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(s.data()), s.size()));
    SMM_REQUIRE(h != 0);
    // Same input twice must give the same hash.
    const auto h2 = FC::Fnv1a64(std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(s.data()), s.size()));
    SMM_REQUIRE(h == h2);
    // Build-frame-key is deterministic too.
    const auto k1 = FC::BuildFrameKey(0xAA, 7, 100, 50, 30, 0xBB);
    const auto k2 = FC::BuildFrameKey(0xAA, 7, 100, 50, 30, 0xBB);
    SMM_REQUIRE(k1 == k2);
    // Tick changes the key.
    const auto k3 = FC::BuildFrameKey(0xAA, 8, 100, 50, 30, 0xBB);
    SMM_REQUIRE(k1 != k3);
}

} // namespace

int main() {
    TestFnv1aStable();
    TestRoundTrip();
    TestMissReturnsFalse();
    TestDeltaEncoding();
    TestPersistenceAcrossOpen();
    TestBudgetEviction();
    TestCorruptIndexIsRecoverable();
    TestCompactReclaimsSpace();
    // TTL test sleeps for 2 s; run last so quicker tests fail fast.
    TestTtlEvictsOnOpen();
    if (g_Failures == 0) {
        std::printf("[OK] SmmFrameCacheTest: all checks passed.\n");
        return 0;
    }
    std::fprintf(stderr, "[FAIL] SmmFrameCacheTest: %d failure(s).\n", g_Failures);
    return 1;
}
