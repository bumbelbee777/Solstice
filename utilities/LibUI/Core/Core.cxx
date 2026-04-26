#include "LibUI/Core/Core.hxx"
#include "LibUI/Icons/Icons.hxx"
#include "LibUI/Tools/DiagLog.hxx"
#include "LibUI/Tools/OpenGlDebug.hxx"
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>

#if defined(_WIN32)
#include <windows.h>
#endif

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

namespace LibUI::Core {

namespace {

constexpr int kRecentMax = 16;
static std::string s_ImgIniPath;
static std::vector<std::string> s_RecentPaths;
static std::once_flag s_recentPathsLoadedOnce;

static std::filesystem::path ToolsStateDir() {
    if (!s_ImgIniPath.empty()) {
        return std::filesystem::path(s_ImgIniPath).parent_path();
    }
    return std::filesystem::current_path();
}

static void RecentPathsSave() {
    const auto p = ToolsStateDir() / "solstice_tools_recent.txt";
    std::error_code ec;
    std::filesystem::create_directories(p.parent_path(), ec);
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    if (!out) {
        return;
    }
    for (const auto& line : s_RecentPaths) {
        out << line << '\n';
    }
}

static void RecentPathsLoad() {
    try {
        s_RecentPaths.clear();
        const auto p = ToolsStateDir() / "solstice_tools_recent.txt";
        std::ifstream in(p, std::ios::binary);
        if (!in) {
            return;
        }
        std::string line;
        while (std::getline(in, line) && static_cast<int>(s_RecentPaths.size()) < kRecentMax) {
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
                line.pop_back();
            }
            if (!line.empty()) {
                s_RecentPaths.push_back(std::move(line));
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "LibUI RecentPathsLoad filesystem_error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "LibUI RecentPathsLoad: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "LibUI RecentPathsLoad: unknown exception" << std::endl;
    }
}

/// Deferred from Initialize so a bad path / AV in filesystem code cannot abort startup before ImGui backends exist.
static void RecentPathsEnsureLoaded() {
    std::call_once(s_recentPathsLoadedOnce, RecentPathsLoad);
}

/// Segoe UI on Windows when available; override with `SOLSTICE_UI_FONT` (path to .ttf). Called after ImGui + GL backend init.
static void ApplySolsticeUiFontAndStyle() {
    ImGuiIO& io = ImGui::GetIO();
#ifdef IMGUI_HAS_DOCK
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
#endif
    ImGuiStyle& st = ImGui::GetStyle();
    st.WindowRounding = 7.0f;
    st.ChildRounding = 6.0f;
    st.FrameRounding = 5.0f;
    st.PopupRounding = 6.0f;
    st.ScrollbarRounding = 8.0f;
    st.GrabRounding = 4.0f;
    st.TabRounding = 5.0f;
    st.WindowMenuButtonPosition = ImGuiDir_None;
    st.WindowPadding = ImVec2(10.0f, 10.0f);
    st.ItemSpacing = ImVec2(9.0f, 6.0f);
    st.ItemInnerSpacing = ImVec2(7.0f, 5.0f);

    if (const char* envFont = std::getenv("SOLSTICE_UI_FONT")) {
        if (ImFont* f = io.Fonts->AddFontFromFileTTF(envFont, 18.0f, nullptr, io.Fonts->GetGlyphRangesDefault())) {
            io.FontDefault = f;
        }
        return;
    }
    if (const char* base = SDL_GetBasePath()) {
        try {
            const std::filesystem::path cascadia = std::filesystem::path(base) / "fonts" / "CascadiaCode-Regular.ttf";
            if (std::filesystem::is_regular_file(cascadia)) {
                const std::string p = cascadia.string();
                if (ImFont* f = io.Fonts->AddFontFromFileTTF(p.c_str(), 17.0f, nullptr, io.Fonts->GetGlyphRangesDefault())) {
                    io.FontDefault = f;
                    return;
                }
            }
        } catch (...) {
        }
    }
#if defined(_WIN32)
    char windir[MAX_PATH]{};
    if (GetEnvironmentVariableA("WINDIR", windir, MAX_PATH) > 0) {
        const std::string path = std::string(windir) + "\\Fonts\\segoeui.ttf";
        if (ImFont* f = io.Fonts->AddFontFromFileTTF(path.c_str(), 17.0f, nullptr, io.Fonts->GetGlyphRangesDefault())) {
            io.FontDefault = f;
        }
    }
#endif
}

} // namespace

/// When `SDL_GetWindowSizeInPixels` is 0 but logical size is valid, imgui_impl_sdl3 sets `FramebufferScale` to 0; ImGui's
/// OpenGL backend then skips all draws (black window). Patch `io` before `ImGui_ImplOpenGL3_NewFrame` / `ImGui::NewFrame`.
static void ApplyImGuiFramebufferPixelFallback(SDL_Window* window, int fallbackPixelW, int fallbackPixelH) {
    if (!window || fallbackPixelW <= 0 || fallbackPixelH <= 0) {
        return;
    }
    int lw = 0;
    int lh = 0;
    SDL_GetWindowSize(window, &lw, &lh);
    if (lw <= 0 || lh <= 0) {
        return;
    }
    int pw = 0;
    int ph = 0;
    SDL_GetWindowSizeInPixels(window, &pw, &ph);
    if (pw > 0 && ph > 0) {
        return;
    }
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(lw), static_cast<float>(lh));
    io.DisplayFramebufferScale = ImVec2(static_cast<float>(fallbackPixelW) / static_cast<float>(lw),
        static_cast<float>(fallbackPixelH) / static_cast<float>(lh));
}

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

