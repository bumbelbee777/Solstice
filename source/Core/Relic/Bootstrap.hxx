#pragma once

#include "Solstice.hxx"
#include "Types.hxx"
#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <vector>

namespace Solstice::Core::Relic {

// Parsed bootstrap config from game.data.relic (in-memory only, read once at startup).
struct BootstrapConfig {
    std::vector<BootstrapEntry> Entries;
    bool Valid = false;
};

// Parse game.data.relic from the given path. Returns nullopt if file missing or invalid.
// Bootstrap file format: magic (4), version (2), reserved (2), entry_count (4),
// then per entry: path_len (2), path_utf8[path_len], priority (4), hint (1), tag_set (2).
SOLSTICE_API std::optional<BootstrapConfig> ParseBootstrap(const std::filesystem::path& path);

// Get the default path for game.data.relic (relative to executable/base path).
SOLSTICE_API std::filesystem::path GetDefaultBootstrapPath(const std::filesystem::path& basePath);

// Write bootstrap file (same layout as ParseBootstrap).
SOLSTICE_API bool WriteBootstrap(const std::filesystem::path& path, const std::vector<BootstrapEntry>& entries);

} // namespace Solstice::Core::Relic
