#pragma once

#include <sstream>
#include <string>
#include <utility>

namespace Solstice::Core {

class DebugLogger {
public:
    static void Initialize();
    static void LogMessage(const std::string& message);
};

/** Streams any number of arguments (like std::cout) into the debug log. */
template<typename... Args>
inline void SolsticeLog(Args&&... args) {
    std::ostringstream os;
    (os << ... << std::forward<Args>(args));
    DebugLogger::LogMessage(os.str());
}

} // namespace Solstice::Core

#define SIMPLE_LOG(msg)                                                                                \
    do {                                                                                               \
        std::ostringstream _solstice_simple_log_ss_;                                                   \
        _solstice_simple_log_ss_ << (msg);                                                             \
        ::Solstice::Core::DebugLogger::LogMessage(_solstice_simple_log_ss_.str());                     \
    } while (0)

#define SOLSTICE_LOG(...) ::Solstice::Core::SolsticeLog(__VA_ARGS__)
