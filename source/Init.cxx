#include "Solstice.hxx"
#include "Core/Debug.hxx"
#include "Core/Async.hxx"
#include "Core/Audio.hxx"
#include "UI/UISystem.hxx"
#include "Physics/PhysicsSystem.hxx"

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

        // 2. Initialize job system
        SIMPLE_LOG("Solstice: Initializing job system...");
        Core::JobSystem::Instance().Initialize();
        SIMPLE_LOG("Solstice: Job system initialized");

        // 3. Initialize audio
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

        // 2. Shutdown physics system (if running)
        // Note: We can't easily check if physics is running, so we'll just call Stop()
        // It's safe to call even if not started
        SIMPLE_LOG("Solstice: Shutting down physics system...");
        Physics::PhysicsSystem::Instance().Stop();
        SIMPLE_LOG("Solstice: Physics system shut down");

        // 3. Shutdown audio
        SIMPLE_LOG("Solstice: Shutting down audio system...");
        Core::Audio::AudioManager::Instance().Shutdown();
        SIMPLE_LOG("Solstice: Audio system shut down");

        // 4. Shutdown job system
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

}
