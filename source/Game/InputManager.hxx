#pragma once

#include "../Solstice.hxx"
#include "../UI/Window.hxx"
#include <unordered_map>
#include <string>
#include <functional>
#include <vector>

namespace Solstice::Game {

// Input action types
enum class InputAction {
    MoveForward,
    MoveBackward,
    MoveLeft,
    MoveRight,
    Jump,
    Crouch,
    Sprint,
    Interact,
    Attack,
    Reload,
    Use,
    Inventory,
    Pause,
    COUNT
};

// Input state
struct InputState {
    bool IsPressed{false};
    bool WasPressed{false};
    bool JustPressed{false};
    bool JustReleased{false};
};

class SOLSTICE_API InputManager {
public:
    InputManager();
    ~InputManager() = default;

    // Update input state (call once per frame)
    void Update(UI::Window* Window);

    // Action mapping
    void BindAction(InputAction Action, int Scancode);
    void UnbindAction(InputAction Action);
    int GetActionScancode(InputAction Action) const;

    // Action queries
    bool IsActionPressed(InputAction Action) const;
    bool IsActionJustPressed(InputAction Action) const;
    bool IsActionJustReleased(InputAction Action) const;

    // Direct key queries
    bool IsKeyPressed(int Scancode) const;
    bool IsKeyJustPressed(int Scancode) const;
    bool IsKeyJustReleased(int Scancode) const;

    // Mouse state
    void SetMousePosition(float X, float Y);
    void UpdateMouseDelta(float DeltaX, float DeltaY);
    std::pair<float, float> GetMousePosition() const { return {m_MouseX, m_MouseY}; }
    std::pair<float, float> GetMouseDelta() const { return {m_MouseDeltaX, m_MouseDeltaY}; }
    bool IsMouseButtonPressed(int Button) const;
    bool IsMouseButtonJustPressed(int Button) const;
    bool IsMouseButtonJustReleased(int Button) const;

    // Callbacks
    using ActionCallback = std::function<void()>;
    void RegisterActionCallback(InputAction Action, ActionCallback Callback);
    void UnregisterActionCallback(InputAction Action);

private:
    // Action to scancode mapping
    std::unordered_map<InputAction, int> m_ActionMap;

    // Key state tracking
    std::unordered_map<int, InputState> m_KeyStates;
    std::unordered_map<int, InputState> m_PrevKeyStates;

    // Mouse state
    float m_MouseX{0.0f};
    float m_MouseY{0.0f};
    float m_MouseDeltaX{0.0f};
    float m_MouseDeltaY{0.0f};
    std::unordered_map<int, InputState> m_MouseButtonStates;
    std::unordered_map<int, InputState> m_PrevMouseButtonStates;

    // Action callbacks
    std::unordered_map<InputAction, std::vector<ActionCallback>> m_ActionCallbacks;

    // Helper to update state
    void UpdateState(InputState& State, bool IsPressed);
    void UpdateKeyStates(UI::Window* Window);
    void UpdateMouseStates(UI::Window* Window);
};

} // namespace Solstice::Game
