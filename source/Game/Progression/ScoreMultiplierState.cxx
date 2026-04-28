#include "Progression/ScoreMultiplierState.hxx"

#include <algorithm>
#include <cmath>

namespace Solstice::Game {

void ScoreMultiplierState::Clear() {
    m_Base = 1.0f;
    m_Session = 1.0f;
}

void ScoreMultiplierState::SetBase(float base) {
    m_Base = (std::max)(base, 1e-3f);
}

void ScoreMultiplierState::SetSession(float session) {
    m_Session = (std::max)(session, 1e-3f);
}

} // namespace Solstice::Game
