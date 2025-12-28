#include "GameState.hxx"
#include "Core/Debug.hxx"

namespace Solstice::Game {

GameState::GameState() {
    m_CurrentState = GameStateType::MainMenu;
}

void GameState::SetState(GameStateType State) {
    if (m_CurrentState != State) {
        OnStateExit(m_CurrentState);
        m_CurrentState = State;
        OnStateEnter(m_CurrentState);
        SIMPLE_LOG("GameState: Changed to state: " + std::to_string(static_cast<int>(State)));
    }
}

void GameState::RegisterTransition(GameStateType From, GameStateType To, std::function<bool()> Condition) {
    m_Transitions[From][To] = Condition ? Condition : []() { return true; };
}

bool GameState::CanTransition(GameStateType To) const {
    auto fromIt = m_Transitions.find(m_CurrentState);
    if (fromIt != m_Transitions.end()) {
        auto toIt = fromIt->second.find(To);
        if (toIt != fromIt->second.end()) {
            return toIt->second();
        }
    }
    return false;
}

bool GameState::TryTransition(GameStateType To) {
    if (CanTransition(To)) {
        SetState(To);
        return true;
    }
    return false;
}

void GameState::RegisterStateEnterCallback(GameStateType State, StateCallback Callback) {
    m_EnterCallbacks[State].push_back(Callback);
}

void GameState::RegisterStateExitCallback(GameStateType State, StateCallback Callback) {
    m_ExitCallbacks[State].push_back(Callback);
}

void GameState::RegisterStateUpdateCallback(GameStateType State, StateCallback Callback) {
    m_UpdateCallbacks[State].push_back(Callback);
}

void GameState::Update(float DeltaTime) {
    OnStateUpdate(m_CurrentState, DeltaTime);
}

void GameState::SaveState(const std::string& FilePath) {
    // Placeholder - would serialize state data
    SIMPLE_LOG("GameState: Saving state to: " + FilePath);
}

void GameState::LoadState(const std::string& FilePath) {
    // Placeholder - would deserialize state data
    SIMPLE_LOG("GameState: Loading state from: " + FilePath);
}

void GameState::OnStateEnter(GameStateType State) {
    auto it = m_EnterCallbacks.find(State);
    if (it != m_EnterCallbacks.end()) {
        for (auto& callback : it->second) {
            if (callback) callback();
        }
    }
}

void GameState::OnStateExit(GameStateType State) {
    auto it = m_ExitCallbacks.find(State);
    if (it != m_ExitCallbacks.end()) {
        for (auto& callback : it->second) {
            if (callback) callback();
        }
    }
}

void GameState::OnStateUpdate(GameStateType State, float DeltaTime) {
    (void)DeltaTime;
    auto it = m_UpdateCallbacks.find(State);
    if (it != m_UpdateCallbacks.end()) {
        for (auto& callback : it->second) {
            if (callback) callback();
        }
    }
}

} // namespace Solstice::Game
