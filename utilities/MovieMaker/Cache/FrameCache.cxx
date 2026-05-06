// SMM frame cache: persistent, LZX-compressed, mmap-backed cache of rendered frames keyed by
// content hash. Stores either full RGBA8 frames or XOR deltas against a base frame, whichever
// compresses smaller. Eviction is a weighted blend of LRU (recency) and LFU (access count) so a
// "warm" working set survives across exports; TTL drops stale entries unconditionally.
//
// On-disk layout (Root/):
//   index.bin:
//       Header { magic, version, reserved, entryCount, nextBlobOffset, statsReserved... }
//       Entry  [entryCount]
//   blob.dat:
//       <append-only compressed payloads at Entry::BlobOffset, length=Entry::CompressedSize>
//
// Atomicity: index updates are written to `index.bin.tmp` and renamed; partial writes leave the
// previous index intact. The blob is grow-only between Compact() calls so a torn write extends
// `nextBlobOffset` only after the bytes are durably appended to disk.

#include "Cache/FrameCache.hxx"

#include <Core/System/LZX.hxx>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <system_error>

namespace Solstice::MovieMaker::Cache {

namespace {

constexpr std::uint64_t kIndexMagic = 0x534D4D5F46434400ull; // "SMM_FCD\0"
constexpr std::uint32_t kIndexVersion = 1;

struct IndexHeader {
    std::uint64_t Magic{0};
    std::uint32_t Version{0};
    std::uint32_t Reserved{0};
    std::uint64_t EntryCount{0};
    std::uint64_t NextBlobOffset{0};
    std::uint64_t LifetimeReserved[4]{0, 0, 0, 0};
};
static_assert(sizeof(IndexHeader) == 64, "IndexHeader layout must be stable for on-disk format");

bool WriteFileAtomic(const std::filesystem::path& target, std::span<const std::byte> bytes) {
    std::error_code ec;
    std::filesystem::path tmp = target;
    tmp += ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) {
            return false;
        }
        f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!f.good()) {
            f.close();
            std::filesystem::remove(tmp, ec);
            return false;
        }
        // ofstream destructor flushes/closes here.
    }
    std::filesystem::remove(target, ec);
    std::filesystem::rename(tmp, target, ec);
    if (ec) {
        std::filesystem::remove(tmp, ec);
        return false;
    }
    return true;
}

bool AppendFile(const std::filesystem::path& path, std::span<const std::byte> bytes,
    std::uint64_t& outOffset) {
    // Take the offset from the stat'd file size before opening so it is well-defined regardless
    // of the platform's interpretation of `std::ios::app + tellp()` semantics. The blob file is
    // single-writer (the export step is single-threaded), so this is race-free in practice.
    std::error_code ec;
    outOffset = std::filesystem::exists(path, ec) ? std::filesystem::file_size(path, ec) : 0u;
    if (ec) {
        return false;
    }
    std::ofstream f(path, std::ios::binary | std::ios::app);
    if (!f) {
        return false;
    }
    f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return f.good();
}

bool ReadFileRange(const std::filesystem::path& path, std::uint64_t offset, std::size_t length,
    std::vector<std::byte>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        return false;
    }
    f.seekg(static_cast<std::streamoff>(offset));
    out.resize(length);
    f.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(length));
    return f.good() || (static_cast<std::size_t>(f.gcount()) == length);
}

} // namespace

std::uint64_t Fnv1a64(std::span<const std::byte> data) noexcept {
    return Fnv1a64Update(0xCBF29CE484222325ull, data);
}

std::uint64_t Fnv1a64Update(std::uint64_t seed, std::span<const std::byte> data) noexcept {
    std::uint64_t h = seed;
    for (std::byte b : data) {
        h ^= static_cast<std::uint8_t>(b);
        h *= 0x100000001B3ull;
    }
    return h;
}

std::uint64_t BuildFrameKey(std::uint64_t sceneFingerprint, std::uint64_t tick,
    std::uint32_t width, std::uint32_t height, std::uint32_t fps,
    std::uint64_t postProcessFingerprint) noexcept {
    std::uint64_t h = 0xCBF29CE484222325ull;
    auto mix = [&](std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            h ^= static_cast<std::uint8_t>(v & 0xFF);
            h *= 0x100000001B3ull;
            v >>= 8;
        }
    };
    mix(sceneFingerprint);
    mix(tick);
    mix(static_cast<std::uint64_t>(width) | (static_cast<std::uint64_t>(height) << 32));
    mix(static_cast<std::uint64_t>(fps));
    mix(postProcessFingerprint);
    return h;
}

