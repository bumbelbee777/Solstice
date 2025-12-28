#pragma once

#include "../Solstice.hxx"
#include <Core/Profiler.hxx>

namespace Solstice::Core {

/**
 * ScopeTimer - RAII helper for automatic scope timing
 * Automatically starts timing on construction and stops on destruction
 */
class SOLSTICE_API ScopeTimer {
public:
    explicit ScopeTimer(const char* name);
    ~ScopeTimer();

    // Non-copyable, non-movable
    ScopeTimer(const ScopeTimer&) = delete;
    ScopeTimer& operator=(const ScopeTimer&) = delete;
    ScopeTimer(ScopeTimer&&) = delete;
    ScopeTimer& operator=(ScopeTimer&&) = delete;

private:
    const char* m_name;
};

} // namespace Solstice::Core

// Convenience macro for scope timing
#define PROFILE_SCOPE(name) Solstice::Core::ScopeTimer _profile_scope(name)

