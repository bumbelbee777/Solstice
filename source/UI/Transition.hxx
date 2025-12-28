#pragma once

#include "../Solstice.hxx"
#include <UI/Animation.hxx>
#include <imgui.h>
#include <functional>
#include <memory>
#include <vector>

namespace Solstice::UI {

enum class TransitionState {
    Idle,
    Running,
    Paused,
    Completed
};

class SOLSTICE_API Transition {
public:
    Transition() = default;
    virtual ~Transition() = default;

    virtual void Start() {
        m_State = TransitionState::Running;
        m_Progress = 0.0f;
        if (m_OnStart) {
            m_OnStart();
        }
    }

    virtual void Update(float DeltaTime) {
        if (m_State != TransitionState::Running) {
            return;
        }

        m_Progress += DeltaTime / m_Duration;

        if (m_Progress >= 1.0f) {
            m_Progress = 1.0f;
            m_State = TransitionState::Completed;
            if (m_OnComplete) {
                m_OnComplete();
            }
        }
    }

    virtual void Pause() { m_State = TransitionState::Paused; }
    virtual void Resume() { m_State = TransitionState::Running; }
    virtual void Reset() {
        m_Progress = 0.0f;
        m_State = TransitionState::Idle;
    }

    TransitionState GetState() const { return m_State; }
    float GetProgress() const { return m_Progress; }
    bool IsCompleted() const { return m_State == TransitionState::Completed; }

    void SetDuration(float Duration) { m_Duration = Duration; }
    float GetDuration() const { return m_Duration; }

    void SetEasing(Animation::EasingType Easing) { m_Easing = Easing; }
    Animation::EasingType GetEasing() const { return m_Easing; }

    void SetOnStart(std::function<void()> Callback) { m_OnStart = Callback; }
    void SetOnComplete(std::function<void()> Callback) { m_OnComplete = Callback; }

protected:
    float m_Duration{1.0f};
    float m_Progress{0.0f};
    Animation::EasingType m_Easing{Animation::EasingType::EaseInOut};
    TransitionState m_State{TransitionState::Idle};
    std::function<void()> m_OnStart;
    std::function<void()> m_OnComplete;
};

class SOLSTICE_API FadeTransition : public Transition {
public:
    FadeTransition() = default;
    FadeTransition(float FromAlpha, float ToAlpha) : m_From(FromAlpha), m_To(ToAlpha) {}

    void Apply(float& Alpha) const {
        float easedProgress = Animation::Ease(m_Progress, m_Easing);
        Alpha = m_From + (m_To - m_From) * easedProgress;
    }

    void SetFrom(float From) { m_From = From; }
    void SetTo(float To) { m_To = To; }
    float GetFrom() const { return m_From; }
    float GetTo() const { return m_To; }

private:
    float m_From{0.0f};
    float m_To{1.0f};
};

class SOLSTICE_API SlideTransition : public Transition {
public:
    SlideTransition() = default;
    SlideTransition(const ImVec2& From, const ImVec2& To) : m_From(From), m_To(To) {}

    void Apply(ImVec2& Position) const {
        float easedProgress = Animation::Ease(m_Progress, m_Easing);
        Position.x = m_From.x + (m_To.x - m_From.x) * easedProgress;
        Position.y = m_From.y + (m_To.y - m_From.y) * easedProgress;
    }

    void SetFrom(const ImVec2& From) { m_From = From; }
    void SetTo(const ImVec2& To) { m_To = To; }
    ImVec2 GetFrom() const { return m_From; }
    ImVec2 GetTo() const { return m_To; }

private:
    ImVec2 m_From{0.0f, 0.0f};
    ImVec2 m_To{0.0f, 0.0f};
};

class SOLSTICE_API ScaleTransition : public Transition {
public:
    ScaleTransition() = default;
    ScaleTransition(const ImVec2& From, const ImVec2& To) : m_From(From), m_To(To) {}

    void Apply(ImVec2& Scale) const {
        float easedProgress = Animation::Ease(m_Progress, m_Easing);
        Scale.x = m_From.x + (m_To.x - m_From.x) * easedProgress;
        Scale.y = m_From.y + (m_To.y - m_From.y) * easedProgress;
    }

    void SetFrom(const ImVec2& From) { m_From = From; }
    void SetTo(const ImVec2& To) { m_To = To; }
    ImVec2 GetFrom() const { return m_From; }
    ImVec2 GetTo() const { return m_To; }

private:
    ImVec2 m_From{1.0f, 1.0f};
    ImVec2 m_To{1.0f, 1.0f};
};

class SOLSTICE_API RotateTransition : public Transition {
public:
    RotateTransition() = default;
    RotateTransition(float From, float To) : m_From(From), m_To(To) {}

    void Apply(float& Rotation) const {
        float easedProgress = Animation::Ease(m_Progress, m_Easing);
        Rotation = m_From + (m_To - m_From) * easedProgress;
    }

    void SetFrom(float From) { m_From = From; }
    void SetTo(float To) { m_To = To; }
    float GetFrom() const { return m_From; }
    float GetTo() const { return m_To; }

private:
    float m_From{0.0f};
    float m_To{0.0f};
};

class SOLSTICE_API CombinedTransition {
public:
    CombinedTransition() = default;
    ~CombinedTransition() = default;

    void AddTransition(std::shared_ptr<Transition> Transition) {
        m_Transitions.push_back(Transition);
    }

    void Start() {
        for (auto& transition : m_Transitions) {
            if (transition) {
                transition->Start();
            }
        }
    }

    void Update(float DeltaTime) {
        for (auto& transition : m_Transitions) {
            if (transition) {
                transition->Update(DeltaTime);
            }
        }
    }

    void Pause() {
        for (auto& transition : m_Transitions) {
            if (transition) {
                transition->Pause();
            }
        }
    }

    void Resume() {
        for (auto& transition : m_Transitions) {
            if (transition) {
                transition->Resume();
            }
        }
    }

    void Reset() {
        for (auto& transition : m_Transitions) {
            if (transition) {
                transition->Reset();
            }
        }
    }

    bool IsCompleted() const {
        for (const auto& transition : m_Transitions) {
            if (transition && !transition->IsCompleted()) {
                return false;
            }
        }
        return true;
    }

    size_t GetTransitionCount() const { return m_Transitions.size(); }
    std::shared_ptr<Transition> GetTransition(size_t Index) const {
        if (Index < m_Transitions.size()) {
            return m_Transitions[Index];
        }
        return nullptr;
    }

private:
    std::vector<std::shared_ptr<Transition>> m_Transitions;
};

} // namespace Solstice::UI
