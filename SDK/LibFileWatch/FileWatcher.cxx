#include <Solstice/FileWatch/FileWatcher.hxx>

#include <algorithm>
#include <system_error>

namespace Solstice::FileWatch {

FileWatcher::FileWatcher(std::chrono::milliseconds debounce) : m_Debounce(debounce) {}

void FileWatcher::SetCallback(ChangeCallback cb) {
    m_Callback = std::move(cb);
}

void FileWatcher::AddPath(const std::string& pathUtf8) {
    if (pathUtf8.empty()) {
        return;
    }
    std::filesystem::path p(pathUtf8);
    for (auto& e : m_Entries) {
        if (e.Path == p) {
            return;
        }
    }
    Entry e;
    e.Path = std::move(p);
    std::error_code ec;
    if (std::filesystem::exists(e.Path, ec) && std::filesystem::is_regular_file(e.Path, ec)) {
        e.LastWrite = std::filesystem::last_write_time(e.Path, ec);
        e.HasLastWrite = !ec;
    }
    m_Entries.push_back(std::move(e));
}

void FileWatcher::RemovePath(const std::string& pathUtf8) {
    std::filesystem::path p(pathUtf8);
    m_Entries.erase(
        std::remove_if(m_Entries.begin(), m_Entries.end(), [&](const Entry& e) { return e.Path == p; }),
        m_Entries.end());
}

void FileWatcher::ClearPaths() {
    m_Entries.clear();
}

void FileWatcher::ResyncPath(const std::string& pathUtf8) {
    if (pathUtf8.empty()) {
        return;
    }
    const std::filesystem::path target(pathUtf8);
    for (auto& e : m_Entries) {
        if (e.Path != target) {
            continue;
        }
        e.PendingNotify = false;
        std::error_code ec;
        if (std::filesystem::exists(e.Path, ec) && std::filesystem::is_regular_file(e.Path, ec)) {
            e.LastWrite = std::filesystem::last_write_time(e.Path, ec);
            e.HasLastWrite = !ec;
        }
        return;
    }
}

void FileWatcher::Poll() {
    if (!m_Enabled || !m_Callback) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    for (auto& e : m_Entries) {
        std::error_code ec;
        if (!std::filesystem::exists(e.Path, ec) || !std::filesystem::is_regular_file(e.Path, ec)) {
            continue;
        }
        const auto wt = std::filesystem::last_write_time(e.Path, ec);
        if (ec) {
            continue;
        }
        if (!e.HasLastWrite) {
            e.LastWrite = wt;
            e.HasLastWrite = true;
            continue;
        }
        if (wt != e.LastWrite) {
            e.LastWrite = wt;
            if (!e.PendingNotify) {
                e.PendingNotify = true;
                e.PendingSince = now;
            }
        }
        if (e.PendingNotify) {
            if (now - e.PendingSince >= m_Debounce) {
                e.PendingNotify = false;
                m_Callback(e.Path.string());
            }
        }
    }
}

} // namespace Solstice::FileWatch