FrameCache::FrameCache() = default;

FrameCache::~FrameCache() {
    Close();
}

bool FrameCache::Open(const std::filesystem::path& path, const FrameCacheOptions& options) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    Close();
    m_Root = path;
    m_Options = options;
    m_IndexPath = m_Root / "index.bin";
    m_BlobPath = m_Root / "blob.dat";

    std::error_code ec;
    std::filesystem::create_directories(m_Root, ec);
    if (ec) {
        return false;
    }

    if (!std::filesystem::exists(m_BlobPath, ec)) {
        // Touch the blob file so subsequent appends don't race with directory creation.
        std::ofstream f(m_BlobPath, std::ios::binary | std::ios::trunc);
        if (!f) {
            return false;
        }
    }

    m_Entries.clear();
    m_NextBlobOffset = 0;
    m_DirtyIndex = false;

    if (std::filesystem::exists(m_IndexPath, ec)) {
        if (!LoadIndex_()) {
            // Corrupt index: discard but keep the blob file (will be wiped on first Compact()).
            m_Entries.clear();
            m_NextBlobOffset = std::filesystem::file_size(m_BlobPath, ec);
            if (ec) {
                m_NextBlobOffset = 0;
            }
            m_DirtyIndex = true;
        }
    }

    if (m_Options.UseMmap) {
        RemapBlob_();
    }

    m_Open = true;

    // TTL prune on open so a long-shelved cache doesn't ride forever.
    if (m_Options.TtlSeconds > 0) {
        const std::uint64_t now = NowEpochS_();
        std::vector<std::uint64_t> stale;
        stale.reserve(m_Entries.size() / 16 + 1);
        for (const auto& kv : m_Entries) {
            if (now > kv.second.CreatedEpochS && (now - kv.second.CreatedEpochS) > m_Options.TtlSeconds) {
                stale.push_back(kv.first);
            }
        }
        for (auto k : stale) {
            m_Entries.erase(k);
            ++m_Stats.Evicted;
            ++m_Stats.EvictedTtl;
            m_DirtyIndex = true;
        }
    }

    return true;
}

void FrameCache::Close() {
    if (!m_Open) {
        m_BlobMmap.Close();
        return;
    }
    if (m_DirtyIndex) {
        WriteIndexAtomic_();
    }
    m_BlobMmap.Close();
    m_MmapValidSize = 0;
    m_Open = false;
    m_DirtyIndex = false;
}

bool FrameCache::LoadIndex_() {
    std::ifstream f(m_IndexPath, std::ios::binary);
    if (!f) {
        return false;
    }
    IndexHeader hdr{};
    f.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
    if (!f.good() || hdr.Magic != kIndexMagic || hdr.Version != kIndexVersion) {
        return false;
    }
    if (hdr.EntryCount > (1ull << 30)) {
        return false; // sanity bound: 1 G entries.
    }
    m_NextBlobOffset = hdr.NextBlobOffset;
    m_Entries.reserve(static_cast<std::size_t>(hdr.EntryCount));
    for (std::uint64_t i = 0; i < hdr.EntryCount; ++i) {
        Entry e{};
        f.read(reinterpret_cast<char*>(&e), sizeof(e));
        if (!f.good()) {
            return false;
        }
        m_Entries.emplace(e.KeyHash, e);
    }
    return true;
}

bool FrameCache::WriteIndexAtomic_() {
    std::vector<std::byte> buf;
    buf.resize(sizeof(IndexHeader) + m_Entries.size() * sizeof(Entry));
    auto* p = buf.data();
    IndexHeader hdr{};
    hdr.Magic = kIndexMagic;
    hdr.Version = kIndexVersion;
    hdr.Reserved = 0;
    hdr.EntryCount = static_cast<std::uint64_t>(m_Entries.size());
    hdr.NextBlobOffset = m_NextBlobOffset;
    std::memcpy(p, &hdr, sizeof(hdr));
    p += sizeof(hdr);
    for (const auto& kv : m_Entries) {
        std::memcpy(p, &kv.second, sizeof(Entry));
        p += sizeof(Entry);
    }
    if (!WriteFileAtomic(m_IndexPath, std::span<const std::byte>(buf.data(), buf.size()))) {
        return false;
    }
    m_DirtyIndex = false;
    return true;
}

