#include "InputManager.hxx"
#include <algorithm>

namespace Solstice::Game {

InputManager::InputManager() {
    // Default key bindings (WASD layout)
    m_ActionMap[InputAction::MoveForward] = 26;  // W
    m_ActionMap[InputAction::MoveBackward] = 22;  // S
    m_ActionMap[InputAction::MoveLeft] = 4;       // A
    m_ActionMap[InputAction::MoveRight] = 7;      // D
    m_ActionMap[InputAction::Jump] = 44;          // Space
    m_ActionMap[InputAction::Crouch] = 225;       // Left Shift
    m_ActionMap[InputAction::Sprint] = 42;        // Left Shift (alternative)
    m_ActionMap[InputAction::Interact] = 69;      // E
    m_ActionMap[InputAction::Attack] = 1;          // Left Mouse Button
    m_ActionMap[InputAction::Reload] = 82;        // R
    m_ActionMap[InputAction::Use] = 70;            // F
    m_ActionMap[InputAction::Inventory] = 73;      // I
    m_ActionMap[InputAction::Pause] = 27;         // ESC
}

void InputManager::Update(UI::Window* Window) {
    if (!Window) return;

    // Save previous states
    m_PrevKeyStates = m_KeyStates;
    m_PrevMouseButtonStates = m_MouseButtonStates;

    // Update key states
    UpdateKeyStates(Window);

    // Update mouse states
    UpdateMouseStates(Window);

    // Reset mouse delta (will be updated by cursor callback)
    m_MouseDeltaX = 0.0f;
    m_MouseDeltaY = 0.0f;

    // Trigger action callbacks for just-pressed actions
    for (auto& [action, callbacks] : m_ActionCallbacks) {
        if (IsActionJustPressed(action)) {
            for (auto& callback : callbacks) {
                if (callback) callback();
            }
        }
    }
}

void InputManager::BindAction(InputAction Action, int Scancode) {
    m_ActionMap[Action] = Scancode;
}

void InputManager::UnbindAction(InputAction Action) {
    m_ActionMap.erase(Action);
}

int InputManager::GetActionScancode(InputAction Action) const {
    auto it = m_ActionMap.find(Action);
    if (it != m_ActionMap.end()) {
        return it->second;
    }
    return -1;
}

bool InputManager::IsActionPressed(InputAction Action) const {
    auto it = m_ActionMap.find(Action);
    if (it != m_ActionMap.end()) {
        return IsKeyPressed(it->second);
    }
    return false;
}

bool InputManager::IsActionJustPressed(InputAction Action) const {
    auto it = m_ActionMap.find(Action);
    if (it != m_ActionMap.end()) {
        return IsKeyJustPressed(it->second);
    }
    return false;
}

bool InputManager::IsActionJustReleased(InputAction Action) const {
    auto it = m_ActionMap.find(Action);
    if (it != m_ActionMap.end()) {
        return IsKeyJustReleased(it->second);
    }
    return false;
}

bool InputManager::IsKeyPressed(int Scancode) const {
    auto it = m_KeyStates.find(Scancode);
    if (it != m_KeyStates.end()) {
        return it->second.IsPressed;
    }
    return false;
}

bool InputManager::IsKeyJustPressed(int Scancode) const {
    auto it = m_KeyStates.find(Scancode);
    auto prevIt = m_PrevKeyStates.find(Scancode);
    if (it != m_KeyStates.end()) {
        bool current = it->second.IsPressed;
        bool previous = (prevIt != m_PrevKeyStates.end()) ? prevIt->second.IsPressed : false;
        return current && !previous;
    }
    return false;
}

bool InputManager::IsKeyJustReleased(int Scancode) const {
    auto it = m_KeyStates.find(Scancode);
    auto prevIt = m_PrevKeyStates.find(Scancode);
    if (it != m_KeyStates.end()) {
        bool current = it->second.IsPressed;
        bool previous = (prevIt != m_PrevKeyStates.end()) ? prevIt->second.IsPressed : false;
        return !current && previous;
    }
    return false;
}

void InputManager::SetMousePosition(float X, float Y) {
    m_MouseX = X;
    m_MouseY = Y;
}

void InputManager::UpdateMouseDelta(float DeltaX, float DeltaY) {
    m_MouseDeltaX = DeltaX;
    m_MouseDeltaY = DeltaY;
}

bool InputManager::IsMouseButtonPressed(int Button) const {
    auto it = m_MouseButtonStates.find(Button);
    if (it != m_MouseButtonStates.end()) {
        return it->second.IsPressed;
    }
    return false;
}

bool InputManager::IsMouseButtonJustPressed(int Button) const {
    auto it = m_MouseButtonStates.find(Button);
    auto prevIt = m_PrevMouseButtonStates.find(Button);
    if (it != m_MouseButtonStates.end()) {
        bool current = it->second.IsPressed;
        bool previous = (prevIt != m_PrevMouseButtonStates.end()) ? prevIt->second.IsPressed : false;
        return current && !previous;
    }
    return false;
}

bool InputManager::IsMouseButtonJustReleased(int Button) const {
    auto it = m_MouseButtonStates.find(Button);
    auto prevIt = m_PrevMouseButtonStates.find(Button);
    if (it != m_MouseButtonStates.end()) {
        bool current = it->second.IsPressed;
        bool previous = (prevIt != m_PrevMouseButtonStates.end()) ? prevIt->second.IsPressed : false;
        return !current && previous;
    }
    return false;
}

void InputManager::RegisterActionCallback(InputAction Action, ActionCallback Callback) {
    m_ActionCallbacks[Action].push_back(Callback);
}

void InputManager::UnregisterActionCallback(InputAction Action) {
    m_ActionCallbacks.erase(Action);
}

void InputManager::UpdateState(InputState& State, bool IsPressed) {
    State.WasPressed = State.IsPressed;
    State.IsPressed = IsPressed;
    State.JustPressed = IsPressed && !State.WasPressed;
    State.JustReleased = !IsPressed && State.WasPressed;
}

void InputManager::UpdateKeyStates(UI::Window* Window) {
    // Update states for all tracked keys
    for (auto& [scancode, state] : m_KeyStates) {
        bool isPressed = Window->IsKeyScanPressed(scancode);
        UpdateState(state, isPressed);
    }

    // Also check action-mapped keys
    for (const auto& [action, scancode] : m_ActionMap) {
        if (m_KeyStates.find(scancode) == m_KeyStates.end()) {
            InputState newState;
            bool isPressed = Window->IsKeyScanPressed(scancode);
            UpdateState(newState, isPressed);
            m_KeyStates[scancode] = newState;
        }
    }
}

void InputManager::UpdateMouseStates(UI::Window* Window) {
    // Mouse button states are typically updated via callbacks
    // This is a placeholder - actual implementation would track mouse button states
    // For now, we'll rely on callbacks to update mouse button states
}

} // namespace Solstice::Game
