#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace {

struct SmokeCallback : public bgfx::CallbackI {
    void fatal(const char*, uint16_t, bgfx::Fatal::Enum, const char* msg) override {
        std::fprintf(stderr, "[GpuSmokeTest] bgfx fatal: %s\n", msg ? msg : "");
    }
    void traceVargs(const char*, uint16_t, const char*, va_list) override {}
    void profilerBegin(const char*, uint32_t, const char*, uint16_t) override {}
    void profilerBeginLiteral(const char*, uint32_t, const char*, uint16_t) override {}
    void profilerEnd() override {}
    uint32_t cacheReadSize(uint64_t) override { return 0; }
    bool cacheRead(uint64_t, void*, uint32_t) override { return false; }
    void cacheWrite(uint64_t, const void*, uint32_t) override {}
    void screenShot(const char*, uint32_t, uint32_t, uint32_t, bgfx::TextureFormat::Enum, const void*, uint32_t, bool) override {}
    void captureBegin(uint32_t, uint32_t, uint32_t, bgfx::TextureFormat::Enum, bool) override {}
    void captureEnd() override {}
    void captureFrame(const void*, uint32_t) override {}
};

bool EnvEnabled() {
    const char* v = std::getenv("SOLSTICE_GPU_TEST");
    return v && std::strcmp(v, "1") == 0;
}

} // namespace

int main() {
    if (!EnvEnabled()) {
        std::printf("[GpuSmokeTest] SKIP (set SOLSTICE_GPU_TEST=1 to run)\n");
        return 0;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "[GpuSmokeTest] SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Solstice GpuSmoke", 64, 64, SDL_WINDOW_HIDDEN);
    if (!window) {
        std::fprintf(stderr, "[GpuSmokeTest] SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    bgfx::PlatformData pd{};
#if defined(_WIN32)
    pd.nwh = SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#elif defined(__APPLE__)
    pd.nwh = SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
#elif defined(__ANDROID__)
    pd.nwh = SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER, nullptr);
#elif defined(__EMSCRIPTEN__)
    pd.nwh = (void*)"#canvas";
#elif defined(__linux__)
    pd.ndt = SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
    pd.nwh = (void*)(uintptr_t)SDL_GetNumberProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    if (!pd.nwh) {
        pd.ndt = SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
        pd.nwh = SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
    }
#endif

    if (!pd.nwh) {
        std::fprintf(stderr, "[GpuSmokeTest] No native window handle from SDL\n");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    static SmokeCallback callback;
    std::vector<bgfx::RendererType::Enum> renderersToTry;
#if defined(__EMSCRIPTEN__)
    renderersToTry.push_back(bgfx::RendererType::WebGPU);
    renderersToTry.push_back(bgfx::RendererType::OpenGLES);
#elif defined(_WIN32)
    renderersToTry.push_back(bgfx::RendererType::Direct3D11);
    renderersToTry.push_back(bgfx::RendererType::Direct3D12);
    renderersToTry.push_back(bgfx::RendererType::Vulkan);
    renderersToTry.push_back(bgfx::RendererType::OpenGL);
#elif defined(__APPLE__)
    renderersToTry.push_back(bgfx::RendererType::Metal);
    renderersToTry.push_back(bgfx::RendererType::OpenGL);
#else
    renderersToTry.push_back(bgfx::RendererType::Vulkan);
    renderersToTry.push_back(bgfx::RendererType::OpenGL);
    renderersToTry.push_back(bgfx::RendererType::OpenGLES);
#endif

    bool ok = false;
    for (auto rt : renderersToTry) {
        bgfx::Init init{};
        init.type = rt;
        init.vendorId = BGFX_PCI_ID_NONE;
        init.resolution.width = 64;
        init.resolution.height = 64;
        init.resolution.reset = 0;
        init.callback = &callback;
        init.platformData = pd;
        if (bgfx::init(init)) {
            ok = true;
            break;
        }
    }

    if (!ok) {
        std::fprintf(stderr, "[GpuSmokeTest] bgfx::init failed for all renderers\n");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    bgfx::setViewRect(0, 0, 0, 64, 64);
    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);
    bgfx::touch(0);
    bgfx::frame();
    bgfx::shutdown();

    SDL_DestroyWindow(window);
    SDL_Quit();
    std::printf("[GpuSmokeTest] PASS\n");
    return 0;
}