void FrameCache::RemapBlob_() {
    m_BlobMmap.Close();
    m_MmapValidSize = 0;
    std::error_code ec;
    const auto size = std::filesystem::file_size(m_BlobPath, ec);
    if (ec || size == 0) {
        return;
    }
    if (m_BlobMmap.Open(m_BlobPath)) {
        m_MmapValidSize = static_cast<std::uint64_t>(size);
    }
}

bool FrameCache::ReadCompressedBytes_(const Entry& e, std::vector<std::byte>& out) const {
    out.resize(static_cast<std::size_t>(e.CompressedSize));
    if (m_Options.UseMmap && m_BlobMmap.IsOpen()
        && (e.BlobOffset + e.CompressedSize) <= m_MmapValidSize) {
        std::span<const std::byte> view = m_BlobMmap.Read(e.BlobOffset,
            static_cast<std::size_t>(e.CompressedSize));
        if (view.size() == e.CompressedSize) {
            std::memcpy(out.data(), view.data(), out.size());
            return true;
        }
    }
    return ReadFileRange(m_BlobPath, e.BlobOffset, static_cast<std::size_t>(e.CompressedSize), out);
}

bool FrameCache::DecompressInto_(const Entry& e, std::span<const std::byte> compressed,
    std::vector<std::uint8_t>& out) const {
    std::vector<std::byte> decompressed = Solstice::Core::LZXDecompress(compressed,
        static_cast<std::size_t>(e.UncompressedSize));
    if (decompressed.size() != e.UncompressedSize) {
        return false;
    }
    out.resize(decompressed.size());
    std::memcpy(out.data(), decompressed.data(), decompressed.size());
    return true;
}

bool FrameCache::Lookup(std::uint64_t keyHash, std::vector<std::uint8_t>& outBytes) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    ++m_Stats.Lookups;
    auto it = m_Entries.find(keyHash);
    if (it == m_Entries.end()) {
        ++m_Stats.Misses;
        return false;
    }
    Entry& e = it->second;

    std::vector<std::byte> compressed;
    if (!ReadCompressedBytes_(e, compressed)) {
        ++m_Stats.Misses;
        return false;
    }
    m_Stats.BytesReadCompressed += compressed.size();

    std::vector<std::uint8_t> raw;
    if (!DecompressInto_(e, std::span<const std::byte>(compressed.data(), compressed.size()), raw)) {
        // Decompression failure → integrity issue. Drop the entry so the export retries.
        m_Entries.erase(it);
        m_DirtyIndex = true;
        ++m_Stats.Misses;
        return false;
    }

    if (!e.IsDelta) {
        // Verify content checksum.
        const std::uint64_t cs = Fnv1a64(std::span<const std::byte>(
            reinterpret_cast<const std::byte*>(raw.data()), raw.size()));
        if (cs != e.ContentChecksum) {
            m_Entries.erase(keyHash);
            m_DirtyIndex = true;
            ++m_Stats.Misses;
            return false;
        }
        TouchAccess_(e);
        ++m_Stats.Hits;
        outBytes = std::move(raw);
        return true;
    }

    // Delta entry: resolve the base, then XOR.
    auto base = m_Entries.find(e.BaseKeyHash);
    if (base == m_Entries.end() || base->second.IsDelta != 0) {
        ++m_Stats.Misses;
        return false;
    }
    std::vector<std::byte> baseCompressed;
    if (!ReadCompressedBytes_(base->second, baseCompressed)) {
        ++m_Stats.Misses;
        return false;
    }
    m_Stats.BytesReadCompressed += baseCompressed.size();
    std::vector<std::uint8_t> baseRaw;
    if (!DecompressInto_(base->second,
            std::span<const std::byte>(baseCompressed.data(), baseCompressed.size()), baseRaw)) {
        ++m_Stats.Misses;
        return false;
    }
    if (baseRaw.size() != raw.size()) {
        ++m_Stats.Misses;
        return false;
    }
    for (std::size_t i = 0; i < raw.size(); ++i) {
        raw[i] ^= baseRaw[i];
    }
    const std::uint64_t cs = Fnv1a64(std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(raw.data()), raw.size()));
    if (cs != e.ContentChecksum) {
        m_Entries.erase(keyHash);
        m_DirtyIndex = true;
        ++m_Stats.Misses;
        return false;
    }
    TouchAccess_(e);
    TouchAccess_(base->second);
    ++m_Stats.Hits;
    ++m_Stats.HitsDelta;
    outBytes = std::move(raw);
    return true;
}

