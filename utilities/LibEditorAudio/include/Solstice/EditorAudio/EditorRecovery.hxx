#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Solstice::EditorAudio::FileRecovery {

/// `basePath/.solstice_recovery/<appSlug>/` (e.g. basePath = SDL_GetBasePath()).
[[nodiscard]] std::filesystem::path RecoveryDir(
    const char* sdlBasePathUtf8OrNull, std::string_view appSlug);

struct Entry {
    std::filesystem::path path;
    std::uint64_t unixTimeUtc = 0;
    std::uint64_t payloadBytes = 0;
};

/// Timestamped `prefix_<utc>_<threadtag>.srec` — creates parent dirs as needed.
bool WriteSnapshot(
    const std::filesystem::path& recoveryDir, std::string_view fileNamePrefix, std::span<const std::byte> payload, std::string* outError);

[[nodiscard]] std::vector<Entry> List(const std::filesystem::path& recoveryDir, std::string_view fileNamePrefix);

bool ReadFile(const std::filesystem::path& file, std::vector<std::byte>& out, std::string* outError);
bool ReadLatest(
    const std::filesystem::path& recoveryDir, std::string_view fileNamePrefix, std::vector<std::byte>& out, std::string* outError);

void ClearMatchingPrefix(const std::filesystem::path& recoveryDir, std::string_view fileNamePrefix);

} // namespace Solstice::EditorAudio::FileRecovery
