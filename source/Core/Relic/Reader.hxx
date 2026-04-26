#pragma once

#include "Solstice.hxx"
#include "Types.hxx"
#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <cstddef>

namespace Solstice::Core::Relic {

// In-memory parsed RELIC container (header + manifest + dependency table blob).
// Data blob is not loaded here; use Mmap for blob access.
struct RelicContainer {
    RelicFileHeader Header;
    std::vector<RelicManifestEntry> Manifest;
    std::vector<std::byte> DependencyTableBlob;  // raw bytes; DependencyListOffset is byte offset into this
    std::vector<std::byte> PathTableBlob;
    std::filesystem::path FilePath;
};

// Parse RELIC container: read header, manifest, and dependency table (sequential reads).
// Returns nullopt if file invalid or missing.
SOLSTICE_API std::optional<RelicContainer> OpenRelic(const std::filesystem::path& path);

// Get dependency list for a manifest entry. DependencyListOffset is byte offset into DependencyTableBlob;
// layout at offset: uint32_t count, then count * uint64_t hashes.
SOLSTICE_API void GetDependencies(const RelicContainer& container, const RelicManifestEntry& entry,
    std::vector<AssetHash>& outDeps);

} // namespace Solstice::Core::Relic
