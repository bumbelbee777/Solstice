#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace Solstice::FileWatch {

/**
 * Polling-based file change detection (main-thread friendly; no inotify/FSEvents).
 * Debounces rapid writes so a single logical save triggers one callback.
 */
class FileWatcher {
public:
    using ChangeCallback = std::function<void(const std::string& pathUtf8)>;

    explicit FileWatcher(std::chrono::milliseconds debounce = std::chrono::milliseconds(400));

    void SetCallback(ChangeCallback cb);

    /** Watch a file path (UTF-8). Duplicate adds are ignored. */
    void AddPath(const std::string& pathUtf8);
    void RemovePath(const std::string& pathUtf8);
    void ClearPaths();

    /**
     * After the app writes the watched file, call this so Poll() does not treat that write as an external change.
     * Updates the stored modification time from disk and clears any pending debounced notification.
     */
    void ResyncPath(const std::string& pathUtf8);

    /**
     * Call once per frame (or periodically). Uses wall clock for debounce timing.
     */
    void Poll();

    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

private:
    struct Entry {
        std::filesystem::path Path;
        std::filesystem::file_time_type LastWrite{};
        bool HasLastWrite{false};
        bool PendingNotify{false};
        std::chrono::steady_clock::time_point PendingSince{};
    };

    ChangeCallback m_Callback;
    std::vector<Entry> m_Entries;
    std::chrono::milliseconds m_Debounce;
    bool m_Enabled{true};
};

} // namespace Solstice::FileWatch
