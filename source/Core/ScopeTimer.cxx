#include <Core/ScopeTimer.hxx>

namespace Solstice::Core {

ScopeTimer::ScopeTimer(const char* name) : m_name(name) {
    Profiler::Instance().BeginScope(m_name);
}

ScopeTimer::~ScopeTimer() {
    Profiler::Instance().EndScope(m_name);
}

} // namespace Solstice::Core

