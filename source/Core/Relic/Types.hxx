#pragma once

#include "Solstice.hxx"
#include <cstdint>
#include <cstddef>
#include <string>

namespace Solstice::Core::Relic {

// RELIC container magic and format version
constexpr uint32_t RELIC_MAGIC = 0x494C4552u; // "RELI" in little-endian
constexpr uint16_t RELIC_FORMAT_VERSION = 1;

// Bootstrap (game.data.relic) magic
constexpr uint32_t RELIC_BOOTSTRAP_MAGIC = 0x544F4F42u; // "BOOT"

// Container type
enum class ContainerType : uint8_t {
    Content = 0,
    Music = 1
};

// Streaming hint for bootstrap entries
enum class StreamingHint : uint8_t {
    Preload = 0,
    Stream = 1,
    Lazy = 2
};

// Tag set bits (base, DLC, mod, locale)
enum RelicTag : uint16_t {
    TagBase = 1u << 0,
    TagDLC = 1u << 1,
    TagMod = 1u << 2,
    TagLocale = 1u << 3
};

// Asset type tag (mesh, texture, material, audio, script, lightmap, etc.)
enum class AssetTypeTag : uint16_t {
    Unknown = 0,
    Mesh = 1,
    Texture = 2,
    Material = 3,
    Audio = 4,
    Script = 5,
    Lightmap = 6,
    ParallaxScene = 7,
    Count
};

// Compression type (per-entry flags)
enum class CompressionType : uint8_t {
    None = 0,
    LZ4 = 1,
    Zstd = 2
};

// Manifest entry flags
enum RelicEntryFlags : uint16_t {
    FlagNone = 0,
    FlagIsDelta = 1u << 0,
    FlagIsBase = 1u << 1,
    FlagCompressionMask = 0x00FCu,  // bits 2-7 for compression type
    FlagStreamingPriorityShift = 8,
    FlagStreamingPriorityMask = 0xFF00u
};

inline CompressionType GetCompressionType(uint16_t flags) {
    return static_cast<CompressionType>((flags & FlagCompressionMask) >> 2);
}

inline uint8_t GetStreamingPriority(uint16_t flags) {
    return static_cast<uint8_t>((flags & FlagStreamingPriorityMask) >> FlagStreamingPriorityShift);
}

inline uint16_t WithCompression(CompressionType c, uint16_t baseFlags = FlagNone) {
    return static_cast<uint16_t>((baseFlags & ~FlagCompressionMask) |
        (static_cast<uint16_t>(c) << 2));
}

// 64-bit asset hash (primary key)
using AssetHash = uint64_t;

// RELIC container file header (fixed size, at start of file)
#pragma pack(push, 1)
struct RelicFileHeader {
    uint32_t Magic;
    uint16_t FormatVersion;
    uint8_t ContainerType;  // ContainerType
    uint8_t Reserved;
    uint16_t TagSet;        // RelicTag bits
    uint16_t Reserved2;
    uint64_t ManifestOffset;
    uint64_t ManifestSize;
    uint64_t DependencyTableOffset;
    uint64_t DependencyTableSize;
    uint64_t DataBlobOffset;
    uint64_t PathTableOffset; // 0 if absent (or legacy header without these fields)
    uint64_t PathTableSize;
};
#pragma pack(pop)

// Manifest entry: <= 48 bytes, cache-friendly
#pragma pack(push, 1)
struct RelicManifestEntry {
    uint64_t AssetHash;
    uint64_t DataOffset;      // byte offset into data blob
    uint32_t CompressedSize;
    uint32_t UncompressedSize;
    uint16_t AssetTypeTag;    // AssetTypeTag
    uint16_t Flags;           // RelicEntryFlags, compression type, streaming priority
    uint32_t DependencyListOffset;  // offset into dependency table
    uint32_t ClusterId;
};
#pragma pack(pop)

static_assert(sizeof(RelicManifestEntry) <= 48, "Manifest entry must be <= 48 bytes");
static_assert(sizeof(RelicFileHeader) == 68, "RELIC v1 extended header size");

// Bootstrap entry: one RELIC file to load
struct BootstrapEntry {
    std::string Path;           // path or filename relative to base
    uint32_t Priority;         // higher = override earlier
    StreamingHint Hint;
    uint16_t TagSet;
};

} // namespace Solstice::Core::Relic