        const char* base = SDL_GetBasePath();
        if (base) {
            s_ImgIniPath = std::string(base) + "solstice_tools_imgui.ini";
            // SDL3: SDL_GetBasePath() returns a cached internal pointer; do not SDL_free it (SDL_filesystem.c CachedBasePath).
        } else {
            s_ImgIniPath = "solstice_tools_imgui.ini";
        }
        ImGui::SetCurrentContext(m_Context);
        ImGui::GetIO().IniFilename = s_ImgIniPath.c_str();

        // Setup ImGui style
        ImGui::StyleColorsDark();

        // Setup platform/renderer bindings
        ImGui_ImplSDL3_InitForOpenGL(window, glContext);

        // GLSL must match the context. OpenGL 3.3+ core typically requires #version 330; #version 130 is for older
        // contexts and often fails to compile on 3.3 core (black window, failed ImGui shaders).
        const char* glsl_version = "#version 130";
        GLint glMajor = 0;
        GLint glMinor = 0;
#if defined(__APPLE__)
        glsl_version = "#version 150";
#else
        glGetIntegerv(GL_MAJOR_VERSION, &glMajor);
        glGetIntegerv(GL_MINOR_VERSION, &glMinor);
        if (glMajor > 3 || (glMajor == 3 && glMinor >= 3)) {
            glsl_version = "#version 330";
        } else if (glMajor == 3 && glMinor >= 2) {
            glsl_version = "#version 150";
        }
#endif

        // Try to initialize OpenGL3 renderer
        if (!ImGui_ImplOpenGL3_Init(glsl_version)) {
            std::cerr << "LibUI::Core::Initialize: ERROR - Failed to initialize OpenGL3 renderer" << std::endl;
            ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext(m_Context);
            m_Context = nullptr;
            return false;
        }

        ApplySolsticeUiFontAndStyle();

        LibUI::Tools::GlLogDriverInfo();
        LibUI::Tools::GlFlushErrors("after ImGui_ImplOpenGL3_Init");
        if (LibUI::Tools::GlDebugContextRequested()) {
            LibUI::Tools::GlTryInstallDebugMessenger();
        }

