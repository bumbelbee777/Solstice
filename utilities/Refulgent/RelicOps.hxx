#pragma once

#include <Core/Relic/Reader.hxx>
#include <Core/Relic/Types.hxx>
#include <Core/Relic/Writer.hxx>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Refulgent::RelicOps {

using Solstice::Core::Relic::AssetTypeTag;
using Solstice::Core::Relic::BootstrapEntry;
using Solstice::Core::Relic::CompressionType;
using Solstice::Core::Relic::RelicContainer;
using Solstice::Core::Relic::RelicFileHeader;
using Solstice::Core::Relic::RelicWriteInput;
using Solstice::Core::Relic::RelicWriteOptions;

// Same FNV-1a 64 as Parallax DevSession/Relic resolvers (UTF-8 bytes).
uint64_t HashPathFNV1a(std::string_view path);
uint64_t HashBytesFNV1a(std::span<const std::byte> data);

const char* CompressionName(Solstice::Core::Relic::CompressionType c);
const char* AssetTypeShortName(Solstice::Core::Relic::AssetTypeTag t);

AssetTypeTag AssetTypeFromExtension(const std::filesystem::path& filePath);

// Normalize relative path: forward slashes, trim leading "./".
std::string NormalizeLogicalPath(std::string_view rel);

struct LoadError {
    std::string Message;
};

/// Full manifest + blob decode for rewrite. Fails on delta-tagged entries (WriteRelic cannot emit them).
std::optional<std::vector<RelicWriteInput>> LoadEntriesForRewrite(
    const std::filesystem::path& relicPath, LoadError* errOut = nullptr);

RelicWriteOptions OptionsFromHeader(const RelicFileHeader& hdr);

struct PackDirOptions {
    enum class HashMode { Path, Content };
    HashMode Hash = HashMode::Path;
    CompressionType Compression = CompressionType::Zstd;
    /// When true, only regular files (skip symlinks for portability).
    bool SkipSymlinks = true;
};

/// Recursively pack files under `root` into a new RELIC. LogicalPath is relative to `root` (POSIX-style).
std::optional<std::vector<RelicWriteInput>> BuildInputsFromDirectory(const std::filesystem::path& root,
    const PackDirOptions& opt, std::string* errOut = nullptr);

bool WriteRelicFile(const std::filesystem::path& outPath, std::vector<RelicWriteInput> inputs, const RelicWriteOptions& opt,
    std::string* errOut = nullptr);

/// Remove all manifest rows whose hash is in `remove`.
std::optional<std::vector<RelicWriteInput>> RemoveHashes(const std::filesystem::path& relicPath,
    const std::vector<uint64_t>& remove, std::string* errOut = nullptr);

/// Append new files; fails if any new hash already exists in the container.
std::optional<std::vector<RelicWriteInput>> AddFilesToContainer(const std::filesystem::path& relicPath,
    const std::vector<std::pair<std::filesystem::path, std::string>>& fileAndLogical, CompressionType compression,
    std::string* errOut = nullptr);

/// One asset bytes, decompressed.
std::optional<std::vector<std::byte>> ExtractUncompressed(
    const std::filesystem::path& relicPath, uint64_t assetHash, std::string* errOut = nullptr);

/// Later files win on duplicate asset hash (last in `paths` kept).
bool MergeRelics(
    const std::filesystem::path& outPath, const std::vector<std::filesystem::path>& paths, std::string* errOut = nullptr);

struct RelicArchiveStats {
    uint64_t FileSizeBytes = 0;
    size_t EntryCount = 0;
    uint64_t TotalCompressedPayload = 0;
    uint64_t TotalUncompressed = 0;
    bool HasPathTable = false;
    uint64_t PathTableBytes = 0;
    uint64_t DependencyTableBytes = 0;
};

/// Aggregate sizes from header + manifest (file size from disk).
std::optional<RelicArchiveStats> GetArchiveStats(const std::filesystem::path& relicPath, std::string* errOut = nullptr);

const char* AssetTypeLongName(Solstice::Core::Relic::AssetTypeTag t);

/// Read and decompress every non-delta entry. Delta entries are skipped (counted in `skippedDelta`); I/O errors fail.
bool VerifyRelic(const std::filesystem::path& relicPath, std::string* firstError, size_t& okCount, size_t& failCount,
    size_t& skippedDelta, bool verbose = false);

} // namespace Refulgent::RelicOps
