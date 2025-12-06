// Debug.hxx
#pragma once

#include "../SolsticeExport.hxx"
#include <string>
#include <sstream>
#include "Async.hxx"

namespace Solstice::Core {

class SOLSTICE_API DebugLogger {
public:

    static void Initialize();

    static void Write(const std::string& Message);
    static void Write(const char* Message);
    
    template<typename... Args>
    static void Log(Args&&... args) {
        std::ostringstream ss;
        (ss << ... << std::forward<Args>(args));
        Write(ss.str());
    }
};

// Initialize the logging system
void InitializeLogging();

// Helper macro for logging
#define SOLSTICE_LOG(...) Solstice::Core::DebugLogger::Log(__VA_ARGS__)

} // namespace Solstice::Core

#define SIMPLE_LOG(msg) Solstice::Core::DebugLogger::Log(msg)

// Verbose log that can be disabled in Release builds or by definition
#ifdef NDEBUG
    #define VERBOSE_LOG(msg) ((void)0)
#else
    #define VERBOSE_LOG(msg) Solstice::Core::DebugLogger::Log(msg)
#endif