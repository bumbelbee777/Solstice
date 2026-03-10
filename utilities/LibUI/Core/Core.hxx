#pragma once

#include <imgui.h>
#include <SDL3/SDL.h>
#include <mutex>
#include <atomic>

// LIBUI_API macro for DLL export/import
#if defined(_WIN32) || defined(_WIN64)
    #ifdef LIBUI_EXPORTS
        #define LIBUI_API __declspec(dllexport)
    #else
        #define LIBUI_API __declspec(dllimport)
    #endif
#else
    #define LIBUI_API
#endif

namespace LibUI::Core {

// Context manager for LibUI - singleton pattern
class LIBUI_API Context {
public:
    // Get singleton instance
    static Context& Instance();

    // Initialize LibUI with SDL window
    // Creates independent ImGui context separate from Solstice
    bool Initialize(SDL_Window* window);

    // Shutdown and cleanup
    void Shutdown();

    // Frame lifecycle
    void NewFrame();
    void Render();

    // Process SDL events for ImGui
    void ProcessEvent(const SDL_Event* event);

    // Get ImGui context
    ImGuiContext* GetContext() const { return m_Context; }

    // Check if initialized
    bool IsInitialized() const { return m_Initialized.load(std::memory_order_acquire); }

    // Disable copy/move
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    Context(Context&&) = delete;
    Context& operator=(Context&&) = delete;

private:
    Context() = default;
    ~Context() = default;

    ImGuiContext* m_Context{nullptr};
    std::atomic<bool> m_Initialized{false};
    std::mutex m_Mutex;
    SDL_Window* m_Window{nullptr};
};

// Convenience functions
LIBUI_API bool Initialize(SDL_Window* window);
LIBUI_API void Shutdown();
LIBUI_API void NewFrame();
LIBUI_API void Render();
LIBUI_API void ProcessEvent(const SDL_Event* event);
LIBUI_API ImGuiContext* GetContext();
LIBUI_API bool IsInitialized();

} // namespace LibUI::Core

