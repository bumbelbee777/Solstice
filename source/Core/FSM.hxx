#pragma once

#include "../Solstice.hxx"
#include <functional>
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>

namespace Solstice::Core {

// Forward declarations
class State;
class StateMachine;

// State transition with condition function
struct Transition {
    std::string FromState;
    std::string ToState;
    std::function<bool()> Condition;
    std::string EventName; // Optional event name

    Transition(const std::string& From, const std::string& To,
               std::function<bool()> Cond = []() { return true; },
               const std::string& Event = "")
        : FromState(From), ToState(To), Condition(std::move(Cond)), EventName(Event) {}
};

// Base state class
class State {
public:
    State(const std::string& Name) : m_Name(Name) {}
    virtual ~State() = default;

    virtual void OnEnter() {}
    virtual void OnExit() {}
    virtual void OnUpdate(float DeltaTime) {}

    std::string GetName() const { return m_Name; }

protected:
    std::string m_Name;
};

// Simple state machine
class SOLSTICE_API StateMachine {
public:
    StateMachine() = default;

    void AddState(std::shared_ptr<State> StatePtr) {
        m_States[StatePtr->GetName()] = StatePtr;
    }

    void AddTransition(const Transition& Trans) {
        m_Transitions.push_back(Trans);
    }

    void SetInitialState(const std::string& StateName) {
        if (m_States.find(StateName) != m_States.end()) {
            m_CurrentStateName = StateName;
            if (m_CurrentState) {
                m_CurrentState->OnExit();
            }
        m_CurrentState = m_States[StateName];
        if (m_CurrentState) {
            m_CurrentState->OnEnter();
        }
        }
    }

    void Update(float DeltaTime) {
        if (!m_CurrentState) return;

        // Check for transitions
        for (const auto& trans : m_Transitions) {
            if (trans.FromState == m_CurrentStateName && trans.Condition()) {
                TransitionTo(trans.ToState);
                break;
            }
        }

        // Update current state
        if (m_CurrentState) {
            m_CurrentState->OnUpdate(DeltaTime);
        }
    }

    void TriggerEvent(const std::string& EventName) {
        if (!m_CurrentState) return;

        for (const auto& trans : m_Transitions) {
            if (trans.FromState == m_CurrentStateName &&
                trans.EventName == EventName &&
                trans.Condition()) {
                TransitionTo(trans.ToState);
                break;
            }
        }
    }

    std::string GetCurrentStateName() const {
        return m_CurrentStateName;
    }

    std::shared_ptr<State> GetCurrentState() const {
        return m_CurrentState;
    }

private:
    void TransitionTo(const std::string& NewStateName) {
        if (m_States.find(NewStateName) == m_States.end()) {
            return;
        }

        if (m_CurrentState) {
            m_CurrentState->OnExit();
        }

        m_CurrentStateName = NewStateName;
        m_CurrentState = m_States[NewStateName];
        if (m_CurrentState) {
            m_CurrentState->OnEnter();
        }
    }

    std::unordered_map<std::string, std::shared_ptr<State>> m_States;
    std::vector<Transition> m_Transitions;
    std::shared_ptr<State> m_CurrentState;
    std::string m_CurrentStateName;
};

// Hierarchical state (can contain substates)
class HierarchicalState : public State {
public:
    HierarchicalState(const std::string& Name) : State(Name) {}

    void AddSubstate(std::shared_ptr<State> Substate) {
        m_Substates[Substate->GetName()] = Substate;
    }

    void SetInitialSubstate(const std::string& SubstateName) {
        if (m_Substates.find(SubstateName) != m_Substates.end()) {
            m_CurrentSubstateName = SubstateName;
            m_CurrentSubstate = m_Substates[SubstateName];
            m_CurrentSubstate->OnEnter();
        }
    }

    void OnEnter() override {
        State::OnEnter();
        if (m_CurrentSubstate) {
            m_CurrentSubstate->OnEnter();
        }
    }

    void OnExit() override {
        if (m_CurrentSubstate) {
            m_CurrentSubstate->OnExit();
        }
        State::OnExit();
    }