bool FrameCache::Insert(std::uint64_t keyHash, std::uint64_t baseKeyHash,
    std::uint32_t width, std::uint32_t height, std::span<const std::uint8_t> bytes) {
    if (m_Options.ReadOnly) {
        return false;
    }
    if (bytes.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_Mutex);

    // Already present → skip (caller decides whether to overwrite by erasing first).
    if (m_Entries.find(keyHash) != m_Entries.end()) {
        return true;
    }

    const std::uint64_t contentChecksum = Fnv1a64(std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()));

    // Try full encoding.
    std::vector<std::byte> fullCompressed = Solstice::Core::LZXCompress(
        std::span<const std::byte>(reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()));

    // Try delta encoding when a viable base exists.
    bool storeDelta = false;
    std::vector<std::byte> deltaCompressed;
    auto baseIt = baseKeyHash != 0 ? m_Entries.find(baseKeyHash) : m_Entries.end();
    if (baseIt != m_Entries.end() && baseIt->second.IsDelta == 0
        && baseIt->second.UncompressedSize == bytes.size()
        && baseIt->second.Width == width && baseIt->second.Height == height) {
        std::vector<std::byte> baseCompressedBytes;
        std::vector<std::uint8_t> baseRaw;
        if (ReadCompressedBytes_(baseIt->second, baseCompressedBytes)
            && DecompressInto_(baseIt->second,
                   std::span<const std::byte>(baseCompressedBytes.data(), baseCompressedBytes.size()), baseRaw)
            && baseRaw.size() == bytes.size()) {
            std::vector<std::uint8_t> diff(bytes.size());
            for (std::size_t i = 0; i < bytes.size(); ++i) {
                diff[i] = static_cast<std::uint8_t>(bytes[i] ^ baseRaw[i]);
            }
            deltaCompressed = Solstice::Core::LZXCompress(std::span<const std::byte>(
                reinterpret_cast<const std::byte*>(diff.data()), diff.size()));
            const std::size_t fullSz = fullCompressed.size();
            const std::size_t deltaSz = deltaCompressed.size();
            const float winRatio = fullSz == 0 ? 0.f
                : 1.f - (static_cast<float>(deltaSz) / static_cast<float>(fullSz));
            if (winRatio >= m_Options.DeltaWinThreshold) {
                storeDelta = true;
                m_Stats.BytesSavedByDelta += (fullSz > deltaSz) ? (fullSz - deltaSz) : 0;
            }
        }
    }

    std::vector<std::byte>& chosen = storeDelta ? deltaCompressed : fullCompressed;

    // Make room.
    if (!EvictUntilFits_(static_cast<std::uint64_t>(chosen.size()))) {
        return false;
    }

    std::uint64_t writeOffset = 0;
    if (!AppendFile(m_BlobPath, std::span<const std::byte>(chosen.data(), chosen.size()), writeOffset)) {
        return false;
    }
    // Sanity: the on-disk offset should match our running cursor (within race-free single-thread
    // append). If not, trust the file system and update.
    if (writeOffset != m_NextBlobOffset) {
        m_NextBlobOffset = writeOffset;
    }

    Entry e{};
    e.KeyHash = keyHash;
    e.BaseKeyHash = storeDelta ? baseKeyHash : 0ull;
    e.BlobOffset = writeOffset;
    e.CompressedSize = static_cast<std::uint64_t>(chosen.size());
    e.UncompressedSize = static_cast<std::uint64_t>(bytes.size());
    e.ContentChecksum = contentChecksum;
    e.CreatedEpochS = NowEpochS_();
    e.LastAccessEpochS = e.CreatedEpochS;
    e.Width = width;
    e.Height = height;
    e.AccessCount = 1;
    e.IsDelta = storeDelta ? 1 : 0;

    m_NextBlobOffset = writeOffset + e.CompressedSize;
    m_Entries.emplace(keyHash, e);

    ++m_Stats.Inserts;
    if (storeDelta) {
        ++m_Stats.InsertsDelta;
    }
    m_Stats.BytesStored += e.CompressedSize;
    m_Stats.BytesWrittenCompressed += e.CompressedSize;
    m_DirtyIndex = true;

    // Refresh mmap so subsequent Lookup() within the same session can see the appended bytes.
    if (m_Options.UseMmap) {
        RemapBlob_();
    }

    // Periodic durability flush — every 64 entries inserts the index, keeping crash exposure
    // bounded without amortizing a write per frame.
    if ((m_Stats.Inserts & 0x3F) == 0) {
        WriteIndexAtomic_();
    }

    return true;
}

