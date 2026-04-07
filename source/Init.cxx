#include "Solstice.hxx"
#include "Core/Debug.hxx"
#include "Core/Async.hxx"
#include "Core/Audio.hxx"
#include "Core/Relic/Relic.hxx"
#include "UI/UISystem.hxx"
#include "Physics/PhysicsSystem.hxx"
#include "SolsticeAPI/V1/Core.h"
#include "SolsticeAPI/V1/Scripting.h"
#include <cstring>
#include <filesystem>

namespace Solstice {

std::atomic<bool> Initialized = false;

SOLSTICE_API bool Initialize() {
    if (Initialized.load(std::memory_order_acquire)) {
        SIMPLE_LOG("Solstice: Already initialized, skipping");
        return true;
    }

    try {
        SIMPLE_LOG("Solstice: Starting initialization...");

        // 1. Initialize logging first (if not already done)
        Core::DebugLogger::Initialize();

        // 2. RELIC bootstrap (game.data.relic): parse and build virtual table before any asset load
        {
            std::filesystem::path basePath = std::filesystem::current_path();
            if (Core::Relic::Initialize(basePath)) {
                SIMPLE_LOG("Solstice: RELIC initialized from " + basePath.string());
            }
        }

        // 3. Initialize job system
        SIMPLE_LOG("Solstice: Initializing job system...");
        Core::JobSystem::Instance().Initialize();
        SIMPLE_LOG("Solstice: Job system initialized");

        // 4. Initialize audio
        SIMPLE_LOG("Solstice: Initializing audio system...");
        Core::Audio::AudioManager::Instance().Initialize();
        SIMPLE_LOG("Solstice: Audio system initialized");

        // Mark as initialized
        Initialized.store(true, std::memory_order_release);
        SIMPLE_LOG("Solstice: Initialization complete");

        return true;
    } catch (const std::exception& e) {
        SIMPLE_LOG("Solstice: Initialization failed: " + std::string(e.what()));
        Initialized.store(false, std::memory_order_release);
        return false;
    }
}

SOLSTICE_API bool Reinitialize() {
    SIMPLE_LOG("Solstice: Reinitializing...");

    // Shutdown first
    Shutdown();

    // Then initialize again
    return Initialize();
}

SOLSTICE_API void Shutdown() {
    if (!Initialized.load(std::memory_order_acquire)) {
        SIMPLE_LOG("Solstice: Not initialized, skipping shutdown");
        return;
    }

    try {
        SIMPLE_LOG("Solstice: Starting shutdown...");

        // Shutdown in reverse order of initialization
        // 1. Shutdown UI system (if initialized)
        if (UI::UISystem::Instance().IsInitialized()) {
            SIMPLE_LOG("Solstice: Shutting down UI system...");
            UI::UISystem::Instance().Shutdown();
            SIMPLE_LOG("Solstice: UI system shut down");
        }

        // 2. Shutdown RELIC
        Core::Relic::Shutdown();

        // 3. Shutdown physics system (if running)
        // Note: We can't easily check if physics is running, so we'll just call Stop()
        // It's safe to call even if not started
        SIMPLE_LOG("Solstice: Shutting down physics system...");
        Physics::PhysicsSystem::Instance().Stop();
        SIMPLE_LOG("Solstice: Physics system shut down");

        // 4. Shutdown audio
        SIMPLE_LOG("Solstice: Shutting down audio system...");
        Core::Audio::AudioManager::Instance().Shutdown();
        SIMPLE_LOG("Solstice: Audio system shut down");

        // 5. Shutdown job system
        SIMPLE_LOG("Solstice: Shutting down job system...");
        Core::JobSystem::Instance().Shutdown();
        SIMPLE_LOG("Solstice: Job system shut down");

        Initialized.store(false, std::memory_order_release);
        SIMPLE_LOG("Solstice: Shutdown complete");
    } catch (const std::exception& e) {
        SIMPLE_LOG("Solstice: Shutdown error: " + std::string(e.what()));
        Initialized.store(false, std::memory_order_release);
    }
}

} // namespace Solstice

#if defined(_MSC_VER)
#define SOLSTICE_LEGACY_C_DEPRECATED __declspec(deprecated("Use SolsticeV1_* C API"))
#elif defined(__GNUC__) || defined(__clang__)
#define SOLSTICE_LEGACY_C_DEPRECATED __attribute__((deprecated("Use SolsticeV1_* C API")))
#else
#define SOLSTICE_LEGACY_C_DEPRECATED
#endif

// Extern "C" wrappers for DLL loading (to avoid C++ name mangling); deprecated in favor of SolsticeV1_*.
extern "C" {
SOLSTICE_LEGACY_C_DEPRECATED
SOLSTICE_API bool Initialize() {
    return SolsticeV1_CoreInitialize() != SolsticeV1_False;
}

SOLSTICE_LEGACY_C_DEPRECATED
SOLSTICE_API bool Reinitialize() {
    return SolsticeV1_CoreReinitialize() != SolsticeV1_False;
}

SOLSTICE_LEGACY_C_DEPRECATED
SOLSTICE_API void Shutdown() {
    SolsticeV1_CoreShutdown();
}

SOLSTICE_LEGACY_C_DEPRECATED
SOLSTICE_API int Compile(const char* source, char* errorBuffer, size_t errorBufferSize) {
    return static_cast<int>(SolsticeV1_ScriptingCompile(source, errorBuffer, errorBufferSize));
}

SOLSTICE_LEGACY_C_DEPRECATED
SOLSTICE_API int Execute(const char* source, char* outputBuffer, size_t outputBufferSize, char* errorBuffer, size_t errorBufferSize) {
    return static_cast<int>(SolsticeV1_ScriptingExecute(source, outputBuffer, outputBufferSize, errorBuffer, errorBufferSize));
}
}
