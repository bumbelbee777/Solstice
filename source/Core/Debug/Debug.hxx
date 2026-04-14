#pragma once

#include <sstream>
#include <string>

namespace Solstice::Core {

class DebugLogger {
public:
    static void Initialize();
    static void LogMessage(const std::string& message);
};

} // namespace Solstice::Core

#define SIMPLE_LOG(msg)                                                                                \
    do {                                                                                               \
        std::ostringstream _solstice_simple_log_ss_;                                                   \
        _solstice_simple_log_ss_ << (msg);                                                             \
        ::Solstice::Core::DebugLogger::LogMessage(_solstice_simple_log_ss_.str());                     \
    } while (0)
