#include "SharponEmbeddedShell.hxx"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <cwchar>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <processthreadsapi.h>
#include <consoleapi.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#include <util.h>
#else
#include <pty.h>
#endif
#endif

namespace {

static void AppendTrimAnsi(std::string& out, const char* p, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        if (p[i] == '\x1b' && i + 1 < n) {
            if (p[i + 1] == '[') {
                i += 2;
                while (i < n) {
                    const unsigned char c = static_cast<unsigned char>(p[i]);
                    if (c >= 0x40 && c <= 0x7E) {
                        break;
                    }
                    ++i;
                }
                continue;
            }
            if (p[i + 1] == ']') {
                i += 2;
                while (i < n && p[i] != '\a' && p[i] != '\x1b') {
                    ++i;
                }
                continue;
            }
        }
        if (p[i] == '\r' && i + 1 < n && p[i + 1] == '\n') {
            out += '\n';
            ++i;
        } else if (p[i] == '\r') {
            out += '\n';
        } else {
            out += p[i];
        }
    }
}

} // namespace

#ifdef _WIN32
static void WinCloseH(void** hp) {
    if (hp && *hp) {
        CloseHandle(static_cast<HANDLE>(*hp));
        *hp = nullptr;
    }
}
#endif

SharponEmbeddedShell::SharponEmbeddedShell(SharponEmbeddedShell&& o) noexcept
    : displayBuffer(std::move(o.displayBuffer))
    , m_startError(std::move(o.m_startError))
    , m_active(o.m_active)
    , m_started(o.m_started)
    , m_lastCols(o.m_lastCols)
    , m_lastRows(o.m_lastRows) {
    o.m_active = false;
    o.m_started = false;
    o.m_lastCols = 0;
    o.m_lastRows = 0;
#ifdef _WIN32
    m_inWrite = o.m_inWrite;
    m_outRead = o.m_outRead;
    m_hProcess = o.m_hProcess;
    m_pCon = o.m_pCon;
    o.m_inWrite = o.m_outRead = o.m_hProcess = o.m_pCon = nullptr;
#else
    m_masterFd = o.m_masterFd;
    m_childPid = o.m_childPid;
    o.m_masterFd = -1;
    o.m_childPid = -1;
#endif
}

SharponEmbeddedShell& SharponEmbeddedShell::operator=(SharponEmbeddedShell&& o) noexcept {
    if (this == &o) {
        return *this;
    }
    shutdown();
    displayBuffer = std::move(o.displayBuffer);
    m_startError = std::move(o.m_startError);
    m_active = o.m_active;
    m_started = o.m_started;
    m_lastCols = o.m_lastCols;
    m_lastRows = o.m_lastRows;
    o.m_active = false;
    o.m_started = false;
    o.m_lastCols = 0;
    o.m_lastRows = 0;
#ifdef _WIN32
    m_inWrite = o.m_inWrite;
    m_outRead = o.m_outRead;
    m_hProcess = o.m_hProcess;
    m_pCon = o.m_pCon;
    o.m_inWrite = o.m_outRead = o.m_hProcess = o.m_pCon = nullptr;
#else
    m_masterFd = o.m_masterFd;
    m_childPid = o.m_childPid;
    o.m_masterFd = -1;
    o.m_childPid = -1;
#endif
    return *this;
}

#ifdef _WIN32
void SharponEmbeddedShell::closeWinHandles() {
    WinCloseH(&m_inWrite);
    WinCloseH(&m_outRead);
    if (m_pCon) {
        ClosePseudoConsole(static_cast<HPCON>(m_pCon));
        m_pCon = nullptr;
    }
    if (m_hProcess) {
        WinCloseH(&m_hProcess);
    }
}
#endif

void SharponEmbeddedShell::trimIfNeeded() {
    if (displayBuffer.size() > kMaxOut) {
        const size_t drop = displayBuffer.size() - kMaxOut + 1;
        displayBuffer.erase(0, drop);
        displayBuffer.insert(0, "…(truncated)…\n");
    }
}

void SharponEmbeddedShell::appendAndSanitize(const char* data, size_t n) {
    if (!n) {
        return;
    }
    const size_t before = displayBuffer.size();
    AppendTrimAnsi(displayBuffer, data, n);
    trimIfNeeded();
    (void)before; // if we need delta scroll later
}

void SharponEmbeddedShell::ensureStarted(const std::filesystem::path& workingDirectory) {
    if (m_started) {
        return;
    }
    startImpl(workingDirectory);
    m_started = true;
}

