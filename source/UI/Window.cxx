#include <UI/Window.hxx>
#include <UI/UISystem.hxx>
#include <stdexcept>
#include <iostream>

// Include SDL3
#include <SDL3/SDL.h>

namespace Solstice::UI {

Window::Window(int Width, int Height, const std::string& Title)
    : m_Width(Width), m_Height(Height), m_Title(Title) {
    
    // Initialize SDL
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        throw std::runtime_error(std::string("Failed to initialize SDL: ") + SDL_GetError());
    }
    
    // Create window (without OpenGL)
    m_Window = SDL_CreateWindow(m_Title.c_str(), m_Width, m_Height, SDL_WINDOW_RESIZABLE);
    if (!m_Window) {
        SDL_Quit();
        throw std::runtime_error(std::string("Failed to create SDL window: ") + SDL_GetError());
    }
    
    // Store window position and size for later use
    SDL_GetWindowPosition(m_Window, &m_WindowedState.X, &m_WindowedState.Y);
    SDL_GetWindowSize(m_Window, &m_WindowedState.Width, &m_WindowedState.Height);
}

Window::~Window() {
    if (m_Window) {
        SDL_DestroyWindow(m_Window);
    }
    SDL_Quit();
}

bool Window::ShouldClose() const {
    return m_ShouldClose;
}

void Window::PollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        // Forward event to UISystem for ImGui processing first (only if initialized)
        if (UISystem::Instance().IsInitialized()) {
            UISystem::Instance().ProcessEvent(&event);
        }
        
        switch (event.type) {
            case SDL_EVENT_QUIT:
                m_ShouldClose = true;
                break;
            
            case SDL_EVENT_WINDOW_RESIZED:
                if (event.window.windowID == SDL_GetWindowID(m_Window)) {
                    m_Width = event.window.data1;
                    m_Height = event.window.data2;
                    if (m_ResizeCallback) {
                        m_ResizeCallback(m_Width, m_Height);
                    }
                }
                break;

            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
                if (event.key.windowID == SDL_GetWindowID(m_Window)) {
                    if (m_KeyCallback) {
                        // Map SDL key event to callback signature
                        // Note: SDL3 key codes are different from GLFW. 
                        // Passing raw SDL values for now.
                        int action = (event.type == SDL_EVENT_KEY_DOWN) ? 1 : 0; // 1 = Press, 0 = Release
                        m_KeyCallback(event.key.key, event.key.scancode, action, event.key.mod);
                    }
                }
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (event.button.windowID == SDL_GetWindowID(m_Window)) {
                    if (m_MouseButtonCallback) {
                        int action = (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) ? 1 : 0;
                        m_MouseButtonCallback(event.button.button, action, SDL_GetModState());
                    }
                }
                break;

            case SDL_EVENT_MOUSE_MOTION:
                if (event.motion.windowID == SDL_GetWindowID(m_Window)) {
                    if (m_CursorPosCallback) {
                        if (m_RelativeMouse) {
                            m_CursorPosCallback(event.motion.xrel, event.motion.yrel);
                        } else {
                            m_CursorPosCallback(event.motion.x, event.motion.y);
                        }
                    }
                }
                break;

            case SDL_EVENT_MOUSE_WHEEL:
                if (event.wheel.windowID == SDL_GetWindowID(m_Window)) {
                    if (m_ScrollCallback) {
                        m_ScrollCallback(event.wheel.x, event.wheel.y);
                    }
                }
                break;
        }
    }
}

// Window state management
void Window::SetTitle(const std::string& Title) {
    m_Title = Title;
    SDL_SetWindowTitle(m_Window, m_Title.c_str());
}

void Window::SetSize(int Width, int Height) {
    m_Width = Width;
    m_Height = Height;
    SDL_SetWindowSize(m_Window, m_Width, m_Height);
}

void Window::SetFullscreen(bool Fullscreen) {
    if (Fullscreen == m_Fullscreen) return;
    
    m_Fullscreen = Fullscreen;
    
    if (m_Fullscreen) {
        // Save current window position and size
        SDL_GetWindowPosition(m_Window, &m_WindowedState.X, &m_WindowedState.Y);
        SDL_GetWindowSize(m_Window, &m_WindowedState.Width, &m_WindowedState.Height);
        
        SDL_SetWindowFullscreen(m_Window, true);
    } else {
        SDL_SetWindowFullscreen(m_Window, false);
        // Restore windowed mode
        SDL_SetWindowSize(m_Window, m_WindowedState.Width, m_WindowedState.Height);
        SDL_SetWindowPosition(m_Window, m_WindowedState.X, m_WindowedState.Y);
    }
}

void Window::SetResizable(bool Resizable) {
    SDL_SetWindowResizable(m_Window, Resizable);
}

void Window::SetCursorGrab(bool Grab) {
    SDL_SetWindowMouseGrab(m_Window, Grab ? true : false);
}

void Window::SetRelativeMouse(bool Enable) {
    m_RelativeMouse = Enable;
    SDL_SetWindowRelativeMouseMode(m_Window, Enable ? true : false);
}

void Window::ShowCursor(bool Show) {
    if (Show) SDL_ShowCursor(); else SDL_HideCursor();
}

void Window::Close() {
    m_ShouldClose = true;
}

// Getters
std::pair<int, int> Window::GetSize() const {
    int width, height;
    SDL_GetWindowSize(m_Window, &width, &height);
    return {width, height};
}

std::pair<int, int> Window::GetFramebufferSize() const {
    int width, height;
    // In SDL3, GetWindowSizeInPixels gives framebuffer size
    SDL_GetWindowSizeInPixels(m_Window, &width, &height);
    return {width, height};
}

std::string Window::GetTitle() const {
    return m_Title;
}

bool Window::IsFullscreen() const {
    return m_Fullscreen;
}

bool Window::IsMinimized() const {
    return (SDL_GetWindowFlags(m_Window) & SDL_WINDOW_MINIMIZED) != 0;
}

// Callback setters
void Window::SetResizeCallback(ResizeCallback Callback) {
    m_ResizeCallback = std::move(Callback);
}

void Window::SetKeyCallback(KeyCallback Callback) {
    m_KeyCallback = std::move(Callback);
}

void Window::SetMouseButtonCallback(MouseButtonCallback Callback) {
    m_MouseButtonCallback = std::move(Callback);
}

void Window::SetCursorPosCallback(CursorPosCallback Callback) {
    m_CursorPosCallback = std::move(Callback);
}

void Window::SetScrollCallback(ScrollCallback Callback) {
    m_ScrollCallback = std::move(Callback);
}

// Input polling
bool Window::IsKeyScanPressed(int Scancode) const {
    int num = 0;
    const bool* State = SDL_GetKeyboardState(&num);
    if (!State) return false;
    if (Scancode < 0 || Scancode >= num) return false;
    return State[Scancode];
}

} // namespace Solstice::UI