void FrameCache::TouchAccess_(Entry& e) {
    e.LastAccessEpochS = NowEpochS_();
    if (e.AccessCount < std::numeric_limits<std::uint32_t>::max()) {
        ++e.AccessCount;
    }
    m_DirtyIndex = true;
}

float FrameCache::ScoreEntry_(const Entry& e, std::uint64_t nowS) const noexcept {
    // Recency in [0,1]: 1 just-touched, 0 at TTL boundary (or 1 day if TTL is 0).
    const std::uint64_t maxAgeS = m_Options.TtlSeconds > 0 ? m_Options.TtlSeconds : (24ull * 60ull * 60ull);
    const std::uint64_t ageS = nowS > e.LastAccessEpochS ? (nowS - e.LastAccessEpochS) : 0ull;
    const double recency = ageS >= maxAgeS ? 0.0
        : 1.0 - static_cast<double>(ageS) / static_cast<double>(maxAgeS);
    const double freq = std::log2(1.0 + static_cast<double>(e.AccessCount));
    return static_cast<float>(static_cast<double>(m_Options.WLru) * recency
        + static_cast<double>(m_Options.WLfu) * freq);
}

bool FrameCache::EvictUntilFits_(std::uint64_t pendingExtra) {
    const std::uint64_t budget = m_Options.BudgetBytes;
    if (budget == 0) {
        return true; // unbounded
    }
    std::uint64_t stored = 0;
    for (const auto& kv : m_Entries) {
        stored += kv.second.CompressedSize;
    }
    if (stored + pendingExtra <= budget) {
        return true;
    }

    // First sweep: drop TTL-expired entries.
    if (m_Options.TtlSeconds > 0) {
        const std::uint64_t now = NowEpochS_();
        std::vector<std::uint64_t> stale;
        for (const auto& kv : m_Entries) {
            if (now > kv.second.CreatedEpochS && (now - kv.second.CreatedEpochS) > m_Options.TtlSeconds) {
                stale.push_back(kv.first);
            }
        }
        for (auto k : stale) {
            auto it = m_Entries.find(k);
            if (it != m_Entries.end()) {
                stored -= std::min<std::uint64_t>(stored, it->second.CompressedSize);
                m_Entries.erase(it);
                ++m_Stats.Evicted;
                ++m_Stats.EvictedTtl;
                m_DirtyIndex = true;
            }
        }
        if (stored + pendingExtra <= budget) {
            return true;
        }
    }

    // Score-driven eviction. Build a list and sort ascending; remove until under budget.
    struct Scored {
        std::uint64_t Key;
        std::uint64_t Bytes;
        float Score;
    };
    std::vector<Scored> scored;
    scored.reserve(m_Entries.size());
    const std::uint64_t now = NowEpochS_();
    for (const auto& kv : m_Entries) {
        // Prefer keeping non-delta entries (deltas depend on a base; eviction of the base
        // strands the delta). Deltas get a tiny score bump down so they evict first.
        Scored s{kv.first, kv.second.CompressedSize, ScoreEntry_(kv.second, now)};
        if (kv.second.IsDelta) {
            s.Score *= 0.85f;
        }
        scored.push_back(s);
    }
    std::sort(scored.begin(), scored.end(), [](const Scored& a, const Scored& b) {
        return a.Score < b.Score;
    });
    for (const auto& s : scored) {
        if (stored + pendingExtra <= budget) {
            break;
        }
        auto it = m_Entries.find(s.Key);
        if (it == m_Entries.end()) {
            continue;
        }
        // Evicting a base would orphan its deltas; drop them too.
        if (it->second.IsDelta == 0) {
            std::vector<std::uint64_t> orphans;
            for (const auto& kv : m_Entries) {
                if (kv.second.IsDelta && kv.second.BaseKeyHash == s.Key) {
                    orphans.push_back(kv.first);
                }
            }
            for (auto o : orphans) {
                auto oi = m_Entries.find(o);
                if (oi != m_Entries.end()) {
                    stored -= std::min<std::uint64_t>(stored, oi->second.CompressedSize);
                    m_Entries.erase(oi);
                    ++m_Stats.Evicted;
                    m_DirtyIndex = true;
                }
            }
        }
        stored -= std::min<std::uint64_t>(stored, it->second.CompressedSize);
        m_Entries.erase(it);
        ++m_Stats.Evicted;
        m_DirtyIndex = true;
    }
    return stored + pendingExtra <= budget;
}

