#pragma once

#include "../../Solstice.hxx"

namespace Solstice::Game {

/// Authoritative **score multiplier** stack for match / session rules (streak, difficulty, events).
class SOLSTICE_API ScoreMultiplierState {
public:
    void Clear();

    void SetBase(float base);
    void SetSession(float session);

    /// Effective multiplier in `(0, +inf)`; at least a small epsilon.
    [[nodiscard]] float GetEffective() const { return m_Base * m_Session; }

    float GetBase() const { return m_Base; }
    float GetSession() const { return m_Session; }

private:
    float m_Base{1.0f};
    float m_Session{1.0f};
};

} // namespace Solstice::Game
