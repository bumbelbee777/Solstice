#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>

// Hosts the OS default command shell inside the editor (ConPTY on Windows, PTY on POSIX).

class SharponEmbeddedShell {
  public:
    SharponEmbeddedShell() = default;
    ~SharponEmbeddedShell() { shutdown(); }
    SharponEmbeddedShell(SharponEmbeddedShell&& o) noexcept;
    SharponEmbeddedShell& operator=(SharponEmbeddedShell&& o) noexcept;
    SharponEmbeddedShell(const SharponEmbeddedShell&) = delete;
    SharponEmbeddedShell& operator=(const SharponEmbeddedShell&) = delete;

    void ensureStarted(const std::filesystem::path& workingDirectory);
    void shutdown();
    void pump();
    void setTerminalSize(int columns, int rows);
    void writeLine(std::string_view line);

    const std::string& viewText() const { return displayBuffer; }
    std::string_view startError() const { return m_startError; }
    bool isRunning() const { return m_active; }

  private:
    void startImpl(const std::filesystem::path& cwd);
    void appendAndSanitize(const char* data, size_t n);
    void trimIfNeeded();
#ifdef _WIN32
    void closeWinHandles();
#endif

    static constexpr size_t kMaxOut = 500000;
#ifdef _WIN32
    void* m_inWrite = nullptr; // we write (user to shell)
    void* m_outRead = nullptr; // we read (shell to user)
    void* m_hProcess = nullptr;
    void* m_pCon = nullptr; // HPCON
#else
    int m_masterFd = -1;
    int m_childPid = -1;
#endif
    std::string displayBuffer;
    std::string m_startError;
    bool m_active = false;
    bool m_started = false;
    int m_lastCols = 0;
    int m_lastRows = 0;
};