void SharponEmbeddedShell::startImpl(const std::filesystem::path& cwd) {
    m_startError.clear();
#ifdef _WIN32
    std::error_code ec;
    std::filesystem::path work = cwd;
    if (work.empty() || !std::filesystem::is_directory(work, ec)) {
        work = std::filesystem::current_path(ec);
    }
    const std::wstring workW = work.wstring();

    const COORD conSize{80, 25};
    HANDLE inPipeRead = nullptr;
    HANDLE inPipeWrite = nullptr;
    HANDLE outPipeRead = nullptr;
    HANDLE outPipeWrite = nullptr;
    if (!CreatePipe(&inPipeRead, &inPipeWrite, nullptr, 0) || !CreatePipe(&outPipeRead, &outPipeWrite, nullptr, 0)) {
        m_startError = "CreatePipe failed.";
        return;
    }
    HPCON hPCon = nullptr;
    if (CreatePseudoConsole(conSize, inPipeRead, outPipeWrite, 0, &hPCon) != S_OK) {
        CloseHandle(inPipeRead);
        CloseHandle(inPipeWrite);
        CloseHandle(outPipeRead);
        CloseHandle(outPipeWrite);
        m_startError = "CreatePseudoConsole failed (requires Windows 10 1809+).";
        return;
    }
    CloseHandle(inPipeRead);
    CloseHandle(outPipeWrite);
    m_inWrite = inPipeWrite;
    m_outRead = outPipeRead;
    m_pCon = hPCon;

    auto freeAttributeList = [](LPPROC_THREAD_ATTRIBUTE_LIST p) {
        if (!p) {
            return;
        }
        DeleteProcThreadAttributeList(p);
        HeapFree(GetProcessHeap(), 0, p);
    };

    LPPROC_THREAD_ATTRIBUTE_LIST pAttr = nullptr;
    SIZE_T bytesRequired = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &bytesRequired);
    pAttr = static_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(HeapAlloc(GetProcessHeap(), 0, bytesRequired));
    if (!pAttr) {
        m_startError = "HeapAlloc failed for process attributes.";
        closeWinHandles();
        return;
    }
    if (!InitializeProcThreadAttributeList(pAttr, 1, 0, &bytesRequired)
        || !UpdateProcThreadAttribute(
                pAttr,
                0,
                PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                &hPCon,
                sizeof(hPCon),
                nullptr,
                nullptr)) {
        m_startError = "Proc thread attribute list setup failed.";
        freeAttributeList(pAttr);
        closeWinHandles();
        return;
    }

    wchar_t comspecBuf[2048] = {0};
    const DWORD nCom = GetEnvironmentVariableW(
        L"COMSPEC", comspecBuf, static_cast<DWORD>(sizeof(comspecBuf) / sizeof(comspecBuf[0])));
    if (nCom == 0 || nCom >= sizeof(comspecBuf) / sizeof(comspecBuf[0]) - 1) {
        wcscpy_s(comspecBuf, 2048, L"C:\\Windows\\System32\\cmd.exe");
    }

    const size_t cmdLen = wcslen(comspecBuf) + 1;
    std::vector<wchar_t> cmdLineBuf(cmdLen);
    wmemcpy(cmdLineBuf.data(), comspecBuf, cmdLen);

    STARTUPINFOEXW si{};
    si.StartupInfo.cb = sizeof(STARTUPINFOEXW);
    si.StartupInfo.dwFlags = 0;
    si.lpAttributeList = pAttr;

    PROCESS_INFORMATION pi{};
    const DWORD flags = EXTENDED_STARTUPINFO_PRESENT;
    const wchar_t* curDirW = workW.empty() ? nullptr : workW.c_str();
    if (!CreateProcessW(
            nullptr, cmdLineBuf.data(), nullptr, nullptr, FALSE, flags, nullptr, curDirW, &si.StartupInfo, &pi)) {
        m_startError = "CreateProcessW failed to start system shell (COMSPEC).";
        freeAttributeList(pAttr);
        closeWinHandles();
        return;
    }
    freeAttributeList(pAttr);
    CloseHandle(pi.hThread);

    m_hProcess = pi.hProcess;
    m_active = true;
    m_lastCols = 80;
    m_lastRows = 25;