    void OnUpdate(float DeltaTime) override {
        State::OnUpdate(DeltaTime);
        if (m_CurrentSubstate) {
            m_CurrentSubstate->OnUpdate(DeltaTime);
        }
    }

    void TransitionSubstate(const std::string& NewSubstateName) {
        if (m_Substates.find(NewSubstateName) == m_Substates.end()) {
            return;
        }

        if (m_CurrentSubstate) {
            m_CurrentSubstate->OnExit();
        }

        m_CurrentSubstateName = NewSubstateName;
        m_CurrentSubstate = m_Substates[NewSubstateName];
        m_CurrentSubstate->OnEnter();
    }

    std::string GetCurrentSubstateName() const {
        return m_CurrentSubstateName;
    }

private:
    std::unordered_map<std::string, std::shared_ptr<State>> m_Substates;
    std::shared_ptr<State> m_CurrentSubstate;
    std::string m_CurrentSubstateName;
};

// Hierarchical state machine
class SOLSTICE_API HierarchicalStateMachine {
public:
    HierarchicalStateMachine() = default;

    void AddState(std::shared_ptr<State> StatePtr) {
        m_States[StatePtr->GetName()] = StatePtr;
    }

    void AddHierarchicalState(std::shared_ptr<HierarchicalState> HState) {
        m_States[HState->GetName()] = HState;
        m_HierarchicalStates[HState->GetName()] = HState;
    }

    void AddTransition(const Transition& Trans) {
        m_Transitions.push_back(Trans);
    }

    void SetInitialState(const std::string& StateName) {
        if (m_States.find(StateName) != m_States.end()) {
            m_CurrentStateName = StateName;
            if (m_CurrentState) {
                m_CurrentState->OnExit();
            }
            m_CurrentState = m_States[StateName];
            m_CurrentState->OnEnter();
        }
    }

    void Update(float DeltaTime) {
        if (!m_CurrentState) return;

        // Check for transitions at current level
        for (const auto& trans : m_Transitions) {
            if (trans.FromState == m_CurrentStateName && trans.Condition()) {
                TransitionTo(trans.ToState);
                break;
            }
        }

        // Update current state (which may be hierarchical)
        if (m_CurrentState) {
            m_CurrentState->OnUpdate(DeltaTime);
        }
    }

    void TriggerEvent(const std::string& EventName) {
        if (!m_CurrentState) return;

        // First check if current state is hierarchical and can handle event
        auto hStateIt = m_HierarchicalStates.find(m_CurrentStateName);
        if (hStateIt != m_HierarchicalStates.end()) {
            // Could propagate to substate if needed
        }

        // Check transitions at current level
        for (const auto& trans : m_Transitions) {
            if (trans.FromState == m_CurrentStateName &&
                trans.EventName == EventName &&
                trans.Condition()) {
                TransitionTo(trans.ToState);
                break;
            }
        }
    }

    std::string GetCurrentStateName() const {
        return m_CurrentStateName;
    }

    std::shared_ptr<State> GetCurrentState() const {
        return m_CurrentState;
    }

    std::string GetCurrentSubstateName() const {
        auto hStateIt = m_HierarchicalStates.find(m_CurrentStateName);
        if (hStateIt != m_HierarchicalStates.end()) {
            return hStateIt->second->GetCurrentSubstateName();
        }
        return "";
    }

private:
    void TransitionTo(const std::string& NewStateName) {
        if (m_States.find(NewStateName) == m_States.end()) {
            return;
        }

        if (m_CurrentState) {
            m_CurrentState->OnExit();
        }

        m_CurrentStateName = NewStateName;
        m_CurrentState = m_States[NewStateName];
        m_CurrentState->OnEnter();
    }

    std::unordered_map<std::string, std::shared_ptr<State>> m_States;
    std::unordered_map<std::string, std::shared_ptr<HierarchicalState>> m_HierarchicalStates;
    std::vector<Transition> m_Transitions;
    std::shared_ptr<State> m_CurrentState;
    std::string m_CurrentStateName;
};

} // namespace Solstice::Core

