#include "LibUI/Core/Core.hxx"
#include "LibUI/Icons/Icons.hxx"
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>
#include <SDL3/SDL.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace LibUI::Core {

namespace {

constexpr int kRecentMax = 16;
static std::string s_ImgIniPath;
static std::vector<std::string> s_RecentPaths;

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
}

} // namespace

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
            SDL_free((void*)base);
        } else {
            s_ImgIniPath = "solstice_tools_imgui.ini";
        }
        ImGui::GetIO().IniFilename = s_ImgIniPath.data();
        RecentPathsLoad();

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

        if (const char* iconPath = std::getenv("SOLSTICE_ICON_FONT")) {
            if (Icons::TryLoadIconFontPackFromFile(iconPath, 15.f)) {
                Icons::RefreshFontGpuTexture();
            }
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

void RecentPathPush(const char* path) {
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
    return static_cast<int>(s_RecentPaths.size());
}

const char* RecentPathGet(int index) {
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

