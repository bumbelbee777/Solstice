#pragma once

#include "../Solstice.hxx"
#include <string>
#include <functional>
#include <utility>
#include <memory>

// Forward declare SDL_Window
struct SDL_Window;

namespace Solstice::UI {

class SOLSTICE_API Window {
public:
    using ResizeCallback = std::function<void(int Width, int Height)>;
    using KeyCallback = std::function<void(int Key, int Scancode, int Action, int Mods)>;
    using MouseButtonCallback = std::function<void(int Button, int Action, int Mods)>;
    using CursorPosCallback = std::function<void(double X, double Y)>;
    using ScrollCallback = std::function<void(double XOffset, double YOffset)>;

    Window(int Width, int Height, const std::string& Title);
    ~Window();

    // Core window operations
    bool ShouldClose() const;
    void PollEvents();
    
    // Window state
    void SetTitle(const std::string& Title);
    void SetSize(int Width, int Height);
    void SetFullscreen(bool Fullscreen);
    void SetResizable(bool Resizable);
    void SetCursorGrab(bool Grab);
    void SetRelativeMouse(bool Enable);
    void ShowCursor(bool Show);
    void Close();

    // Getters
    std::pair<int, int> GetSize() const;
    std::pair<int, int> GetFramebufferSize() const;
    std::string GetTitle() const;
    bool IsFullscreen() const;
    bool IsMinimized() const;
    SDL_Window* NativeWindow() const { return m_Window; }
    
    // Input polling
    bool IsKeyScanPressed(int Scancode) const;
    
    // Callback setters
    void SetResizeCallback(ResizeCallback Callback);
    void SetKeyCallback(KeyCallback Callback);
    void SetMouseButtonCallback(MouseButtonCallback Callback);
    void SetCursorPosCallback(CursorPosCallback Callback);
    void SetScrollCallback(ScrollCallback Callback);
    
private:
    SDL_Window* m_Window{nullptr};
    
    int m_Width;
    int m_Height;
    std::string m_Title;
    bool m_Fullscreen{false};
    bool m_ShouldClose{false};
    bool m_RelativeMouse{false};
    
    // Windowed mode state (for toggling fullscreen)
    struct {
        int X, Y;
        int Width, Height;
    } m_WindowedState;
    
    // Callbacks
    ResizeCallback m_ResizeCallback;
    KeyCallback m_KeyCallback;
    MouseButtonCallback m_MouseButtonCallback;
    CursorPosCallback m_CursorPosCallback;
    ScrollCallback m_ScrollCallback;
};

} // namespace Solstice::UI
