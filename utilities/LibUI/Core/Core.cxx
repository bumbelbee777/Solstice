#include "LibUI/Core/Core.hxx"
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>
#include <iostream>

namespace LibUI::Core {

Context& Context::Instance() {
    static Context instance;
    return instance;
}

bool Context::Initialize(SDL_Window* window) {
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (m_Initialized.load(std::memory_order_acquire)) {
        return true; // Already initialized
    }

    if (!window) {
        std::cerr << "LibUI::Core::Initialize: Invalid window pointer" << std::endl;
        return false;
    }

    try {
        // Verify OpenGL context is current
        SDL_GLContext glContext = SDL_GL_GetCurrentContext();
        if (!glContext) {
            std::cerr << "LibUI::Core::Initialize: ERROR - No OpenGL context is current. Make sure to call SDL_GL_MakeCurrent() before Initialize()" << std::endl;
            return false;
        }

        // Create independent ImGui context
        IMGUI_CHECKVERSION();
        m_Context = ImGui::CreateContext();
        ImGui::SetCurrentContext(m_Context);

        // Setup ImGui style
        ImGui::StyleColorsDark();

        // Setup platform/renderer bindings
        ImGui_ImplSDL3_InitForOpenGL(window, glContext);

        // Try to initialize OpenGL3 renderer
        const char* glsl_version = "#version 130";
        if (!ImGui_ImplOpenGL3_Init(glsl_version)) {
            std::cerr << "LibUI::Core::Initialize: ERROR - Failed to initialize OpenGL3 renderer" << std::endl;
            ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext(m_Context);
            m_Context = nullptr;
            return false;
        }

        m_Window = window;
        m_Initialized.store(true, std::memory_order_release);

        return true;
    } catch (const std::exception& e) {
        std::cerr << "LibUI::Core::Initialize: Exception: " << e.what() << std::endl;
        m_Initialized.store(false, std::memory_order_release);
        return false;
    } catch (...) {
        std::cerr << "LibUI::Core::Initialize: Unknown exception" << std::endl;
        m_Initialized.store(false, std::memory_order_release);
        return false;
    }
}

void Context::Shutdown() {
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (!m_Initialized.load(std::memory_order_acquire)) {
        return; // Not initialized
    }

    try {
        // Cleanup renderer
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();

        // Destroy context
        if (m_Context) {
            ImGui::DestroyContext(m_Context);
            m_Context = nullptr;
        }

        m_Window = nullptr;
        m_Initialized.store(false, std::memory_order_release);
    } catch (const std::exception& e) {
        std::cerr << "LibUI::Core::Shutdown: Exception: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "LibUI::Core::Shutdown: Unknown exception" << std::endl;
    }
}

void Context::NewFrame() {
    if (!m_Initialized.load(std::memory_order_acquire)) {
        return;
    }

    try {
        ImGui::SetCurrentContext(m_Context);
        ImGui_ImplSDL3_NewFrame();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui::NewFrame();
    } catch (const std::exception& e) {
        std::cerr << "LibUI::Core::NewFrame: Exception: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "LibUI::Core::NewFrame: Unknown exception" << std::endl;
    }
}

void Context::Render() {
    if (!m_Initialized.load(std::memory_order_acquire)) {
        return;
    }

    try {
        ImGui::SetCurrentContext(m_Context);
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    } catch (const std::exception& e) {
        std::cerr << "LibUI::Core::Render: Exception: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "LibUI::Core::Render: Unknown exception" << std::endl;
    }
}

void Context::ProcessEvent(const SDL_Event* event) {
    if (!m_Initialized.load(std::memory_order_acquire) || !event) {
        return;
    }

    try {
        ImGui::SetCurrentContext(m_Context);
        ImGui_ImplSDL3_ProcessEvent(event);
    } catch (const std::exception& e) {
        std::cerr << "LibUI::Core::ProcessEvent: Exception: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "LibUI::Core::ProcessEvent: Unknown exception" << std::endl;
    }
}

// Convenience functions
bool Initialize(SDL_Window* window) {
    return Context::Instance().Initialize(window);
}

void Shutdown() {
    Context::Instance().Shutdown();
}

void NewFrame() {
    Context::Instance().NewFrame();
}

void Render() {
    Context::Instance().Render();
}

void ProcessEvent(const SDL_Event* event) {
    Context::Instance().ProcessEvent(event);
}

ImGuiContext* GetContext() {
    return Context::Instance().GetContext();
}

bool IsInitialized() {
    return Context::Instance().IsInitialized();
}

} // namespace LibUI::Core

