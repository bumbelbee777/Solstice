#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace Solstice::SettingsStore {

/**
 * `solstice_settings_<appSlug>.json` next to the executable.
 * Pass `SDL_GetBasePath()` from the app (SDL3: cached pointer — do not free); if null, uses the filename alone (cwd).
 */
std::filesystem::path PathNextToExecutable(const char* sdlBasePathUtf8OrNull, std::string_view appSlug);

/**
 * Small persistent key/value store for authoring tools (JSON object, string values only on disk).
 * Booleans and integers are encoded as strings ("true"/"false", decimal text).
 */
class Store {
public:
    explicit Store(std::filesystem::path path);

    const std::filesystem::path& Path() const { return m_Path; }

    bool Load(std::string* outError = nullptr);
    bool Save(std::string* outError = nullptr);

    void Clear();

    std::optional<std::string> GetString(std::string_view key) const;
    void SetString(std::string_view key, std::string value);

    std::optional<bool> GetBool(std::string_view key) const;
    void SetBool(std::string_view key, bool value);

    std::optional<std::int64_t> GetInt64(std::string_view key) const;
    void SetInt64(std::string_view key, std::int64_t value);

private:
    std::filesystem::path m_Path;
    std::unordered_map<std::string, std::string> m_Values;
};

// Well-known keys (optional; apps may use custom strings)
inline constexpr std::string_view kKeyFormatVersion = "formatVersion";
inline constexpr std::string_view kKeyFileBrowserRoot = "fileBrowserRoot";
inline constexpr std::string_view kKeyWatchActiveScriptFile = "watchActiveScriptFile";
inline constexpr std::string_view kKeyWatchMapFile = "watchMapFile";
inline constexpr std::string_view kKeyCompressSmf = "compressSmf";
inline constexpr std::string_view kKeyWatchProjectAndPrlx = "watchProjectAndPrlx";

} // namespace Solstice::SettingsStore
