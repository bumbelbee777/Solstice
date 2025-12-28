#pragma once

#include "../Solstice.hxx"
#include "../Core/FSM.hxx"
#include <string>
#include <functional>
#include <unordered_map>

namespace Solstice::Game {

// Game state types
enum class GameStateType {
    MainMenu,
    Playing,
    Paused,
    Inventory,
    Settings,
    GameOver,
    Victory,
    COUNT
};

class SOLSTICE_API GameState {
public:
    GameState();
    ~GameState() = default;

    // State management
    void SetState(GameStateType State);
    GameStateType GetCurrentState() const { return m_CurrentState; }

    // State transitions
    void RegisterTransition(GameStateType From, GameStateType To, std::function<bool()> Condition = nullptr);
    bool CanTransition(GameStateType To) const;
    bool TryTransition(GameStateType To);

    // State callbacks
    using StateCallback = std::function<void()>;
    void RegisterStateEnterCallback(GameStateType State, StateCallback Callback);
    void RegisterStateExitCallback(GameStateType State, StateCallback Callback);
    void RegisterStateUpdateCallback(GameStateType State, StateCallback Callback);

    // Update current state
    void Update(float DeltaTime);

    // State persistence
    void SaveState(const std::string& FilePath);
    void LoadState(const std::string& FilePath);

private:
    GameStateType m_CurrentState{GameStateType::MainMenu};

    // FSM for state management
    Core::StateMachine m_StateMachine;

    // State callbacks
    std::unordered_map<GameStateType, std::vector<StateCallback>> m_EnterCallbacks;
    std::unordered_map<GameStateType, std::vector<StateCallback>> m_ExitCallbacks;
    std::unordered_map<GameStateType, std::vector<StateCallback>> m_UpdateCallbacks;

    // Transition conditions
    std::unordered_map<GameStateType, std::unordered_map<GameStateType, std::function<bool()>>> m_Transitions;

    void OnStateEnter(GameStateType State);
    void OnStateExit(GameStateType State);
    void OnStateUpdate(GameStateType State, float DeltaTime);
};

} // namespace Solstice::Game
