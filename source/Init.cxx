#include "Solstice.hxx"
#include "Core/Debug.hxx"
#include "Core/Async.hxx"
#include "Core/Audio.hxx"
#include "Core/Relic/Relic.hxx"
#include "UI/UISystem.hxx"
#include "Physics/PhysicsSystem.hxx"
#include "Scripting/Compiler.hxx"
#include "Scripting/BytecodeVM.hxx"
#include <cstring>
#include <sstream>
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

// Extern "C" wrappers for DLL loading (to avoid C++ name mangling)
extern "C" {
    SOLSTICE_API bool Initialize() {
        return Solstice::Initialize();
    }

    SOLSTICE_API bool Reinitialize() {
        return Solstice::Reinitialize();
    }

    SOLSTICE_API void Shutdown() {
        Solstice::Shutdown();
    }

    // Compile Moonwalk script source to bytecode
    // Returns: 0 on success, non-zero on error
    // Error message is written to errorBuffer (max errorBufferSize chars)
    SOLSTICE_API int Compile(const char* source, char* errorBuffer, size_t errorBufferSize) {
        try {
            Solstice::Scripting::Compiler compiler;
            auto program = compiler.Compile(std::string(source));
            (void)program; // Program compiled successfully
            return 0; // Success
        } catch (const std::exception& e) {
            if (errorBuffer && errorBufferSize > 0) {
                size_t len = std::min(errorBufferSize - 1, strlen(e.what()));
#ifdef _WIN32
                strncpy_s(errorBuffer, errorBufferSize, e.what(), len);
#else
                strncpy(errorBuffer, e.what(), len);
#endif
                errorBuffer[len] = '\0';
            }
            return 1; // Error
        } catch (...) {
            if (errorBuffer && errorBufferSize > 0) {
                const char* msg = "Unknown compilation error";
                size_t len = std::min(errorBufferSize - 1, strlen(msg));
#ifdef _WIN32
                strncpy_s(errorBuffer, errorBufferSize, msg, len);
#else
                strncpy(errorBuffer, msg, len);
#endif
                errorBuffer[len] = '\0';
            }
            return 1; // Error
        }
    }

    // Execute Moonwalk script (compile and run)
    // Returns: 0 on success, non-zero on error
    // Output is written to outputBuffer (max outputBufferSize chars)
    // Error message is written to errorBuffer (max errorBufferSize chars)
    SOLSTICE_API int Execute(const char* source, char* outputBuffer, size_t outputBufferSize, char* errorBuffer, size_t errorBufferSize) {
        try {
            Solstice::Scripting::Compiler compiler;
            auto program = compiler.Compile(std::string(source));

            // Create VM with basic print function (no need for full bindings for simple execution)
            Solstice::Scripting::BytecodeVM vm;

            // Register print function to capture output
            std::ostringstream outputStream;
            vm.RegisterNative("print", [&outputStream](const std::vector<Solstice::Scripting::Value>& args) -> Solstice::Scripting::Value {
                bool first = true;
                for (const auto& arg : args) {
                    if (!first) outputStream << " ";
                    first = false;

                    if (std::holds_alternative<int64_t>(arg)) {
                        outputStream << std::get<int64_t>(arg);
                    } else if (std::holds_alternative<double>(arg)) {
                        outputStream << std::get<double>(arg);
                    } else if (std::holds_alternative<std::string>(arg)) {
                        outputStream << std::get<std::string>(arg);
                    } else {
                        outputStream << "[value]";
                    }
                }
                outputStream << "\n";
                return (int64_t)0;
            });

            // Load and run program
            vm.LoadProgram(program);
            vm.Run();

            // Copy output to buffer
            std::string output = outputStream.str();
            if (outputBuffer && outputBufferSize > 0) {
                size_t len = std::min(outputBufferSize - 1, output.length());
#ifdef _WIN32
                strncpy_s(outputBuffer, outputBufferSize, output.c_str(), len);
#else
                strncpy(outputBuffer, output.c_str(), len);
#endif
                outputBuffer[len] = '\0';
            }

            return 0; // Success
        } catch (const std::exception& e) {
            if (errorBuffer && errorBufferSize > 0) {
                size_t len = std::min(errorBufferSize - 1, strlen(e.what()));
#ifdef _WIN32
                strncpy_s(errorBuffer, errorBufferSize, e.what(), len);
#else
                strncpy(errorBuffer, e.what(), len);
#endif
                errorBuffer[len] = '\0';
            }
            return 1; // Error
        } catch (...) {
            if (errorBuffer && errorBufferSize > 0) {
                const char* msg = "Unknown execution error";
                size_t len = std::min(errorBufferSize - 1, strlen(msg));
#ifdef _WIN32
                strncpy_s(errorBuffer, errorBufferSize, msg, len);
#else
                strncpy(errorBuffer, msg, len);
#endif
                errorBuffer[len] = '\0';
            }
            return 1; // Error
        }
    }
}