        if (LibUI::Tools::EnvVarTruthy("SOLSTICE_ENABLE_ICON_FONT")) {
            bool iconLoaded = false;
            if (const char* iconEnv = std::getenv("SOLSTICE_ICON_FONT")) {
                if (Icons::TryLoadIconFontPackFromFile(iconEnv, 16.f)) {
                    iconLoaded = true;
                }
            }
            if (!iconLoaded && base) {
                try {
                    const std::filesystem::path phosphor = std::filesystem::path(base) / "fonts" / "Phosphor.ttf";
                    if (std::filesystem::is_regular_file(phosphor)
                        && Icons::TryLoadIconFontPackFromFile(phosphor.string().c_str(), 16.f)) {
                        iconLoaded = true;
                    }
                } catch (...) {
                }
            }
            if (iconLoaded) {
                Icons::RefreshFontGpuTexture();
            }
        }
        LibUI::Tools::DiagLogLine("[LibUI] Core initialized.");

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

void Context::SetFramebufferPixelFallback(int width, int height) {
    m_FramebufferPixelFallbackW.store(width, std::memory_order_relaxed);
    m_FramebufferPixelFallbackH.store(height, std::memory_order_relaxed);
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
        s_RecentPaths.clear();
        s_ImgIniPath.clear();
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
        ApplyImGuiFramebufferPixelFallback(m_Window, m_FramebufferPixelFallbackW.load(std::memory_order_relaxed),
            m_FramebufferPixelFallbackH.load(std::memory_order_relaxed));
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
        LibUI::Tools::GlMaybeFlushErrorsAfterFrame("LibUI::Core::Render");
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

void SetFramebufferPixelFallback(int width, int height) {
    Context::Instance().SetFramebufferPixelFallback(width, height);
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

void RecentPathPush(const char* path) {
    RecentPathsEnsureLoaded();
    if (!path || !path[0]) {
        return;
    }
    std::string p(path);
    while (!p.empty() && (p.back() == '/' || p.back() == '\\')) {
        p.pop_back();
    }
    if (p.empty()) {
        return;
    }
    auto it = std::find(s_RecentPaths.begin(), s_RecentPaths.end(), p);
    if (it != s_RecentPaths.end()) {
        s_RecentPaths.erase(it);
    }
    s_RecentPaths.insert(s_RecentPaths.begin(), std::move(p));
    if (static_cast<int>(s_RecentPaths.size()) > kRecentMax) {
        s_RecentPaths.resize(static_cast<size_t>(kRecentMax));
    }
    RecentPathsSave();
}

int RecentPathGetCount() {
    RecentPathsEnsureLoaded();
    return static_cast<int>(s_RecentPaths.size());
}

const char* RecentPathGet(int index) {
    RecentPathsEnsureLoaded();
    if (index < 0 || index >= static_cast<int>(s_RecentPaths.size())) {
        return nullptr;
    }
    return s_RecentPaths[static_cast<size_t>(index)].c_str();
}

void NewFrameOffscreen(float width, float height, float deltaTime) {
    if (!Context::Instance().IsInitialized()) {
        return;
    }
    try {
        ImGui::SetCurrentContext(Context::Instance().GetContext());
        ImGui_ImplOpenGL3_NewFrame();
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(width, height);
        io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
        io.DeltaTime = deltaTime > 0.0f ? deltaTime : (1.0f / 60.0f);
        ImGui::NewFrame();
    } catch (const std::exception& e) {
        std::cerr << "LibUI::Core::NewFrameOffscreen: Exception: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "LibUI::Core::NewFrameOffscreen: Unknown exception" << std::endl;
    }
}

void RenderOffscreen() {
    if (!Context::Instance().IsInitialized()) {
        return;
    }
    try {
        ImGui::SetCurrentContext(Context::Instance().GetContext());
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    } catch (const std::exception& e) {
        std::cerr << "LibUI::Core::RenderOffscreen: Exception: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "LibUI::Core::RenderOffscreen: Unknown exception" << std::endl;
    }
}

} // namespace LibUI::Core