#else
    std::error_code ec;
    std::filesystem::path work = cwd;
    if (work.empty() || !std::filesystem::is_directory(work, ec)) {
        work = std::filesystem::current_path(ec);
    }
    if (m_masterFd >= 0) {
        return;
    }
    struct winsize ws{};
    ws.ws_col = 80;
    ws.ws_row = 25;
    int master = -1, slave = -1;
    if (openpty(&master, &slave, nullptr, nullptr, &ws) < 0) {
        m_startError = "openpty failed.";
        return;
    }
    fcntl(master, F_SETFL, fcntl(master, F_GETFL, 0) | O_NONBLOCK);
    m_masterFd = master;
    m_childPid = fork();
    if (m_childPid < 0) {
        m_startError = "fork() failed.";
        close(slave);
        close(master);
        m_masterFd = -1;
        m_childPid = -1;
        return;
    }
    if (m_childPid == 0) {
        close(master);
        if (setsid() < 0) {
            _exit(1);
        }
#if defined(TIOCSCTTY)
        if (ioctl(slave, TIOCSCTTY, 0) < 0) {
        }
#endif
        dup2(slave, STDIN_FILENO);
        dup2(slave, STDOUT_FILENO);
        dup2(slave, STDERR_FILENO);
        if (slave > 2) {
            close(slave);
        }
        if (!work.empty()) {
            std::string wd = work.string();
            (void)chdir(wd.c_str());
        }
        const char* sh = std::getenv("SHELL");
        if (!sh || !*sh) {
            sh = "/bin/sh";
        }
        setenv("TERM", "xterm-256color", 1);
        const char* base = strrchr(sh, '/');
        base = base ? base + 1 : sh;
        execlp(sh, base, (char*)NULL);
        _exit(127);
    }
    close(slave);
    m_active = true;
    m_lastCols = 80;
    m_lastRows = 25;
#endif
}

void SharponEmbeddedShell::shutdown() {
#ifdef _WIN32
    if (m_hProcess) {
        TerminateProcess(static_cast<HANDLE>(m_hProcess), 1);
    }
    closeWinHandles();
#else
    if (m_childPid > 0) {
        if (m_masterFd >= 0) {
            (void)kill(m_childPid, SIGTERM);
        }
        int st = 0;
        (void)waitpid(m_childPid, &st, 0);
        m_childPid = -1;
    }
    if (m_masterFd >= 0) {
        close(m_masterFd);
        m_masterFd = -1;
    }
#endif
    m_active = false;
    m_started = false;
    m_lastCols = 0;
    m_lastRows = 0;
}

void SharponEmbeddedShell::setTerminalSize(int columns, int rows) {
    columns = std::max(2, columns);
    rows = std::max(1, rows);
    if (columns == m_lastCols && rows == m_lastRows) {
        return;
    }
    m_lastCols = columns;
    m_lastRows = rows;
#ifdef _WIN32
    if (m_pCon) {
        COORD c{};
        c.X = static_cast<SHORT>(columns);
        c.Y = static_cast<SHORT>(rows);
        (void)ResizePseudoConsole(static_cast<HPCON>(m_pCon), c);
    }
#else
    if (m_masterFd >= 0) {
        struct winsize ws{};
        ws.ws_col = static_cast<unsigned short>(columns);
        ws.ws_row = static_cast<unsigned short>(rows);
        (void)ioctl(m_masterFd, TIOCSWINSZ, &ws);
    }
#endif
}

void SharponEmbeddedShell::pump() {
#ifdef _WIN32
    if (!m_hProcess) {
        return;
    }
    if (WaitForSingleObject(static_cast<HANDLE>(m_hProcess), 0) == WAIT_OBJECT_0) {
        displayBuffer += "\n[Shell process exited.]\n";
        closeWinHandles();
        m_active = false;
        return;
    }
    if (!m_outRead) {
        return;
    }
    char buf[4096];
    for (;;) {
        DWORD avail = 0;
        if (!PeekNamedPipe(static_cast<HANDLE>(m_outRead), nullptr, 0, nullptr, &avail, nullptr) || !avail) {
            break;
        }
        DWORD rd = 0;
        if (!ReadFile(
                static_cast<HANDLE>(m_outRead), buf, static_cast<DWORD>(std::min(sizeof(buf), (size_t)avail)), &rd, nullptr)
            || rd == 0) {
            break;
        }
        appendAndSanitize(buf, rd);
    }
#else
    if (m_masterFd < 0) {
        return;
    }
    if (m_childPid > 0) {
        int wst = 0;
        const pid_t r = waitpid(m_childPid, &wst, WNOHANG);
        if (r == m_childPid) {
            displayBuffer += "\n[Shell process exited.]\n";
            m_childPid = -1;
            close(m_masterFd);
            m_masterFd = -1;
            m_active = false;
            return;
        }
    }
    char buf[4096];
    for (;;) {
        ssize_t n = read(m_masterFd, buf, sizeof(buf));
        if (n > 0) {
            appendAndSanitize(buf, static_cast<size_t>(n));
        } else if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            break;
        } else {
            break;
        }
    }
#endif
}

void SharponEmbeddedShell::writeLine(std::string_view line) {
    if (line.empty()) {
        return;
    }
#ifdef _WIN32
    if (!m_inWrite) {
        return;
    }
    std::string s(line);
    s += "\r\n";
    DWORD wr = 0;
    (void)WriteFile(static_cast<HANDLE>(m_inWrite), s.data(), static_cast<DWORD>(s.size()), &wr, nullptr);
#else
    if (m_masterFd < 0) {
        return;
    }
    std::string s(line);
    s += '\n';
    (void)write(m_masterFd, s.data(), s.size());
#endif
}
