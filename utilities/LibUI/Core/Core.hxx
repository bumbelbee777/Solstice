#pragma once

#include <imgui.h>
#include <SDL3/SDL.h>
#include <atomic>
#include <mutex>
#include <string>

// LIBUI_API: static LibUI (default) has no import/export; shared LibUI uses declspec when LIBUI_STATIC is unset.
#if defined(LIBUI_STATIC)
    #define LIBUI_API
#elif defined(_WIN32) || defined(_WIN64)
    #ifdef LIBUI_EXPORTS
        #define LIBUI_API __declspec(dllexport)
    #else
        #define LIBUI_API __declspec(dllimport)
    #endif
#else
    #define LIBUI_API
#endif

namespace LibUI::Core {

struct LIBUI_API RuntimeConfig {
    std::string AppDisplayName{"Solstice"};
    std::string StateDirectory{};
    std::string ImGuiIniFilename{"solstice_tools_imgui.ini"};
    std::string RecentPathsFilename{"solstice_tools_recent.txt"};
    std::string UiFontEnvVar{"SOLSTICE_UI_FONT"};
    std::string EnableIconFontEnvVar{"SOLSTICE_ENABLE_ICON_FONT"};
    std::string IconFontEnvVar{"SOLSTICE_ICON_FONT"};
    std::string DiagLogEnvVar{"SOLSTICE_DIAG_LOG"};
    std::string DiagLogFilename{"SolsticeTools_diag.txt"};
    std::string FullDumpEnvVar{"SOLSTICE_FULL_DUMP"};
    std::string CrashLogFilename{"SolsticeCrash.log"};
    std::string CrashDumpPrefix{"Solstice"};
};

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

    /// When SDL briefly reports `SDL_GetWindowSizeInPixels` as 0 during resize/maximize, the SDL3 ImGui backend can set
    /// `io.DisplayFramebufferScale` to 0 and ImGui draws nothing (black window). Call each frame **before** `NewFrame()`
    /// with the last known good drawable pixel size (e.g. from the previous frame); use 0,0 to disable.
    void SetFramebufferPixelFallback(int width, int height);

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
    std::atomic<int> m_FramebufferPixelFallbackW{0};
    std::atomic<int> m_FramebufferPixelFallbackH{0};
};

// Convenience functions
LIBUI_API bool Initialize(SDL_Window* window);
LIBUI_API void ConfigureRuntime(const RuntimeConfig& config);
LIBUI_API RuntimeConfig GetRuntimeConfig();
LIBUI_API void Shutdown();
LIBUI_API void NewFrame();
LIBUI_API void SetFramebufferPixelFallback(int width, int height);
LIBUI_API void Render();
LIBUI_API void ProcessEvent(const SDL_Event* event);
LIBUI_API ImGuiContext* GetContext();
LIBUI_API bool IsInitialized();

// ImGui state and recent-path storage use RuntimeConfig paths/filenames.
// Recent paths are newline-separated (max 16): index 0 is most recent.
LIBUI_API void RecentPathPush(const char* path);
LIBUI_API int RecentPathGetCount();
/** Valid until the next RecentPathPush or Shutdown; null if index out of range. */
LIBUI_API const char* RecentPathGet(int index);

/** ImGui frame without SDL backend (for offscreen / FBO export). Requires OpenGL3 backend initialized. */
LIBUI_API void NewFrameOffscreen(float width, float height, float deltaTime);
LIBUI_API void RenderOffscreen();

} // namespace LibUI::Core

