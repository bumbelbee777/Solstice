// Debug.cxx
#include "Core/Debug.hxx"
#include "Async.hxx"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace Solstice::Core {

static std::ofstream LogFile;
static Spinlock LogMutex;
static std::atomic<bool> IsInitializing = false;
static bool IsInitialized = false;

// Add the missing function
static std::string GetCurrentTimeString() {
    try {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::ostringstream ss;
        ss << std::put_time(std::localtime(&time), "%H:%M:%S")
           << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return ss.str();
    } catch (...) {
        return "??:??:??.???";
    }
}

void DebugLogger::Initialize() {
    bool expected = false;
    if (!IsInitializing.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return;
    }

    SIMPLE_LOG("Initializing logging system...");

    try {
        bool logFileOpened = false;
        bool logFileWarning = false;

        {
            LockGuard lock(LogMutex);
            if (IsInitialized) {
                IsInitializing.store(false, std::memory_order_release);
                return;
            }

            // Try to create logs directory
            std::filesystem::path logPath("logs");
            if (!std::filesystem::exists(logPath)) {
                std::filesystem::create_directories(logPath);
            }

            // Try to open log file
            LogFile.open("logs/solstice.log", std::ios::out | std::ios::trunc);
            if (!LogFile.is_open()) {
                logFileWarning = true;
            } else {
                logFileOpened = true;
                LogFile << "=== Log started ===\n";
                // LogFile.flush(); // Let OS handle buffering
            }

            IsInitialized = true;
        }

        if (logFileWarning) {
            SIMPLE_LOG("Warning: Could not open log file, logging to console only");
        }
        if (logFileOpened) {
            SIMPLE_LOG("Log file opened: logs/solstice.log");
        }
        SIMPLE_LOG("Logging system initialized successfully");
        IsInitializing.store(false, std::memory_order_release);
        
    } catch (const std::exception& e) {
        IsInitializing.store(false, std::memory_order_release);
        std::cerr << "CRITICAL: Failed to initialize logging: " << e.what() << std::endl;
        throw;
    }
}

void DebugLogger::Write(const std::string& Message) {
    if (!IsInitialized && !IsInitializing.load(std::memory_order_acquire)) {
        Initialize();
    }
    std::string formatted = "[" + GetCurrentTimeString() + "] " + Message;
    
    // Always write to console
    std::cout << formatted << "\n";
    
    // Write to file if available
    if (IsInitialized && LogFile.is_open()) {
        LockGuard lock(LogMutex);
        LogFile << formatted << "\n";
        // LogFile.flush(); // Removed for performance
    }
}

void DebugLogger::Write(const char* Message) {
    Write(std::string(Message));
}

void InitializeLogging() {
    DebugLogger::Initialize();
}

} // namespace Solstice::Core