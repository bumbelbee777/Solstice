#include "Debug.hxx"

#include <iostream>
#include <mutex>

namespace Solstice::Core {

namespace {
std::mutex g_debugLogMutex;
}

void DebugLogger::Initialize() {}

void DebugLogger::LogMessage(const std::string& message) {
    std::lock_guard<std::mutex> lock(g_debugLogMutex);
    std::cerr << message << '\n';
}

} // namespace Solstice::Core