std::size_t FrameCache::Prune() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    const std::size_t before = m_Entries.size();
    EvictUntilFits_(0);
    if (m_DirtyIndex) {
        WriteIndexAtomic_();
    }
    return before - m_Entries.size();
}

bool FrameCache::Compact() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (!m_Open) {
        return false;
    }
    // Drop the mmap so we can rewrite the underlying file.
    m_BlobMmap.Close();
    m_MmapValidSize = 0;

    std::filesystem::path tmp = m_BlobPath;
    tmp += ".compacting";
    std::error_code ec;
    std::filesystem::remove(tmp, ec);

    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }
    std::uint64_t cursor = 0;
    // Two-pass to keep deltas pointing at their (relocated) base entries.
    std::vector<std::uint64_t> ordered;
    ordered.reserve(m_Entries.size());
    for (const auto& kv : m_Entries) {
        if (kv.second.IsDelta == 0) {
            ordered.push_back(kv.first);
        }
    }
    for (const auto& kv : m_Entries) {
        if (kv.second.IsDelta != 0) {
            ordered.push_back(kv.first);
        }
    }
    for (auto k : ordered) {
        auto it = m_Entries.find(k);
        if (it == m_Entries.end()) {
            continue;
        }
        std::vector<std::byte> compressed;
        if (!ReadCompressedBytes_(it->second, compressed)) {
            // Source bytes unreadable — drop the entry.
            m_Entries.erase(it);
            m_DirtyIndex = true;
            continue;
        }
        out.write(reinterpret_cast<const char*>(compressed.data()),
            static_cast<std::streamsize>(compressed.size()));
        if (!out.good()) {
            return false;
        }
        it->second.BlobOffset = cursor;
        cursor += compressed.size();
    }
    out.close();

    // Atomic rename: replace the live blob.
    std::filesystem::remove(m_BlobPath, ec);
    std::filesystem::rename(tmp, m_BlobPath, ec);
    if (ec) {
        std::filesystem::remove(tmp, ec);
        return false;
    }
    m_NextBlobOffset = cursor;
    m_DirtyIndex = true;
    if (!WriteIndexAtomic_()) {
        return false;
    }
    if (m_Options.UseMmap) {
        RemapBlob_();
    }
    return true;
}

bool FrameCache::Flush() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (!m_Open) {
        return false;
    }
    if (!m_DirtyIndex) {
        return true;
    }
    return WriteIndexAtomic_();
}

FrameCacheStats FrameCache::Stats() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    FrameCacheStats out = m_Stats;
    std::uint64_t live = 0;
    for (const auto& kv : m_Entries) {
        live += kv.second.CompressedSize;
    }
    out.BytesStored = live;
    return out;
}

void FrameCache::ResetStats() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Stats = FrameCacheStats{};
}

std::size_t FrameCache::EntryCount() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_Entries.size();
}

bool FrameCache::Contains(std::uint64_t keyHash) const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    auto it = m_Entries.find(keyHash);
    if (it == m_Entries.end()) {
        return false;
    }
    if (it->second.IsDelta == 0) {
        return true;
    }
    return m_Entries.find(it->second.BaseKeyHash) != m_Entries.end();
}

std::uint64_t FrameCache::NowEpochS_() noexcept {
    using clock = std::chrono::system_clock;
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(clock::now().time_since_epoch()).count());
}

} // namespace Solstice::MovieMaker::Cache
